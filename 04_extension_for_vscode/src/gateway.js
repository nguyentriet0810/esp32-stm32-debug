/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** Initialize BMD before the native debug adapter connects, then relay RSP unchanged. */
const net = require('node:net');
const { validateSettings } = require('./config');
const { acquireProbeLock } = require('./probe-lock');
const { initializeProbe } = require('./rsp');

class ProbeGateway {
    constructor(settings, { beforeConnect = async () => {}, onOutput = () => {}, onFailure = () => {},
        startupTimeoutMs = 10 * 60 * 1000, acquireLock = acquireProbeLock, initialize = initializeProbe, reset = false } = {}) {
        this.config = validateSettings(settings);
        Object.assign(this, { beforeConnect, onOutput, onFailure, startupTimeoutMs, acquireLock, initialize, reset });
        this.abort = new AbortController();
        this.closed = false;
        this.ready = false;
    }
    async start() {
        this.server = net.createServer(socket => { void this.accept(socket); });
        this.server.on('error', error => { void this.fail(error); });
        await new Promise((resolve, reject) => {
            this.server.once('error', reject);
            this.server.listen(0, '127.0.0.1', () => { this.server.off('error', reject); resolve(); });
        });
        this.port = this.server.address().port;
        this.timer = setTimeout(() => { void this.fail(new Error('No debugger connected to the MAGIC gateway before the startup timeout.')); }, this.startupTimeoutMs);
        this.timer.unref?.();
        return this;
    }
    async accept(local) {
        if (this.local || this.closed) { local.destroy(); return; }
        this.local = local;
        clearTimeout(this.timer);
        local.pause();
        local.setNoDelay(true);
        local.on('error', error => { void this.fail(error); });
        local.on('close', () => { void this.close(); });
        this.initializing = true;
        try {
            this.release = await this.acquireLock(this.config);
            if (this.closed) { await this.release(); return; }
            // Native preLaunchTask / ST preBuild has finished when its GDB connects here.
            await this.beforeConnect(this.abort.signal);
            if (this.closed) { return; }
            this.remote = await this.initialize(this.config, { signal: this.abort.signal, onOutput: this.onOutput, reset: this.reset });
            if (this.closed) { this.remote.destroy(); return; }
            this.remote.on('error', error => { void this.fail(error); });
            this.remote.on('close', () => { if (!this.closed) { void this.fail(new Error('Wi-Fi probe connection closed. Start a new debug session after reconnecting.')); } });
            this.ready = true;
            local.pipe(this.remote); this.remote.pipe(local);
            this.remote.resume(); local.resume();
        } catch (error) { if (!this.closed) { await this.fail(error); } }
        finally {
            this.initializing = false;
            if (this.closed) { await this.release?.(); }
        }
    }
    async fail(error) {
        if (this.closed) { return; }
        this.onOutput(`\nMAGIC gateway: ${error.message}\n`);
        await this.close();
        this.onFailure(error);
    }
    async close() {
        if (this.closed) { return; }
        this.closed = true;
        this.ready = false;
        clearTimeout(this.timer);
        this.abort.abort();
        this.local?.destroy(); this.remote?.destroy();
        this.server?.close();
        if (!this.initializing) { await this.release?.(); }
    }
}

module.exports = { ProbeGateway };
