/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
// Feasibility test: the unmodified GDB client uses target remote after BMD bootstrap.
const path = require('node:path');
const { ProbeGateway } = require('../src/gateway');
const { validateSettings, resolveGdb } = require('../src/config');
const { runGdb } = require('../src/gdb');
const { quoteGdb } = require('../src/workflows');
async function main() {
    const config = validateSettings({ host: process.argv[2] });
    const elf = process.argv[3];
    if (!process.argv[2] || !elf) { throw new Error('Usage: node tools/gateway-smoke.js <host> <ELF>'); }
    const gateway = await new ProbeGateway(config, { onOutput: text => process.stdout.write(text) }).start();
    try {
        const result = await runGdb({ executable: resolveGdb(), directory: path.resolve('.cache/gateway-smoke'),
            timeoutMs: 30000, onOutput: text => process.stdout.write(text),
            script: `set pagination off\nset confirm off\nfile ${quoteGdb(elf)}\ntarget remote 127.0.0.1:${gateway.port}\np blink_ready\nbt\ndetach\nquit\n` });
        if (!result.output.includes('$1 = 1')) { throw new Error('GDB could not evaluate the live test program.'); }
        console.log('PASS standard target remote through BMD bootstrap gateway. No flash writes.');
    } finally { await gateway.close(); }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
