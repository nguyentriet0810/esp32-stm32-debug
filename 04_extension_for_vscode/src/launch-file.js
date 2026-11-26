/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** Surgical JSONC updates: keep wired entries, comments and other launch properties. */
const jsonc = require('jsonc-parser');

function addConfiguration(text, config, { tabSize = 4, insertSpaces = true, eol = '\n' } = {}) {
    const errors = [];
    const root = jsonc.parse(text, errors, { allowTrailingComma: true, disallowComments: false });
    if (errors.length || !root || typeof root !== 'object' || Array.isArray(root)) {
        throw new Error('Fix the JSONC errors in launch.json before adding a MAGIC profile.');
    }
    if (root.configurations !== undefined && !Array.isArray(root.configurations)) {
        throw new Error('launch.json configurations must be an array.');
    }
    if (root.configurations?.some(item => item.name === config.name)) {
        throw new Error(`A launch configuration named '${config.name}' already exists.`);
    }
    const options = { formattingOptions: { tabSize, insertSpaces, eol } };
    let value = text;
    if (!root.version) { value = jsonc.applyEdits(value, jsonc.modify(value, ['version'], '0.2.0', options)); }
    if (!root.configurations) {
        return jsonc.applyEdits(value, jsonc.modify(value, ['configurations'], [config], options));
    }
    const tree = jsonc.parseTree(value);
    const array = jsonc.findNodeAtLocation(tree, ['configurations']);
    const close = array.offset + array.length - 1;
    const line = value.slice(value.lastIndexOf('\n', close - 1) + 1, close);
    const ownLine = /^[\t ]*$/.test(line);
    const propertyLine = value.slice(value.lastIndexOf('\n', array.offset) + 1, array.offset);
    const closingIndent = ownLine ? line : /^\s*/.exec(propertyLine)[0];
    const indentUnit = insertSpaces ? ' '.repeat(tabSize) : '\t';
    const indent = closingIndent + indentUnit;
    const formatted = JSON.stringify(config, null, indentUnit).split('\n').map(part => indent + part).join(eol);
    const offset = ownLine ? close - line.length : close;
    const prefix = value.slice(0, offset).endsWith('\n') ? '' : eol;
    const edits = [{ offset, length: 0, content: prefix + formatted + eol + (ownLine ? '' : closingIndent) }];
    const last = array.children?.at(-1);
    if (last) {
        const end = last.offset + last.length;
        const scanner = jsonc.createScanner(value.slice(end, close), true);
        if (scanner.scan() !== jsonc.SyntaxKind.CommaToken) { edits.push({ offset: end, length: 0, content: ',' }); }
    }
    return jsonc.applyEdits(value, edits);
}

module.exports = { addConfiguration };
