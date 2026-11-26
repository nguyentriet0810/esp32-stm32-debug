/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const { validateSettings, resolveFile } = require('../src/config');
const w = require('../src/workflows');
const { createProbeConfiguration, runtimeConfiguration } = require('../src/project-debug');
const { parseElf } = require('../src/elf');
const { elfFixture, target } = require('./helpers');

test('reject script injection and invalid settings before GDB is started', () => {
    for (const host of ['host\nload evil', 'host;quit', '-bad', '127.0.0.1:2022', '', 'with space']) {
        assert.throws(() => validateSettings({ host }));
    }
    assert.throws(() => validateSettings({ clockHz: 0 }));
    assert.throws(() => validateSettings({ port: 1.5 }));
    assert.throws(() => resolveFile('${env:MISSING}/a.elf', undefined, {}));
    assert.throws(() => w.quoteGdb('name\nload evil'));
    assert.equal(w.quoteGdb('C:\\Some folder\\a.elf'), '"C:/Some folder/a.elf"');
});

test('parse real BMD target list and identity, reject incomplete/no-target output', () => {
    const text = 'header\r\nMAGIC_SCAN_BEGIN\r\nTarget voltage: Unknown\r\nAvailable Targets:\r\nNo. Att Driver\r\n 1      STM32F1 L/M density M3\r\n 2   *  ARM Cortex-M4\r\nMAGIC_SCAN_END\r\n';
    assert.deepEqual(w.parseScan(text), [{ index: 1, attached: false, driver: 'STM32F1 L/M density M3' },
        { index: 2, attached: true, driver: 'ARM Cortex-M4' }]);
    assert.throws(() => w.parseScan('SWD scan failed!'), /no target/);
    assert.equal(w.parseScan('MAGIC_SCAN_BEGIN\nAvailable Targets:\nNo. Att Driver\nMAGIC_SCAN_END\n 1      STM32F1 L/M density M3\n')[0].index, 1);
    assert.deepEqual(w.parseInspection('MAGIC_CPUID=0x411fc231\nMAGIC_DEVICE_ID=0x00006410\nMAGIC_FLASH_KIB=64\nMAGIC_INSPECT_OK'), target);
    assert.throws(() => w.parseInspection('MAGIC_FLASH_KIB=64'), /complete/);
});

test('flash backs up before load and bounds readback to validated image', () => {
    const image = parseElf(elfFixture());
    const settings = validateSettings({});
    const script = w.flashCommands(settings, image, target,
        { elf: '/tmp/a.elf', backup: '/tmp/backup.bin', readback: '/tmp/readback.bin' });
    assert(script.indexOf('dump binary memory backup.bin') < script.indexOf('\nload\n'));
    assert(script.includes('0x08000000 0x8010000'));
    assert(script.includes('0x08000000 0x8000040'));
    assert(script.includes('hbreak *0x8000020'));
    assert(!script.includes('mass_erase') && !script.includes('monitor reset halt'));
    assert(script.includes('Flash size changed; programming aborted'));
});

test('native Cortex-Debug keeps its project and delegates BMD initialization to the gateway', () => {
    const original = { name: 'STM32', type: 'cortex-debug', request: 'attach', executable: '/project/a.elf',
        gdbPath: '/tools/arm-none-eabi-gdb', preLaunchTask: 'Build', servertype: 'openocd', configFiles: ['stlink.cfg'] };
    const profile = createProbeConfiguration(original).config;
    const config = runtimeConfiguration(profile, 43210, 120, 'session-id');
    assert.equal(config.servertype, 'external');
    assert.equal(config.gdbTarget, '127.0.0.1:43210');
    assert.equal(config.preLaunchTask, 'Build');
    assert.equal(config.executable, original.executable);
    assert.equal(config.gdbPath, original.gdbPath);
    assert.deepEqual(config.overrideAttachCommands, []);
    assert.equal(original.servertype, 'openocd');
    assert.deepEqual(original.configFiles, ['stlink.cfg']);
});
