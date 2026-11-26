/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** Minimal acknowledged RSP client used only before handing the socket to GDB. */
const net = require('node:net');

function packet(payload) {
    const data = Buffer.from(payload, 'ascii');
    const checksum = data.reduce((sum, byte) => (sum + byte) & 255, 0);
    return Buffer.concat([Buffer.from('$'), data, Buffer.from(`#${checksum.toString(16).padStart(2, '0')}`)]);
}

class BootstrapClient {
    constructor(socket, { timeoutMs = 15000, onOutput = () => {} } = {}) {
        this.socket = socket;
        this.timeoutMs = timeoutMs;
        this.onOutput = onOutput;
        this.buffer = Buffer.alloc(0);
        this.pending = undefined;
        this.receive = chunk => this.consume(chunk);
        this.fail = error => this.reject(error instanceof Error ? error : new Error('Probe connection closed.'));
        socket.on('data', this.receive);
        socket.on('error', this.fail);
        socket.on('close', this.fail);
    }
    reject(error) { this.pending?.reject(error); }
    consume(chunk) {
        this.buffer = Buffer.concat([this.buffer, chunk]);
        if (this.buffer.length > 1024 * 1024) { this.reject(new Error('Probe RSP reply exceeded 1 MiB.')); return; }
        while (this.buffer.length) {
            if (this.buffer[0] === 43) { this.buffer = this.buffer.subarray(1); continue; }
            if (this.buffer[0] === 45) {
                this.buffer = this.buffer.subarray(1);
                if (this.pending && ++this.pending.retries <= 3) { this.socket.write(this.pending.packet); }
                else { this.reject(new Error('Probe rejected the RSP checksum repeatedly.')); }
                continue;
            }
            if (this.buffer[0] !== 36) { this.reject(new Error('Unexpected data during probe initialization.')); return; }
            const end = this.buffer.indexOf(35, 1);
            if (end < 0 || this.buffer.length < end + 3) { return; }
            const data = this.buffer.subarray(1, end);
            const expected = this.buffer.subarray(end + 1, end + 3).toString('ascii');
            this.buffer = this.buffer.subarray(end + 3);
            const checksum = data.reduce((sum, byte) => (sum + byte) & 255, 0).toString(16).padStart(2, '0');
            if (checksum !== expected.toLowerCase()) { this.socket.write('-'); continue; }
            this.socket.write('+');
            const reply = data.toString('ascii');
            if (/^O(?:[0-9a-fA-F]{2})*$/.test(reply)) {
                this.onOutput(Buffer.from(reply.slice(1), 'hex').toString('utf8'));
            } else if (this.pending) { this.pending.resolve(reply); }
        }
    }
    async request(command) {
        if (this.pending) { throw new Error('Concurrent bootstrap RSP requests are not supported.'); }
        if (this.socket.destroyed) { throw new Error('Probe connection is closed.'); }
        try {
            return await new Promise((resolve, reject) => {
                const timer = setTimeout(() => reject(new Error(`Probe initialization timed out: ${command.slice(0, 24)}`)), this.timeoutMs);
                this.pending = { packet: packet(command), retries: 0,
                    resolve: value => { clearTimeout(timer); resolve(value); },
                    reject: error => { clearTimeout(timer); reject(error); } };
                this.socket.write(this.pending.packet);
            });
        } finally { this.pending = undefined; }
    }
    async monitor(command) {
        const result = await this.request(`qRcmd,${Buffer.from(command).toString('hex')}`);
        if (result !== 'OK') { throw new Error(`Probe rejected '${command}': ${result || 'unsupported'}`); }
    }
    handoff() {
        if (this.pending || this.buffer.length) { throw new Error('Cannot hand off an incomplete probe RSP reply.'); }
        this.socket.pause();
        this.socket.off('data', this.receive);
        this.socket.off('error', this.fail);
        this.socket.off('close', this.fail);
    }
}

async function initializeProbe(config, { onOutput, signal, reset = false } = {}) {
    const socket = net.createConnection({ host: config.host, port: config.port });
    socket.setNoDelay(true);
    socket.setKeepAlive(true, 1000);
    const abort = () => socket.destroy(new Error('Probe initialization cancelled.'));
    signal?.addEventListener('abort', abort, { once: true });
    const client = new BootstrapClient(socket, { timeoutMs: config.remoteTimeoutSeconds * 1000, onOutput });
    try {
        if (signal?.aborted) { throw new Error('Probe initialization cancelled.'); }
        const connected = await client.request('!');
        if (connected !== 'OK') { throw new Error('Probe does not support extended remote debugging.'); }
        await client.monitor(`connect_rst ${config.connectUnderReset ? 'enable' : 'disable'}`);
        await client.monitor(`frequency ${config.clockHz}`);
        await client.monitor('swd_scan');
        const stopped = await client.request(`vAttach;${config.targetIndex.toString(16)}`);
        if (!/^[TS][0-9a-fA-F]{2}/.test(stopped)) { throw new Error(`Unable to attach SWD target ${config.targetIndex}: ${stopped}`); }
        if (reset) {
            const restarted = await client.request('vRun;');
            if (!/^[TS][0-9a-fA-F]{2}/.test(restarted)) { throw new Error(`Probe could not reset and halt the target: ${restarted}`); }
        }
        client.handoff();
        return socket;
    } catch (error) { socket.destroy(); throw error; }
    finally { signal?.removeEventListener('abort', abort); }
}

module.exports = { packet, BootstrapClient, initializeProbe };
