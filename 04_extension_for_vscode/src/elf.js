/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** @file elf.js @brief Parse loadable ARM ELF32 segments and bound F103 programming. */
const crypto = require('node:crypto');
const FLASH_BASE = 0x08000000;

function parseElf(buffer) {
    if (!Buffer.isBuffer(buffer) || buffer.length < 52 || buffer.readUInt32BE(0) !== 0x7f454c46) {
        throw new Error('Not an ELF file. Choose the STM32 ELF, not a BIN or an ESP32 image.');
    }
    if (buffer[4] !== 1 || buffer[5] !== 1 || buffer[6] !== 1 ||
        buffer.readUInt16LE(16) !== 2 || buffer.readUInt16LE(18) !== 40) {
        throw new Error('Expected a little-endian ARM ELF32 executable (STM32).');
    }
    const offset = buffer.readUInt32LE(28);
    const entrySize = buffer.readUInt16LE(42);
    const count = buffer.readUInt16LE(44);
    if (count < 1 || count > 128 || entrySize < 32 || offset < 52 ||
        offset + count * entrySize > buffer.length) {
        throw new Error('Invalid ELF program header table.');
    }
    const loads = [];
    for (let index = 0; index < count; index++) {
        const header = offset + index * entrySize;
        if (buffer.readUInt32LE(header) !== 1) { continue; }
        const fileOffset = buffer.readUInt32LE(header + 4);
        const virtualAddress = buffer.readUInt32LE(header + 8);
        const address = buffer.readUInt32LE(header + 12);
        const size = buffer.readUInt32LE(header + 16);
        const memorySize = buffer.readUInt32LE(header + 20);
        if (fileOffset + size > buffer.length || size > memorySize || address + memorySize > 0x100000000 ||
            virtualAddress + memorySize > 0x100000000) {
            throw new Error('Invalid ELF loadable segment bounds.');
        }
        if (size > 0) {
            if (address < FLASH_BASE || address + size > 0x08200000) {
                throw new Error('This version supports flash-based STM32 images at 0x08000000.');
            }
            loads.push({ address, virtualAddress, size, fileOffset });
        }
    }
    // GDB loads sections, not all PT_LOAD padding. Bound every allocated file section to
    // its matching segment/LMA, so the validated bytes are exactly the bytes GDB can load.
    const sectionOffset = buffer.readUInt32LE(32);
    const sectionSize = buffer.readUInt16LE(46);
    const sectionCount = buffer.readUInt16LE(48);
    if (sectionCount < 1 || sectionCount > 8192 || sectionSize < 40 || sectionOffset < 52 ||
        sectionOffset + sectionCount * sectionSize > buffer.length) {
        throw new Error('A valid ELF section header table is required for GDB load.');
    }
    const segments = [];
    for (let index = 0; index < sectionCount; index++) {
        const header = sectionOffset + index * sectionSize;
        const type = buffer.readUInt32LE(header + 4);
        const flags = buffer.readUInt32LE(header + 8);
        const virtualAddress = buffer.readUInt32LE(header + 12);
        const fileOffset = buffer.readUInt32LE(header + 16);
        const size = buffer.readUInt32LE(header + 20);
        if (!(flags & 2) || type === 8 || size === 0) { continue; } // Non-allocated or BSS.
        if (type === 0 || (flags & 0x800) || fileOffset + size > buffer.length ||
            virtualAddress + size > 0x100000000) {
            throw new Error('Invalid allocated ELF section bounds or compression.');
        }
        const matches = loads.filter(load => fileOffset >= load.fileOffset &&
            fileOffset + size <= load.fileOffset + load.size &&
            virtualAddress === load.virtualAddress + fileOffset - load.fileOffset);
        if (matches.length !== 1) {
            throw new Error('Every loadable ELF section must map to one validated flash segment.');
        }
        const load = matches[0];
        segments.push({ address: load.address + fileOffset - load.fileOffset, size, fileOffset });
    }
    segments.sort((a, b) => a.address - b.address);
    if (!segments.length || segments[0].address !== FLASH_BASE || segments[0].size < 8) {
        throw new Error('A standalone image with a vector table at 0x08000000 is required.');
    }
    for (let index = 1; index < segments.length; index++) {
        if (segments[index].address < segments[index - 1].address + segments[index - 1].size) {
            throw new Error('Overlapping ELF flash segments are not supported.');
        }
    }
    const vector = segments[0].fileOffset;
    const resetVector = buffer.readUInt32LE(vector + 4);
    const resetAddress = resetVector - (resetVector % 2);
    if (!(resetVector & 1) || !segments.some(s => resetAddress >= s.address && resetAddress < s.address + s.size)) {
        throw new Error('The reset vector must point to Thumb code inside the image.');
    }
    return {
        buffer, segments, resetAddress,
        stack: buffer.readUInt32LE(vector),
        start: FLASH_BASE,
        end: Math.max(...segments.map(s => s.address + s.size)),
        loadBytes: segments.reduce((sum, s) => sum + s.size, 0),
        sha256: crypto.createHash('sha256').update(buffer).digest('hex')
    };
}

function validateF103Image(image, target) {
    if ((target.deviceId & 0xfff) !== 0x410 || (target.cpuid & 0xff00fff0) !== 0x4100c230 ||
        ![64, 128].includes(target.flashKiB)) {
        throw new Error('Flash v0.1 supports medium-density STM32F103 Cortex-M3 with 64/128 KiB signature.');
    }
    if (image.end > FLASH_BASE + target.flashKiB * 1024) {
        throw new Error(`ELF exceeds the target's reported ${target.flashKiB} KiB flash.`);
    }
    if (image.stack <= 0x20000000 || image.stack > 0x20005000 || image.stack % 8 !== 0) {
        throw new Error('Initial stack must be aligned inside the F103 20 KiB SRAM.');
    }
}

function verifyReadback(image, readback) {
    if (readback.length !== image.end - image.start) { throw new Error('Flash readback length is incorrect.'); }
    for (const segment of image.segments) {
        const expected = image.buffer.subarray(segment.fileOffset, segment.fileOffset + segment.size);
        const actual = readback.subarray(segment.address - image.start, segment.address - image.start + segment.size);
        if (!expected.equals(actual)) { throw new Error(`Flash readback mismatch at 0x${segment.address.toString(16)}.`); }
    }
}

module.exports = { parseElf, validateF103Image, verifyReadback, FLASH_BASE };
