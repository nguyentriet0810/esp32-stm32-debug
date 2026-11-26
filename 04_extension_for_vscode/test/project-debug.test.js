/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const { createProbeConfiguration, runtimeConfiguration, flashImage, validateProbeConfiguration } = require('../src/project-debug');
const { addConfiguration } = require('../src/launch-file');
const jsonc = require('jsonc-parser');

test('adding a probe profile preserves wired JSONC, comments and compounds', () => {
    const original = '{\r\n  // existing team settings\r\n  "version": "0.2.0",\r\n  "configurations": [\r\n    /* wired probe */ { "name": "Wired", "type": "cortex-debug", "request": "launch", },\r\n  ],\r\n  "compounds": [{"name":"Team", "configurations":["Wired"]}],\r\n}\r\n';
    const addition = { name: 'Wired (MAGIC Wi-Fi)', type: 'cortex-debug', request: 'launch',
        executable: '${command:project.elf}', preLaunchTask: 'Build' };
    const result = addConfiguration(original, addition, { eol: '\r\n' });
    assert(result.includes('/* wired probe */ { "name": "Wired", "type": "cortex-debug", "request": "launch", },'));
    assert(result.includes('// existing team settings'));
    const parsed = jsonc.parse(result);
    assert.deepEqual(parsed.compounds, jsonc.parse(original).compounds);
    assert.deepEqual(parsed.configurations[1], addition);
    assert.throws(() => addConfiguration(result, addition), /already exists/);
    assert.throws(() => addConfiguration('{invalid', addition), /JSONC/);
});

test('ST project remains on the ST adapter and preserves build, symbols and device information', () => {
    const base = { name: 'ST', type: 'stlinkgdbtarget', request: 'launch', preBuild: '${command:st.build}',
        preLaunchTask: 'Configure', cwd: '${workspaceFolder}', gdb: '${command:st.gdb}',
        deviceName: 'STM32F103C8', deviceCore: 'Cortex-M3', runEntry: 'main',
        program: '${command:st.elf}', serverPort: '61234', serverFrequency: 8000,
        imagesAndSymbols: [{ imageFileName: '${command:st.elf}', symbolFileName: '${command:st.symbols}', symbolOffset: '0' }],
        liveWatch: { enabled: true } };
    const { config, warnings } = createProbeConfiguration(base);
    assert.equal(config.type, 'stgdbtarget');
    assert.equal(config.preBuild, base.preBuild);
    assert.equal(config.gdb, base.gdb);
    assert.equal(config.deviceName, base.deviceName);
    assert(!('serverFrequency' in config));
    assert.deepEqual(warnings, ['liveWatch']);
    assert.equal(flashImage(config), '${command:st.elf}');
    const runtime = runtimeConfiguration(config, 23456, 180, 'id');
    assert.equal(runtime.request, 'attach');
    assert.equal(config.request, 'launch');
    assert.equal(runtime.serverPort, '23456');
    assert.deepEqual(runtime.imagesAndSymbols, [{ symbolFileName: '${command:st.symbols}', symbolOffset: '0' }]);
    assert.deepEqual(base.liveWatch, { enabled: true });
});

test('minimal ST-LINK snippets inherit the ST project commands for GDB and program', () => {
    const config = createProbeConfiguration({ name: 'ST default', type: 'stlinkgdbtarget', request: 'launch',
        imagesAndSymbols: [{ imageFileName: '${command:st-stm32-ide-debug-launch.get-projects-binary-from-context1}' }] }).config;
    assert.equal(config.gdb, '${command:st-stm32-ide-debug-launch.get-gdb-executable}');
    assert.equal(config.program, '${command:st-stm32-ide-debug-launch.get-projects-binary-from-context1}');
});

test('attach and explicit no-load launch do not program flash; extra images and offsets fail closed', () => {
    assert.equal(flashImage({ type: 'cortex-debug', request: 'attach', executable: 'a.elf' }), undefined);
    assert.equal(flashImage({ type: 'cortex-debug', request: 'launch', executable: 'a.elf', loadFiles: [] }), undefined);
    assert.equal(flashImage({ type: 'stgdbtarget', request: 'launch', program: 'a.elf', imagesAndSymbols: [] }), undefined);
    assert.equal(flashImage({ type: 'cortex-debug', request: 'launch', executable: 'symbols.elf', loadFiles: ['program.elf'] }), 'program.elf');
    assert.throws(() => flashImage({ type: 'stgdbtarget', request: 'launch', imagesAndSymbols: [{imageFileName:'a.elf',imageOffset:'0x8000'}] }), /offset/);
    assert.throws(() => flashImage({ type: 'cortex-debug', request: 'launch', executable: 'a.elf', loadFiles: ['boot.elf','a.elf'] }), /standalone/);
});

test('probe-specific user startup hooks require review instead of being lost or executed on a different server', () => {
    const base = { name: 'Debug', type: 'cortex-debug', request: 'launch' };
    assert.throws(() => createProbeConfiguration({ ...base, preLaunchCommands: ['monitor reset halt'] }), /Review preLaunchCommands/);
    assert.throws(() => createProbeConfiguration({ ...base, overrideLaunchCommands: ['load'] }), /Review overrideLaunchCommands/);
    assert.throws(() => createProbeConfiguration({ ...base, postLaunchCommands: ['restore backup.bin binary 0x8000000'] }), /Review/);
    const config = createProbeConfiguration({ ...base, postLaunchCommands: ['print app.run'] }).config;
    assert.deepEqual(config.postLaunchCommands, ['print app.run']);
    assert.doesNotThrow(() => validateProbeConfiguration(config));
    assert.throws(() => validateProbeConfiguration({ ...config, postAttachCommands: ['monitor reset halt'] }), /Review/);
    assert.throws(() => validateProbeConfiguration({ ...config, liveWatch: { enabled: true } }), /single-connection/);
});
