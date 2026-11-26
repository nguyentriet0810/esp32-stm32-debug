/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** @file config.js @brief Validate host settings and resolve local ARM tool paths. */
const fs = require('node:fs');
const path = require('node:path');

function integer(value, name, minimum, maximum) {
    if (!Number.isInteger(value) || value < minimum || value > maximum) {
        throw new Error(`${name} must be an integer from ${minimum} to ${maximum}.`);
    }
    return value;
}

function validateSettings(input = {}) {
    const host = input.host ?? '192.168.4.1';
    if (typeof host !== 'string' || host.length > 253 ||
        !/^[a-zA-Z0-9](?:[a-zA-Z0-9.-]*[a-zA-Z0-9])?$/.test(host)) {
        throw new Error('Probe host must be an IPv4 address or DNS name, without spaces or commands.');
    }
    if (input.connectUnderReset !== undefined && typeof input.connectUnderReset !== 'boolean') {
        throw new Error('connectUnderReset must be a boolean.');
    }
    return {
        host,
        port: integer(input.port ?? 2022, 'TCP port', 1, 65535),
        clockHz: integer(input.clockHz ?? 400000, 'SWD frequency', 1000, 8000000),
        targetIndex: integer(input.targetIndex ?? 1, 'Target index', 1, 16),
        connectUnderReset: input.connectUnderReset ?? false,
        remoteTimeoutSeconds: integer(input.remoteTimeoutSeconds ?? 15, 'Remote timeout', 1, 120),
        operationTimeoutSeconds: integer(input.operationTimeoutSeconds ?? 120, 'Operation timeout', 5, 600)
    };
}

function resolveFile(value, workspaceFolder, environment = process.env) {
    if (typeof value !== 'string' || !value || /[\r\n\0]/.test(value)) {
        throw new Error('Choose a local file path without control characters.');
    }
    let expanded = value.replace(/\$\{env:([^}]+)\}/g, (_, key) => {
        if (!environment[key]) { throw new Error(`Environment variable ${key} is not set.`); }
        return environment[key];
    });
    if (expanded.includes('${workspaceFolder}')) {
        if (!workspaceFolder) { throw new Error('Open a workspace folder to resolve ${workspaceFolder}.'); }
        expanded = expanded.replaceAll('${workspaceFolder}', workspaceFolder);
    }
    if (expanded.includes('${') || /[\r\n\0]/.test(expanded)) {
        throw new Error('The file path contains an unsupported variable or control character.');
    }
    if (!path.isAbsolute(expanded)) {
        if (!workspaceFolder) { throw new Error('Use an absolute path when no workspace folder is open.'); }
        expanded = path.resolve(workspaceFolder, expanded);
    }
    return expanded;
}

function requireFile(file) {
    if (!fs.statSync(file, { throwIfNoEntry: false })?.isFile()) {
        throw new Error(`File does not exist: ${file}`);
    }
    return file;
}

function resolveGdb(configuredPath, workspaceFolder) {
    if (configuredPath && /[\\/$]/.test(configuredPath)) { return requireFile(resolveFile(configuredPath, workspaceFolder)); }
    const executable = process.platform === 'win32' ? 'arm-none-eabi-gdb.exe' : 'arm-none-eabi-gdb';
    for (const directory of (process.env.PATH ?? '').split(path.delimiter)) {
        const name = configuredPath ? configuredPath + (process.platform === 'win32' && !configuredPath.endsWith('.exe') ? '.exe' : '') : executable;
        const candidate = path.join(directory, name);
        if (fs.statSync(candidate, { throwIfNoEntry: false })?.isFile()) { return candidate; }
    }
    if (configuredPath) { return requireFile(resolveFile(configuredPath, workspaceFolder)); }
    if (process.platform === 'win32' && fs.existsSync('C:/ST')) {
        const installations = fs.readdirSync('C:/ST').filter(name => name.startsWith('STM32CubeCLT_'))
            .sort((a, b) => b.localeCompare(a, undefined, { numeric: true }));
        for (const installation of installations) {
            const candidate = path.join('C:/ST', installation, 'GNU-tools-for-STM32/bin', executable);
            if (fs.statSync(candidate, { throwIfNoEntry: false })?.isFile()) { return candidate; }
        }
    }
    throw new Error('ARM GDB was not found. Set MAGIC Probe: Gdb Path to arm-none-eabi-gdb.');
}

module.exports = { validateSettings, resolveFile, requireFile, resolveGdb };
