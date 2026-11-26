/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** @file host.js @brief Native named-launch/F5 integration in an isolated VS Code profile. */
const vscode = require('vscode');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');

async function waitFor(predicate, message, milliseconds = 30000) {
    const deadline = Date.now() + milliseconds;
    while (Date.now() < deadline) {
        if (predicate()) { return; }
        await new Promise(resolve => setTimeout(resolve, 100));
    }
    throw new Error(message);
}

async function run() {
    const report = { trusted: vscode.workspace.isTrusted, version: vscode.version, backends: [] };
    const reportFile = process.env.MAGIC_PROBE_TEST_REPORT;
    const progress = setInterval(() => {
        void fs.writeFile(`${reportFile}.progress`, JSON.stringify({ step: report.step,
            lastRequests: report.lastRequests, recentOutput: report.adapterOutput?.slice(-12) }, null, 2));
    }, 1000);
    let controller, active, stopped = 0, stopReason;
    const subscriptions = [vscode.debug.registerDebugAdapterTrackerFactory('*', {
        createDebugAdapterTracker: () => ({ onWillReceiveMessage: message => {
            (report.lastRequests ??= []).push(message.command);
            report.lastRequests = report.lastRequests.slice(-12);
        }, onDidSendMessage: message => {
            if (message.event === 'stopped') { stopped++; stopReason = message.body.reason; }
            if (message.event === 'output') { (report.adapterOutput ??= []).push(message.body); }
        } })
    })];
    try {
        report.dependencies = vscode.extensions.all.filter(item => /^(marus25|mcu-debug|stmicroelectronics)\./i.test(item.id))
            .map(item => ({ id: item.id, version: item.packageJSON.version }));
        assert(vscode.workspace.isTrusted, 'This host suite requires the reviewed trusted test workspace; use -TrustedTestWorkspace.');
        const extension = vscode.extensions.getExtension('magic-probe.magic-debugger-tool');
        assert(extension, 'Extension must be discovered by VS Code');
        controller = await extension.activate();
        report.extensionPath = extension.extensionPath;
        if (process.env.MAGIC_PROBE_TEST_PACKAGE_ROOT) {
            const installed = path.relative(process.env.MAGIC_PROBE_TEST_PACKAGE_ROOT, extension.extensionPath);
            assert(!installed.startsWith('..') && !path.isAbsolute(installed), 'Packaged suite must load the installed VSIX, not workspace source');
        }
        report.activated = extension.isActive;
        const commands = await vscode.commands.getCommands(true);
        for (const name of ['configure', 'useProbe', 'startProjectDebug', 'checkConnection', 'scanTarget', 'flashElf']) {
            assert(commands.includes(`magicProbe.${name}`), name);
        }
        assert(controller.tree.getChildren().length >= 5);
        report.commandsAndSidebar = true;
        if (process.env.MAGIC_PROBE_TEST_HOST) {
            const backend = process.env.MAGIC_PROBE_TEST_BACKEND;
            if (backend === 'st') { assert(!vscode.extensions.getExtension('marus25.cortex-debug'), 'ST-only profile must not contain Cortex-Debug'); }
            if (backend === 'cortex') { assert(!vscode.extensions.getExtension('stmicroelectronics.stm32cube-ide-debug-generic-gdbserver')); }
            const folder = vscode.workspace.workspaceFolders[0];
            const directory = folder.uri.fsPath;
            const config = vscode.workspace.getConfiguration('magicProbe', folder.uri);
            await config.update('host', process.env.MAGIC_PROBE_TEST_HOST, vscode.ConfigurationTarget.WorkspaceFolder);
            await config.update('gdbPath', process.env.MAGIC_PROBE_TEST_GDB || '', vscode.ConfigurationTarget.WorkspaceFolder);
            const { resolveGdb } = require('../src/config');
            const gdb = resolveGdb(process.env.MAGIC_PROBE_TEST_GDB);
            const elf = process.env.MAGIC_PROBE_TEST_ELF;
            const buildDir = path.join(directory, 'build');
            const builtElf = path.join(buildDir, 'project.elf');
            const marker = path.join(buildDir, 'build.log');
            await fs.mkdir(buildDir, { recursive: true });
            await fs.mkdir(path.join(directory, '.vscode'), { recursive: true });
            await fs.copyFile(elf, builtElf);
            await fs.writeFile(marker, '');
            subscriptions.push(vscode.commands.registerCommand('magicProbeTest.preBuild', async () => {
                await fs.copyFile(elf, builtElf);
                await fs.appendFile(marker, 'build\n');
                return '';
            }), vscode.commands.registerCommand('magicProbeTest.elf', () => builtElf),
            vscode.commands.registerCommand('magicProbeTest.gdb', () => gdb));
            const tasks = vscode.workspace.getConfiguration('tasks', folder.uri);
            await tasks.update('version', '2.0.0', vscode.ConfigurationTarget.WorkspaceFolder);
            await tasks.update('tasks', [{ label: 'Test project build', type: 'process',
                command: process.env.MAGIC_PROBE_TEST_NODE,
                args: ['-e', 'const fs=require("node:fs"); fs.copyFileSync(process.argv[1],process.argv[2]); fs.appendFileSync(process.argv[3],"build\\n");', elf, builtElf, marker],
                problemMatcher: [] }], vscode.ConfigurationTarget.WorkspaceFolder);
            report.connection = (await controller.checkConnection()).endpoint;
            report.targets = await controller.scan();
            report.target = await controller.inspect();
            const source = path.resolve(elf, '../../../examples/stm32f103_pc13_blink/main.c');
            const breakpoint = new vscode.SourceBreakpoint(new vscode.Location(vscode.Uri.file(source), new vscode.Position(110, 0)));
            vscode.debug.addBreakpoints([breakpoint]);
            const types = backend === 'both' ? ['cortex-debug', 'stgdbtarget'] : [backend === 'st' ? 'stgdbtarget' : 'cortex-debug'];
            let lastProfile;
            for (const type of types) {
                for (const request of process.env.MAGIC_PROBE_TEST_FLASH === '1' ? ['attach', 'launch'] : ['attach']) {
                    const base = type === 'cortex-debug' ? {
                        name: `Project ${type} ${request}`, type, request, executable: '${command:magicProbeTest.elf}',
                        cwd: '${workspaceFolder}', gdbPath: gdb, armToolchainPath: path.dirname(gdb),
                        servertype: 'openocd', configFiles: ['interface/stlink.cfg', 'target/stm32f1x.cfg'],
                        preLaunchTask: 'Test project build'
                    } : {
                        name: `Project ST ${request}`, type: 'stlinkgdbtarget', request,
                        program: '${command:magicProbeTest.elf}', cwd: '${workspaceFolder}', gdb: '${command:magicProbeTest.gdb}',
                        deviceName: 'STM32F103C8', deviceCore: 'Cortex-M3',
                        imagesAndSymbols: [{ imageFileName: '${command:magicProbeTest.elf}', symbolFileName: '${command:magicProbeTest.elf}' }],
                        runEntry: request === 'launch' ? 'main' : '', preBuild: '${command:magicProbeTest.preBuild}'
                    };
                    const launchUri = vscode.Uri.joinPath(folder.uri, '.vscode', 'launch.json');
                    report.step = `${type}/${request}: setup`;
                    const initial = `// wired configuration stays intact\n${JSON.stringify({ version: '0.2.0', configurations: [base] }, null, 4)}\n`;
                    try { await vscode.workspace.fs.stat(launchUri); }
                    catch { await vscode.workspace.fs.writeFile(launchUri, Buffer.from(initial)); }
                    const launchDocument = await vscode.workspace.openTextDocument(launchUri);
                    const fixtureEdit = new vscode.WorkspaceEdit();
                    fixtureEdit.replace(launchUri, new vscode.Range(launchDocument.positionAt(0),
                        launchDocument.positionAt(launchDocument.getText().length)), initial);
                    assert(await vscode.workspace.applyEdit(fixtureEdit));
                    assert(await launchDocument.save());
                    await waitFor(() => vscode.workspace.getConfiguration('launch', folder.uri).get('configurations', [])[0]?.name === base.name,
                        'Native launch configuration cache did not update');
                    await config.update('configurations', [], vscode.ConfigurationTarget.WorkspaceFolder);
                    const profile = await controller.useProbe({ folder, config: base });
                    lastProfile = profile;
                    assert.equal(profile.type, type);
                    const updated = await fs.readFile(launchUri.fsPath, 'utf8');
                    assert(updated.includes('// wired configuration stays intact'));
                    const wired = vscode.workspace.getConfiguration('launch', folder.uri).get('configurations').find(item => item.name === base.name);
                    assert.deepEqual(wired, base);
                    assert.equal(await controller.resolveDebug(folder, base), base, 'unmanaged wired config must pass through untouched');
                    const beforeBuild = (await fs.readFile(marker, 'utf8')).split('\n').filter(Boolean).length;
                    const backupRoot = path.join(controller.context.globalStorageUri.fsPath, 'backups');
                    const backupsBefore = await fs.readdir(backupRoot).catch(() => []);
                    const outputStart = report.adapterOutput?.length ?? 0;
                    if (request === 'launch') { await fs.writeFile(builtElf, 'stale output before the project build'); }
                    report.step = `${type}/${request}: start native named configuration`;
                    assert.equal(await vscode.debug.startDebugging(folder, profile.name), true, 'same named configuration path as F5');
                    await waitFor(() => controller.session && controller.gateways.get(controller.session.configuration.__magicProbeGateway)?.gateway.ready &&
                        ['Running', 'Stopped'].includes(controller.model.status), 'Native debugger did not start', 60000);
                    active = controller.session;
                    if (controller.model.status === 'Running') { await controller.control('pause'); }
                    await waitFor(() => controller.model.status === 'Stopped', 'Pause after startup failed');
                    const readFrame = async () => {
                        for (let attempt = 0; attempt < 50; attempt++) {
                            const threads = await active.customRequest('threads');
                            if (threads.threads.length) {
                                const stack = await active.customRequest('stackTrace', { threadId: threads.threads[0].id, levels: 3 });
                                if (stack.stackFrames.length) {
                                    const evaluate = await active.customRequest('evaluate', { expression: 'blink_ready', context: 'watch', frameId: stack.stackFrames[0].id });
                                    return { stack, evaluate };
                                }
                            }
                            await new Promise(resolve => setTimeout(resolve, 100));
                        }
                        throw new Error('Native debugger did not expose a real stack frame after startup.');
                    };
                    let values = await readFrame();
                    report.step = `${type}/${request}: read variables`;
                    if (!/\b1\b/.test(values.evaluate.result)) {
                        const count = stopped;
                        await controller.control('continue');
                        await waitFor(() => stopped > count, 'Program did not reach the source breakpoint');
                        values = await readFrame();
                    }
                    assert.match(values.evaluate.result, /\b1\b/);
                    assert(values.stack.stackFrames[0].source.path.toLowerCase().endsWith('main.c'));
                    await assert.rejects(controller.scan(), /Disconnect/);
                    const beforeStep = stopped;
                    report.step = `${type}/${request}: next`;
                    await controller.control('next');
                    await waitFor(() => stopped > beforeStep, 'Native Step Over did not stop');
                    report.step = `${type}/${request}: step into and out of SysTick_Handler`;
                    await active.customRequest('setBreakpoints', { source: { path: source }, breakpoints: [] });
                    await active.customRequest('setFunctionBreakpoints', { breakpoints: [{ name: 'SysTick_Handler' }] });
                    const beforeHandler = stopped;
                    await controller.control('continue');
                    await waitFor(() => stopped > beforeHandler, 'Function breakpoint in SysTick_Handler did not stop');
                    assert((await readFrame()).stack.stackFrames.some(frame => frame.name.includes('SysTick_Handler')));
                    const beforeInto = stopped;
                    await controller.control('stepIn');
                    await waitFor(() => stopped > beforeInto, 'Native Step Into did not stop');
                    const beforeOut = stopped;
                    await controller.control('stepOut');
                    await waitFor(() => stopped > beforeOut, 'Native Step Out did not stop');
                    await active.customRequest('setFunctionBreakpoints', { breakpoints: [] });
                    await active.customRequest('setBreakpoints', { source: { path: source }, breakpoints: [{ line: 111 }] });
                    const resetCount = stopped;
                    report.step = `${type}/${request}: reset`;
                    // Cortex-Debug 1.12.1 performs restart but omits a successful DAP response.
                    // Observe the real breakpoint event rather than waiting for that response.
                    if (type === 'cortex-debug') { void active.customRequest('restart', {}).catch(() => {}); }
                    else { await active.customRequest('adapter/resetTarget', {}); await controller.control('continue'); }
                    await waitFor(() => stopped > resetCount && stopReason === 'breakpoint', 'Native reset/restart did not reach the source breakpoint');
                    assert.match((await readFrame()).evaluate.result, /\b1\b/);
                    const builds = (await fs.readFile(marker, 'utf8')).split('\n').filter(Boolean).length;
                    assert.equal(builds, beforeBuild + 1, 'native build task must execute exactly once');
                    assert(!(report.adapterOutput ?? []).slice(outputStart).some(item => /Loading section/.test(item.output)), 'native adapter must not download the ELF a second time');
                    const backupsAfter = await fs.readdir(backupRoot).catch(() => []);
                    const added = backupsAfter.filter(item => !backupsBefore.includes(item));
                    let backupPath;
                    if (request === 'launch') {
                        assert.equal(added.length, 1, 'one launch must create exactly one full backup');
                        backupPath = path.join(backupRoot, added[0], 'backup.bin');
                        assert.equal((await fs.stat(backupPath)).size, 65536);
                        const { parseElf, verifyReadback } = require('../src/elf');
                        verifyReadback(parseElf(await fs.readFile(builtElf)), await fs.readFile(path.join(backupRoot, added[0], 'readback.bin')));
                    } else { assert.equal(added.length, 0, 'attach must not invoke the programming workflow'); }
                    report.backends.push({ type, request, ...values, buildOnce: true, stepOver: true, stepIntoOut: true, nativeReset: true, backupPath });
                    await controller.disconnect();
                    report.step = `${type}/${request}: disconnect`;
                    active = undefined;
                    await waitFor(() => !controller.session && controller.gateways.size === 0, 'Debugger did not release gateway');
                }
            }
            vscode.debug.removeBreakpoints([breakpoint]);
            report.step = 'Abort a failed project build before GDB starts';
            await vscode.workspace.getConfiguration('debug').update('onTaskErrors', 'abort', vscode.ConfigurationTarget.Workspace);
            await tasks.update('tasks', [...tasks.get('tasks'), { label: 'Test failing build', type: 'process',
                command: process.env.MAGIC_PROBE_TEST_NODE, args: ['-e', 'process.exit(7)'], problemMatcher: [] }], vscode.ConfigurationTarget.WorkspaceFolder);
            const failed = { ...lastProfile, name: 'MAGIC failed-build test', preLaunchTask: 'Test failing build', request: 'attach' };
            delete failed.preBuild;
            const launchConfig = vscode.workspace.getConfiguration('launch', folder.uri);
            await launchConfig.update('configurations', [...launchConfig.get('configurations'), failed], vscode.ConfigurationTarget.WorkspaceFolder);
            await config.update('configurations', [...config.get('configurations'), failed.name], vscode.ConfigurationTarget.WorkspaceFolder);
            assert.equal(await vscode.debug.startDebugging(folder, failed.name), false);
            await waitFor(() => controller.gateways.size === 0, 'A failed preLaunchTask left a pending gateway');
            report.failedBuildCleanup = true;
            const otherFolder = vscode.workspace.workspaceFolders[1];
            assert(otherFolder, 'Use the multi-root host runner');
            const otherSettings = vscode.workspace.getConfiguration('magicProbe', otherFolder.uri);
            await otherSettings.update('host', 'other-probe.invalid', vscode.ConfigurationTarget.WorkspaceFolder);
            const scoped = { name: 'Folder scope check', type: types[0], request: 'attach', servertype: 'external' };
            await otherSettings.update('configurations', [scoped.name], vscode.ConfigurationTarget.WorkspaceFolder);
            assert.equal(await controller.resolveDebug(folder, scoped), scoped, 'same name in an unrelated folder must not be intercepted');
            const resolved = await controller.resolveDebug(otherFolder, scoped);
            assert(resolved.__magicProbeGateway);
            assert.equal(controller.gateways.get(resolved.__magicProbeGateway).gateway.config.host, 'other-probe.invalid');
            await controller.finishGateway(resolved.__magicProbeGateway);
            report.folderScope = true;
            report.disconnected = true;
        }
        report.passed = true;
    } catch (error) {
        report.error = error.stack ?? String(error);
        throw error;
    } finally {
        clearInterval(progress);
        if (reportFile) {
            await fs.writeFile(`${reportFile}.tmp`, JSON.stringify(report, null, 2));
            await fs.rename(`${reportFile}.tmp`, reportFile);
        }
        if (controller?.session) { await controller.disconnect(); }
        report.magicOutput = controller?.output ? 'See MAGIC Probe Output and retained GDB job logs.' : undefined;
        subscriptions.forEach(item => item.dispose());
    }
}
module.exports = { run };
