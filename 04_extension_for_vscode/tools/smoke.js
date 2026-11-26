/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** @file smoke.js @brief Read-only live test using the same PC service as the extension. */
const path = require('node:path');
const { resolveGdb, validateSettings } = require('../src/config');
const { acquireProbeLock } = require('../src/probe-lock');
const { ProbeService } = require('../src/probe-service');
async function main() {
    const host = process.argv[2];
    if (!host) { throw new Error('Usage: npm run smoke -- <probe-ip> [ARM-GDB-path]'); }
    const service = new ProbeService({ storage: path.join(__dirname, '../.cache/smoke'), onOutput: text => process.stdout.write(text) });
    const options = { host, gdbPath: resolveGdb(process.argv[3], process.cwd()) };
    const release = await acquireProbeLock(validateSettings(options));
    try {
        await service.checkConnection(options);
        const targets = await service.scan(options);
        console.log('TARGETS:', targets);
        const target = await service.inspect(options);
        console.log('IDENTITY:', target);
        console.log('PASS live connection, SWD scan and F103 identity. No target flash writes.');
    } finally { await release(); }
}
main().catch(error => { console.error(error.message); process.exitCode = 1; });
