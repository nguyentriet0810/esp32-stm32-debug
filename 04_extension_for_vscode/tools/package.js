/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** @file package.js @brief Produce a local VSIX with the official VS Code packaging tool. */
const fs = require('node:fs/promises');
const path = require('node:path');
const vsce = require('@vscode/vsce');
async function main() {
    const root = path.resolve(__dirname, '..');
    const manifest = JSON.parse(await fs.readFile(path.join(root, 'package.json'), 'utf8'));
    const directory = path.join(root, 'dist');
    await fs.mkdir(directory, { recursive: true });
    const packagePath = path.join(directory, `${manifest.name}-${manifest.version}.vsix`);
    await vsce.createVSIX({ cwd: root, packagePath, dependencies: true, useYarn: false });
    console.log(`VSIX: ${packagePath}`);
}
main().catch(error => { console.error(error); process.exitCode = 1; });
