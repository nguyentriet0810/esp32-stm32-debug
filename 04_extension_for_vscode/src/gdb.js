/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** @file gdb.js @brief Bounded ARM GDB child processes with output, timeout and cancellation. */
const fs = require('node:fs/promises');
const path = require('node:path');
const childProcess = require('node:child_process');

async function runGdb(options) {
    const { executable, script, directory, timeoutMs, signal, onOutput = () => {} } = options;
    if (signal?.aborted) { throw new Error('Operation cancelled.'); }
    await fs.mkdir(directory, { recursive: true });
    const commandFile = path.join(directory, 'commands.gdb');
    await fs.writeFile(commandFile, script, { mode: 0o600 });
    const args = ['--quiet', '--nx', '--batch', '--command', commandFile];
    const spawn = options.spawn ?? childProcess.spawn;
    let output = '';
    try {
        const result = await new Promise((resolve, reject) => {
            const child = spawn(executable, args, { cwd: directory, windowsHide: true, shell: false,
                stdio: ['ignore', 'pipe', 'pipe'] });
            let failure;
            let finished = false;
            const abort = () => { failure = new Error('Operation cancelled; target flash may be partial if programming had started.'); child.kill(); };
            const timer = setTimeout(() => {
                failure = new Error(`ARM GDB timed out after ${timeoutMs / 1000} seconds.`);
                child.kill();
            }, timeoutMs);
            const complete = (error, code) => {
                if (finished) { return; }
                finished = true;
                clearTimeout(timer);
                signal?.removeEventListener('abort', abort);
                if (error || failure) { reject(error ?? failure); }
                else if (code !== 0) { reject(new Error(`ARM GDB exited with code ${code}. See MAGIC Probe Output.`)); }
                else { resolve({ output, directory, commandFile }); }
            };
            const receive = chunk => {
                const text = chunk;
                output += text;
                onOutput(text);
                if (Buffer.byteLength(output) > 4 * 1024 * 1024) {
                    failure = new Error('ARM GDB output exceeded the 4 MiB limit.');
                    child.kill();
                }
            };
            child.stdout.setEncoding('utf8');
            child.stderr.setEncoding('utf8');
            child.stdout.on('data', receive);
            child.stderr.on('data', receive);
            child.once('error', error => complete(error));
            child.once('close', code => complete(undefined, code));
            signal?.addEventListener('abort', abort, { once: true });
            if (signal?.aborted) { abort(); }
        });
        return result;
    } finally {
        await fs.writeFile(path.join(directory, 'output.log'), output, { mode: 0o600 });
    }
}

module.exports = { runGdb };
