/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
const path = require('node:path');
/** @file workflows.js @brief Generate reviewable BMD/ARM GDB command files without a shell. */
function quoteGdb(value) {
    if (typeof value !== 'string' || /[\0\r\n]/.test(value)) { throw new Error('Invalid GDB path.'); }
    return `"${value.replaceAll('\\', '/').replaceAll('"', '\\"')}"`;
}

function connectCommands(config, elf) {
    const lines = ['set pagination off', 'set confirm off', `set remotetimeout ${config.remoteTimeoutSeconds}`,
        'set mem inaccessible-by-default off'];
    if (elf) { lines.push(`file ${quoteGdb(elf)}`); }
    lines.push(`target extended-remote ${config.host}:${config.port}`,
        `monitor connect_rst ${config.connectUnderReset ? 'enable' : 'disable'}`,
        `monitor frequency ${config.clockHz}`);
    return lines;
}

function scanCommands(config) {
    return [...connectCommands(config), 'echo MAGIC_SCAN_BEGIN\\n', 'monitor swd_scan',
        'echo MAGIC_SCAN_END\\n', 'quit'].join('\n') + '\n';
}

function checkCommands(config) {
    return [...connectCommands(config), 'monitor version', 'echo MAGIC_CONNECTION_OK\\n', 'quit'].join('\n') + '\n';
}

function inspectCommands(config) {
    return [...connectCommands(config), 'monitor swd_scan', `attach ${config.targetIndex}`,
        'printf "MAGIC_CPUID=0x%08x\\n", *(unsigned int *)0xe000ed00',
        'printf "MAGIC_DEVICE_ID=0x%08x\\n", *(unsigned int *)0xe0042000',
        'printf "MAGIC_FLASH_KIB=%u\\n", *(unsigned short *)0x1ffff7e0',
        'detach', 'echo MAGIC_INSPECT_OK\\n', 'quit'].join('\n') + '\n';
}

function flashCommands(config, image, target, files) {
    const flashEnd = (0x08000000 + target.flashKiB * 1024).toString(16);
    const dumpFile = file => {
        const name = path.basename(file);
        if (!/^[a-zA-Z0-9_.-]+$/.test(name)) { throw new Error('GDB dump filenames must be generated safe basenames.'); }
        return name;
    };
    return [...connectCommands(config, files.elf), 'monitor swd_scan', `attach ${config.targetIndex}`,
        'if ((*(unsigned int *)0xe0042000 & 0xfff) != 0x410)',
        '  echo Target changed; programming aborted\\n', '  quit 1', 'end',
        `if (*(unsigned short *)0x1ffff7e0 != ${target.flashKiB})`,
        '  echo Flash size changed; programming aborted\\n', '  quit 1', 'end',
        `dump binary memory ${dumpFile(files.backup)} 0x08000000 0x${flashEnd}`,
        'echo MAGIC_BACKUP_OK\\n', 'load', 'compare-sections',
        `dump binary memory ${dumpFile(files.readback)} 0x08000000 0x${image.end.toString(16)}`,
        `hbreak *0x${image.resetAddress.toString(16)}`, 'run', 'delete breakpoints', 'detach',
        'echo MAGIC_FLASH_DONE\\n', 'quit'].join('\n') + '\n';
}

function parseScan(output) {
    const clean = output.replaceAll('\r', '');
    // GDB monitor output and echo markers use different pipes. Their arrival order
    // can interleave, so a valid target row may arrive after MAGIC_SCAN_END.
    const complete = clean.includes('MAGIC_SCAN_BEGIN\n') && clean.includes('MAGIC_SCAN_END');
    const block = complete ? clean.split('Available Targets:')[1] ?? '' : '';
    const targets = [];
    for (const line of block.split('\n')) {
        const match = /^\s*(\d+)\s+(\*)?\s*(\S.*)$/.exec(line);
        if (match && Number(match[1]) >= 1 && Number(match[1]) <= 16) {
            targets.push({ index: Number(match[1]), attached: Boolean(match[2]), driver: match[3].trim() });
        }
    }
    if (!targets.length) { throw new Error('SWD scan found no target. Check target power, GND and SWD wiring; see Output.'); }
    return targets;
}

function parseInspection(output) {
    const read = (name, radix) => {
        const match = new RegExp(`${name}=(0x[0-9a-fA-F]+|[0-9]+)`).exec(output);
        if (!match) { throw new Error('GDB did not return a complete target identity. See Output.'); }
        return Number.parseInt(match[1], radix);
    };
    if (!output.includes('MAGIC_INSPECT_OK')) { throw new Error('Target inspection did not complete.'); }
    return { cpuid: read('MAGIC_CPUID', 16), deviceId: read('MAGIC_DEVICE_ID', 16), flashKiB: read('MAGIC_FLASH_KIB', 10) };
}

module.exports = { quoteGdb, connectCommands, scanCommands, checkCommands, inspectCommands,
    flashCommands, parseScan, parseInspection };
