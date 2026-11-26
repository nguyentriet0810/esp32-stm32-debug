/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const { parseElf, validateF103Image, verifyReadback } = require('../src/elf');
const { elfFixture, target } = require('./helpers');

test('ARM image preserves load bytes, reset vector and readback', () => {
    const bytes = elfFixture();
    const image = parseElf(bytes);
    validateF103Image(image, target);
    assert.equal(image.loadBytes, 64);
    assert.equal(image.resetAddress, 0x08000020);
    verifyReadback(image, bytes.subarray(256));
    const corrupt = Buffer.from(bytes.subarray(256)); corrupt[33] ^= 1;
    assert.throws(() => verifyReadback(image, corrupt), /mismatch/);
    assert.throws(() => verifyReadback(image, Buffer.alloc(1)), /length/);
});

test('reject wrong architecture, truncation, bad table and reset vector', () => {
    assert.throws(() => parseElf(Buffer.alloc(8)), /ELF/);
    let data = elfFixture(); data.writeUInt16LE(94, 18);
    assert.throws(() => parseElf(data), /ARM/);
    data = elfFixture(); data.writeUInt16LE(200, 44);
    assert.throws(() => parseElf(data), /header/);
    data = elfFixture(); data.writeUInt32LE(1000, 56);
    assert.throws(() => parseElf(data), /bounds/);
    data = elfFixture(); data.writeUInt32LE(0x08000020, 260);
    assert.throws(() => parseElf(data), /Thumb/);
    data = elfFixture(); data.writeUInt32LE(0x20000000, 64);
    assert.throws(() => parseElf(data), /flash-based/);
});

test('F103 size signature and SRAM bounds protect programming', () => {
    const image = parseElf(elfFixture());
    assert.throws(() => validateF103Image(image, { ...target, deviceId: 0x413 }), /F103/);
    assert.throws(() => validateF103Image(image, { ...target, flashKiB: 0xffff }), /F103/);
    assert.throws(() => validateF103Image({ ...image, stack: 0x20010000 }, target), /stack/);
    const tooLarge = parseElf(elfFixture(65540));
    assert.throws(() => validateF103Image(tooLarge, target), /64 KiB/);
    validateF103Image(tooLarge, { ...target, flashKiB: 128 });
});

test('GDB load sections cannot escape validated flash or introduce unverified bytes', () => {
    let data = elfFixture(); data.writeUInt32LE(0x20000000, 136);
    assert.throws(() => parseElf(data), /loadable ELF section/);
    data = elfFixture(); data.writeUInt32LE(65535, 144);
    assert.throws(() => parseElf(data), /section bounds/);
    data = elfFixture(); data.writeUInt32LE(0, 32);
    assert.throws(() => parseElf(data), /section header/);
    data = elfFixture(); data.writeUInt16LE(3, 48);
    data.writeUInt32LE(16, 144);
    data.writeUInt32LE(1, 168); data.writeUInt32LE(6, 172);
    data.writeUInt32LE(0x08000020, 176); data.writeUInt32LE(288, 180); data.writeUInt32LE(32, 184);
    const image = parseElf(data);
    assert.equal(image.loadBytes, 48);
    const readback = Buffer.from(data.subarray(256));
    readback.fill(0xff, 16, 32); // GDB does not program padding between the two sections.
    verifyReadback(image, readback);
    readback[33] ^= 1;
    assert.throws(() => verifyReadback(image, readback), /mismatch/);
});
