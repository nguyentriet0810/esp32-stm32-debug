/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** Native debug configurations: project data stays with its existing debug adapter. */
const path = require('node:path');

const SUPPORTED_TYPES = ['cortex-debug', 'stgdbtarget', 'stlinkgdbtarget', 'jlinkgdbtarget'];
const NATIVE_TYPES = ['cortex-debug', 'stgdbtarget'];
const HOOKS = ['preLaunchCommands', 'postLaunchCommands', 'preAttachCommands', 'postAttachCommands',
    'preRestartCommands', 'postRestartCommands', 'preResetCommands', 'postResetCommands',
    'overrideLaunchCommands', 'overrideAttachCommands', 'overrideRestartCommands', 'overrideResetCommands',
    'initCommands', 'preRunCommands', 'quickReset', 'openOCDPreConfigLaunchCommands', 'openOCDLaunchCommands'];

function reviewHooks(base) {
    for (const key of HOOKS) {
        const commands = base[key] ?? [];
        if (!Array.isArray(commands) || commands.some(command => typeof command !== 'string')) {
            throw new Error(`${key} must be a list of GDB commands.`);
        }
        if (commands.length && (/^(override|quickReset|openOCD)/.test(key) || commands.some(command =>
            command.split(/\r?\n/).some(line => /^\s*(?:-?interpreter-exec\s+console\s+["']?)?-?(?:monitor\b|target[ -](?:remote|extended-remote|select|download)\b|(?:attach|load|restore|run)\b)/i.test(line))))) {
            throw new Error(`Review ${key} before selecting MAGIC: it contains startup or programming commands for the old probe. Keep the wired configuration and create a version with portable GDB hooks first.`);
        }
    }
}

function createProbeConfiguration(base, name = `${base.name} (MAGIC Wi-Fi)`) {
    if (!SUPPORTED_TYPES.includes(base.type)) { throw new Error(`Unsupported debugger: ${base.type}. Use Cortex-Debug or STM32CubeIDE for VS Code.`); }
    if (!['launch', 'attach'].includes(base.request)) { throw new Error('Choose a launch or attach configuration.'); }
    reviewHooks(base);
    const config = structuredClone(base);
    config.name = name;
    for (const key of Object.keys(config)) { if (key.startsWith('__')) { delete config[key]; } }
    const warnings = [];
    for (const key of ['liveWatch', 'swoConfig', 'rttConfig', 'serverRtos']) {
        if (config[key]?.enabled) { warnings.push(key); }
    }
    if (base.type === 'cortex-debug') {
        for (const key of ['configFiles', 'searchDir', 'serverpath', 'serverArgs', 'openOCDPreConfigLaunchCommands',
            'openOCDLaunchCommands', 'jlinkscript', 'boardId', 'ipAddress', 'rtos', 'ctiOpenOCDConfig', 'chainedConfigurations']) {
            if (key === 'chainedConfigurations' && config[key]?.enabled) {
                throw new Error('Chained/multi-core debug requires a separate probe integration; choose a single-target configuration.');
            }
            delete config[key];
        }
        Object.assign(config, { servertype: 'external', gdbTarget: '127.0.0.1:2022',
            overrideLaunchCommands: [], overrideAttachCommands: [],
            overrideRestartCommands: ['maintenance packet vRun;', 'maintenance flush register-cache'],
            liveWatch: { enabled: false }, swoConfig: { enabled: false }, rttConfig: { enabled: false } });
    } else {
        for (const key of Object.keys(config)) {
            if (/^(server|stlink|jlink)/i.test(key)) { delete config[key]; }
        }
        Object.assign(config, { type: 'stgdbtarget', serverHost: '127.0.0.1', serverPort: '2022',
            // The runtime uses ST's attach adapter with MAGIC doing the requested launch programming.
            serverExe: 'cube', liveWatch: { enabled: false }, serverRtos: { enabled: false },
            quickReset: ['maintenance packet vRun;', 'maintenance flush register-cache'] });
        // ST-LINK snippets can omit these because ST resolves them from the project.
        // Generic ST snippets expose the same native commands explicitly.
        config.gdb ||= '${command:st-stm32-ide-debug-launch.get-gdb-executable}';
        const lastImage = config.imagesAndSymbols?.findLast(item => item.imageFileName || item.symbolFileName);
        config.program ||= lastImage?.imageFileName || lastImage?.symbolFileName ||
            '${command:st-stm32-ide-debug-launch.get-projects-binary-from-context1}';
    }
    return { config, warnings };
}

function flashImage(config) {
    if (config.request !== 'launch') { return undefined; }
    if (config.type === 'cortex-debug') {
        if (config.loadFiles?.length === 0) { return undefined; }
        if (config.loadFiles && config.loadFiles.length !== 1) {
            throw new Error('MAGIC launch programming supports one standalone F103 ELF, without extra loadFiles.');
        }
        const file = config.loadFiles?.[0] ?? config.executable;
        if (!file) { throw new Error('The project debugger did not resolve an executable ELF.'); }
        return file;
    }
    const images = config.imagesAndSymbols?.filter(item => item.imageFileName);
    if (images && images.length === 0) { return undefined; }
    if (images?.length > 1 || images?.some(item => item.imageOffset && !/^0(?:x0+)?$/.test(item.imageOffset))) {
        throw new Error('MAGIC launch programming supports one standalone F103 ELF without offsets.');
    }
    const file = images?.[0]?.imageFileName ?? config.program;
    if (!file) { throw new Error('The ST debugger did not resolve the project image.'); }
    return file;
}

function runtimeConfiguration(base, port, timeoutSeconds, gatewayId, remoteTimeoutSeconds = 15) {
    const config = structuredClone(base);
    config.__magicProbeGateway = gatewayId;
    const timeout = `set remotetimeout ${timeoutSeconds}`;
    if (config.type === 'cortex-debug') {
        config.gdbTarget = `127.0.0.1:${port}`;
        config.debuggerArgs = [...(config.debuggerArgs ?? []), '-iex', timeout];
        config.overrideLaunchCommands = [];
        config.overrideAttachCommands = [];
        const after = config.request === 'launch' ? 'postLaunchCommands' : 'postAttachCommands';
        config[after] = [`set remotetimeout ${remoteTimeoutSeconds}`, ...(config[after] ?? [])];
    } else {
        config.request = 'attach'; // Do not launch a second server or load the image a second time.
        config.serverHost = '127.0.0.1'; config.serverPort = String(port);
        config.gdbArguments = [...(config.gdbArguments ?? []), '-iex', timeout];
        config.initCommands = [`set remotetimeout ${remoteTimeoutSeconds}`, ...(config.initCommands ?? [])];
        config.imagesAndSymbols = (config.imagesAndSymbols ?? []).map(item => ({
            symbolFileName: item.symbolFileName ?? item.imageFileName,
            symbolOffset: item.symbolOffset ?? item.imageOffset ?? ''
        })).filter(item => item.symbolFileName);
    }
    return config;
}

function projectGdb(config) {
    if (config.type === 'cortex-debug') {
        return config.gdbPath || (config.armToolchainPath ? path.join(config.armToolchainPath,
            `${config.toolchainPrefix || 'arm-none-eabi'}-gdb${process.platform === 'win32' ? '.exe' : ''}`) : undefined);
    }
    return config.gdb;
}

function validateProbeConfiguration(config) {
    if (!NATIVE_TYPES.includes(config.type) || (config.type === 'cortex-debug' && config.servertype !== 'external')) {
        throw new Error('Use a Cortex-Debug external or ST Generic profile created by Use MAGIC Wi-Fi Probe in Project.');
    }
    const copy = structuredClone(config);
    // These are the exact native reset commands supplied by this integration.
    if (JSON.stringify(copy.overrideRestartCommands) === JSON.stringify(['maintenance packet vRun;', 'maintenance flush register-cache'])) { delete copy.overrideRestartCommands; }
    if (JSON.stringify(copy.quickReset) === JSON.stringify(['maintenance packet vRun;', 'maintenance flush register-cache'])) { delete copy.quickReset; }
    reviewHooks(copy);
    for (const key of ['liveWatch', 'swoConfig', 'rttConfig', 'serverRtos', 'chainedConfigurations']) {
        if (config[key]?.enabled) { throw new Error(`${key} is not supported by this single-connection MAGIC probe. Keep it disabled in the MAGIC profile.`); }
    }
}

module.exports = { SUPPORTED_TYPES, NATIVE_TYPES, createProbeConfiguration, flashImage, runtimeConfiguration, projectGdb, validateProbeConfiguration };
