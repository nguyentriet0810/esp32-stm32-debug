/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const net = require('node:net');
const crypto = require('node:crypto');
const { once } = require('node:events');
const cp = require('node:child_process');
const path = require('node:path');
const { packet, initializeProbe } = require('../src/rsp');
const { validateSettings } = require('../src/config');
const { ProbeGateway } = require('../src/gateway');
const { acquireProbeLock } = require('../src/probe-lock');

async function until(predicate) {
    const deadline = Date.now() + 3000;
    while (!predicate()) {
        if (Date.now() > deadline) { throw new Error('Test state did not settle.'); }
        await new Promise(resolve => setTimeout(resolve, 5));
    }
}
async function fakeProbe(t, { attach = 'T05', monitor = 'OK' } = {}) {
    const commands = [];
    const sockets = new Set();
    const server = net.createServer(socket => {
        sockets.add(socket); socket.on('close', () => sockets.delete(socket));
        let buffer = '';
        socket.on('data', chunk => {
            buffer += chunk.toString('ascii');
            while (buffer.length) {
                if (/^[+-]/.test(buffer)) { buffer = buffer.slice(1); continue; }
                const end = buffer.indexOf('#');
                if (end < 0 || buffer.length < end + 3) { return; }
                const command = buffer.slice(1, end);
                buffer = buffer.slice(end + 3);
                commands.push(command);
                socket.write('+');
                if (command.startsWith('qRcmd,')) {
                    const output = packet('O' + Buffer.from('probe diagnostic\n').toString('hex'));
                    socket.write(output.subarray(0, 3));
                    socket.write(output.subarray(3));
                    socket.write(packet(monitor));
                } else {
                    socket.write(packet(command.startsWith('vAttach;') ? attach : command === 'vRun;' ? 'T05' : command === '!' ? 'OK' : 'S05'));
                }
            }
        });
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    t.after(async () => { sockets.forEach(socket => socket.destroy()); await new Promise(resolve => server.close(resolve)); });
    return { commands, disconnect: () => sockets.forEach(socket => socket.destroy()),
        config: validateSettings({ host: '127.0.0.1', port: server.address().port, targetIndex: 11, remoteTimeoutSeconds: 1 }) };
}

test('BMD bootstrap handles fragmented console replies, selects the SWD target and optionally resets before handoff', async t => {
    const fake = await fakeProbe(t);
    let output = '';
    const socket = await initializeProbe(fake.config, { reset: true, onOutput: text => { output += text; } });
    t.after(() => socket.destroy());
    assert.equal(fake.commands[0], '!');
    assert.deepEqual(fake.commands.filter(cmd => cmd.startsWith('qRcmd,')).map(cmd => Buffer.from(cmd.slice(6), 'hex').toString()),
        ['connect_rst disable', 'frequency 400000', 'swd_scan']);
    assert.deepEqual(fake.commands.slice(-2), ['vAttach;b', 'vRun;']);
    assert.match(output, /probe diagnostic/);
    assert.equal(socket.isPaused(), true);
});

test('a missing target or unsupported monitor command prevents native GDB from taking ownership', async t => {
    const missing = await fakeProbe(t, { attach: 'E01' });
    await assert.rejects(initializeProbe(missing.config), /Unable to attach/);
    const unsupported = await fakeProbe(t, { monitor: '' });
    await assert.rejects(initializeProbe(unsupported.config), /rejected.*unsupported/);
});

test('gateway waits for the native debugger, bootstraps once and forwards its RSP packet unchanged', async t => {
    const fake = await fakeProbe(t);
    let prepared = 0;
    const gateway = await new ProbeGateway(fake.config, { beforeConnect: async () => { prepared++; } }).start();
    t.after(() => gateway.close());
    assert.equal(prepared, 0, 'F5 build must finish before any target operation');
    const local = net.createConnection({ host: '127.0.0.1', port: gateway.port });
    t.after(() => local.destroy());
    await once(local, 'connect');
    local.write(packet('?'));
    let response = '';
    local.on('data', chunk => { response += chunk.toString('ascii'); });
    await until(() => response.includes('$S05#'));
    assert.equal(prepared, 1);
    assert.equal(fake.commands.at(-1), '?');
    assert(!response.includes('probe diagnostic'), 'bootstrap console output belongs to MAGIC Output, not native GDB');
});

test('endpoint ownership is exclusive and released after a failed/cancelled startup has actually settled', async t => {
    const namespace = `test-${crypto.randomUUID()}`;
    const config = validateSettings({ host: 'probe.local' });
    const release = await acquireProbeLock(config, namespace);
    await assert.rejects(acquireProbeLock(config, namespace), /owned/);
    await release();
    const reacquired = await acquireProbeLock(config, namespace);
    await reacquired();
    let finish, cancelled = false, released = false;
    const gateway = await new ProbeGateway(config, {
        acquireLock: async () => async () => { released = true; },
        beforeConnect: signal => new Promise(resolve => {
            finish = resolve; signal.addEventListener('abort', () => { cancelled = true; });
        }),
        initialize: () => { throw new Error('must not initialize after cancellation'); }
    }).start();
    t.after(() => gateway.close());
    const socket = net.createConnection({ host: '127.0.0.1', port: gateway.port });
    await once(socket, 'connect');
    await until(() => finish);
    socket.destroy();
    await until(() => cancelled);
    assert.equal(released, false, 'keep the lease until the programming process has ended');
    finish();
    await until(() => released);
});

test('a second extension-host process cannot acquire the probe, and a crash releases the OS lease', async t => {
    const namespace = `test-${crypto.randomUUID()}`;
    const modulePath = path.resolve(__dirname, '../src/probe-lock.js');
    const child = cp.spawn(process.execPath, ['-e',
        'require(process.argv[1]).acquireProbeLock({host:"probe",port:2022},process.argv[2]).then(()=>{console.log("locked");process.stdin.resume();}).catch(e=>{console.error(e);process.exit(1);});',
        modulePath, namespace], { windowsHide: true, shell: false, stdio: ['pipe', 'pipe', 'pipe'] });
    t.after(() => child.kill());
    let ready = '';
    child.stdout.on('data', chunk => { ready += chunk; });
    await until(() => ready.includes('locked'));
    await assert.rejects(acquireProbeLock({ host: 'probe', port: 2022 }, namespace), /owned/);
    const exited = once(child, 'exit');
    child.kill();
    await exited;
    const release = await acquireProbeLock({ host: 'probe', port: 2022 }, namespace);
    await release();
});

test('a dropped probe connection closes the native socket and releases ownership without reconnecting', async t => {
    const fake = await fakeProbe(t);
    let failure;
    const gateway = await new ProbeGateway(fake.config, { onFailure: error => { failure = error; } }).start();
    t.after(() => gateway.close());
    const socket = net.createConnection({ host: '127.0.0.1', port: gateway.port });
    t.after(() => socket.destroy());
    socket.resume();
    await until(() => gateway.ready);
    fake.disconnect();
    await until(() => failure && socket.destroyed);
    assert.match(failure.message, /connection closed/);
    assert.equal(fake.commands.filter(command => command === '!').length, 1);
    const release = await acquireProbeLock(fake.config);
    await release();
});
