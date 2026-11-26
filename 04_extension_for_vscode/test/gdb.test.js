/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const cp = require('node:child_process');
const { runGdb } = require('../src/gdb');
const { ProbeService } = require('../src/probe-service');
const { elfFixture, target } = require('./helpers');

async function temporary(t) {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'magic-probe-test-'));
    t.after(() => {
        assert.equal(path.dirname(path.resolve(root)), path.resolve(os.tmpdir()));
        assert(path.basename(root).startsWith('magic-probe-test-'));
        return fs.rm(root, { recursive: true, force: true });
    });
    return root;
}

test('child runner uses argument arrays, retains output and reports process failure', async t => {
    const directory = await temporary(t);
    const options = { executable: process.execPath, directory, script: 'echo test', timeoutMs: 3000,
        spawn: (exe, args, settings) => {
            assert.equal(settings.shell, false);
            assert.equal(settings.windowsHide, true);
            assert.equal(args[3], '--command');
            return cp.spawn(exe, ['-e', 'console.log("hello"); console.error("details"); process.exit(2)'], settings);
        } };
    await assert.rejects(runGdb(options), /code 2/);
    assert.match(await fs.readFile(path.join(directory, 'output.log'), 'utf8'), /hello/);
    assert.match(await fs.readFile(path.join(directory, 'output.log'), 'utf8'), /details/);
});

test('timeout and cancellation terminate a child and settle the operation', async t => {
    const directory = await temporary(t);
    const spawn = (exe, _, settings) => cp.spawn(exe, ['-e', 'setInterval(()=>{},1000)'], settings);
    await assert.rejects(runGdb({ executable: process.execPath, directory, script: '', timeoutMs: 30, spawn }), /timed out/);
    const abort = new AbortController();
    const run = runGdb({ executable: process.execPath, directory, script: '', timeoutMs: 5000, spawn, signal: abort.signal });
    setTimeout(() => abort.abort(), 30);
    await assert.rejects(run, /cancelled/);
});

test('service rejects overlapping jobs and releases ownership after failure', async t => {
    const storage = await temporary(t);
    let rejectJob;
    const runner = () => new Promise((_, reject) => { rejectJob = reject; });
    const service = new ProbeService({ storage, runner });
    const options = { gdbPath: process.execPath };
    const first = service.checkConnection(options);
    while (!rejectJob) { await new Promise(resolve => setImmediate(resolve)); }
    await assert.rejects(service.scan(options), /busy/);
    rejectJob(new Error('connection lost'));
    await assert.rejects(first, /connection lost/);
    assert.equal(service.busy, undefined);
});

test('flash uses immutable ELF, a full backup and verifies programmed bytes', async t => {
    const storage = await temporary(t);
    const file = path.join(storage, 'app.elf');
    const bytes = elfFixture();
    await fs.writeFile(file, bytes);
    let calls = 0;
    const runner = async options => {
        calls++;
        if (calls === 1) {
            return { output: 'MAGIC_CPUID=0x411fc231\nMAGIC_DEVICE_ID=0x6410\nMAGIC_FLASH_KIB=64\nMAGIC_INSPECT_OK' };
        }
        assert.equal(await fs.readFile(path.join(options.directory, 'image.elf')).then(b => b.equals(bytes)), true);
        const backup = path.join(options.directory, /^dump binary memory ([a-zA-Z0-9_.-]+) 0x08000000/m.exec(options.script)[1]);
        await fs.writeFile(backup, Buffer.alloc(target.flashKiB * 1024, 0xff));
        await fs.writeFile(path.join(options.directory, 'readback.bin'), bytes.subarray(256));
        return { output: 'MAGIC_BACKUP_OK\nMAGIC_FLASH_DONE' };
    };
    const service = new ProbeService({ storage, runner });
    const plan = await service.prepareFlash({ gdbPath: process.execPath }, file);
    const result = await service.flash(plan);
    assert.equal((await fs.stat(result.backupPath)).size, 65536);
    assert.equal(result.loadBytes, 64);
    await fs.writeFile(file, elfFixture(80));
    await assert.rejects(service.flash(plan), /changed after review/);
    assert.equal(calls, 2, 'changed ELF must not invoke programming');
});
