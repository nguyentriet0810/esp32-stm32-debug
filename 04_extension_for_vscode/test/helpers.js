/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** @file helpers.js @brief Small synthetic ARM ELF images for structural/safety tests. */
function elfFixture(size = 64) {
    const buffer = Buffer.alloc(256 + size);
    buffer.writeUInt32BE(0x7f454c46, 0);
    buffer[4] = 1; buffer[5] = 1; buffer[6] = 1;
    buffer.writeUInt16LE(2, 16); buffer.writeUInt16LE(40, 18);
    buffer.writeUInt32LE(0x08000020, 24);
    buffer.writeUInt32LE(52, 28);
    buffer.writeUInt32LE(84, 32);
    buffer.writeUInt16LE(52, 40); buffer.writeUInt16LE(32, 42); buffer.writeUInt16LE(1, 44);
    buffer.writeUInt16LE(40, 46); buffer.writeUInt16LE(2, 48);
    buffer.writeUInt32LE(1, 52); buffer.writeUInt32LE(256, 56);
    buffer.writeUInt32LE(0x08000000, 60); buffer.writeUInt32LE(0x08000000, 64);
    buffer.writeUInt32LE(size, 68); buffer.writeUInt32LE(size, 72); buffer.writeUInt32LE(5, 76);
    buffer.writeUInt32LE(1, 128); buffer.writeUInt32LE(6, 132);
    buffer.writeUInt32LE(0x08000000, 136); buffer.writeUInt32LE(256, 140); buffer.writeUInt32LE(size, 144);
    buffer.writeUInt32LE(0x20005000, 256); buffer.writeUInt32LE(0x08000021, 260);
    return buffer;
}
const target = { cpuid: 0x411fc231, deviceId: 0x00006410, flashKiB: 64 };
module.exports = { elfFixture, target };
