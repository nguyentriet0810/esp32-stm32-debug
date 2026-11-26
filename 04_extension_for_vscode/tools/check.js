/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** @file check.js @brief Syntax and manifest/entry-point checks without an Extension Host. */
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname, '..');
const manifest = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8'));
for (const directory of ['src', 'tools', 'test']) {
    for (const file of fs.readdirSync(path.join(root, directory)).filter(name => name.endsWith('.js'))) {
        const result = spawnSync(process.execPath, ['--check', path.join(root, directory, file)], { stdio: 'inherit', windowsHide: true });
        if (result.status !== 0) { process.exit(1); }
    }
}
if (!fs.existsSync(path.join(root, manifest.main))) { throw new Error('Missing extension entry point.'); }
const extension = fs.readFileSync(path.join(root, manifest.main), 'utf8');
for (const entry of manifest.contributes.commands) {
    const name = entry.command.slice('magicProbe.'.length);
    if (!new RegExp(`\\b${name}:`).test(extension)) { throw new Error(`Command not implemented: ${entry.command}`); }
}
console.log('PASS JavaScript syntax, manifest and all contributed command handlers.');
