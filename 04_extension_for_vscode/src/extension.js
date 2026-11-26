/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** @file extension.js @brief Integrate the MAGIC probe into the project's native debugger. */
const vscode = require('vscode');
const path = require('node:path');
const fs = require('node:fs/promises');
const crypto = require('node:crypto');
const { validateSettings, resolveFile, requireFile, resolveGdb } = require('./config');
const { parseElf } = require('./elf');
const { ProbeService } = require('./probe-service');
const { ProbeGateway } = require('./gateway');
const { acquireProbeLock } = require('./probe-lock');
const { addConfiguration } = require('./launch-file');
const { SUPPORTED_TYPES, NATIVE_TYPES, createProbeConfiguration, flashImage,
    runtimeConfiguration, projectGdb, validateProbeConfiguration } = require('./project-debug');
let activeController;

class ProbeControls {
    constructor(model) {
        this.model = model;
        this.changed = new vscode.EventEmitter();
        this.onDidChangeTreeData = this.changed.event;
    }
    refresh() { this.changed.fire(); }
    getTreeItem(item) { return item; }
    getChildren(element) {
        if (element?.children) { return element.children; }
        if (element) { return []; }
        const item = (label, command, icon) => {
            const row = new vscode.TreeItem(label);
            if (command) { row.command = { command: `magicProbe.${command}`, title: label }; }
            if (icon) { row.iconPath = new vscode.ThemeIcon(icon); }
            return row;
        };
        const group = (name, children) => {
            const row = new vscode.TreeItem(name, vscode.TreeItemCollapsibleState.Expanded);
            row.children = children;
            return row;
        };
        const endpoint = item(this.model.endpoint, 'configure', 'plug');
        endpoint.description = this.model.status;
        endpoint.tooltip = this.model.error ?? 'Configure the probe address and port.';
        const elf = item('Standalone flash ELF', 'selectElf', 'file-binary');
        elf.description = this.model.elf ? path.basename(this.model.elf) : 'Select an ELF';
        elf.tooltip = this.model.elf ?? 'Only for standalone programming; F5 uses the project ELF.';
        return [endpoint, item('Use MAGIC Wi-Fi probe in project', 'useProbe', 'settings-gear'),
            item('Start project debugger (F5)', 'startProjectDebug', 'debug-start'),
            item(this.model.target ?? 'Target not scanned', undefined, 'circuit-board'),
            group('Probe', [item('Check connection', 'checkConnection', 'plug'),
                item('Scan SWD targets', 'scanTarget', 'search'), item('Inspect F103', 'inspectTarget', 'info')]),
            group('Tools', [item('Output', 'showOutput', 'output'), item('Settings', 'openSettings', 'settings-gear'),
                elf, item('Standalone F103 flash and verify', 'flashElf', 'cloud-upload')])];
    }
    dispose() { this.changed.dispose(); }
}

class Controller {
    constructor(context) {
        this.context = context;
        this.output = vscode.window.createOutputChannel('MAGIC Probe');
        this.model = { endpoint: 'Probe', status: 'Idle' };
        this.tree = new ProbeControls(this.model);
        this.status = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 20);
        this.status.command = 'magicProbe.openControls';
        this.session = undefined;
        this.threadId = 1;
        this.operation = undefined;
        this.gateways = new Map();
        this.service = new ProbeService({ storage: context.globalStorageUri.fsPath,
            onOutput: text => this.output.append(text), onBusy: () => this.refresh() });
        context.subscriptions.push(this.output, this.tree, this.status,
            vscode.window.registerTreeDataProvider('magicProbe.controls', this.tree),
            vscode.workspace.onDidChangeConfiguration(event => {
                if (event.affectsConfiguration('magicProbe')) { this.refresh(); }
            }),
            vscode.debug.onDidStartDebugSession(session => {
                const record = this.gateways.get(session.configuration.__magicProbeGateway);
                if (record) { record.session = session; this.session = session; this.threadId = 1; this.refresh(); }
            }),
            vscode.debug.onDidTerminateDebugSession(session => {
                void this.finishGateway(session.configuration.__magicProbeGateway);
                if (this.session?.id === session.id) { this.session = undefined; this.model.status = 'Idle'; this.refresh(); }
            }));
        for (const type of NATIVE_TYPES) {
            context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider(type, {
                resolveDebugConfigurationWithSubstitutedVariables: (folder, configuration) => this.resolveDebug(folder, configuration)
            }), vscode.debug.registerDebugAdapterTrackerFactory(type, {
                createDebugAdapterTracker: session => {
                    const id = session.configuration.__magicProbeGateway;
                    if (!this.gateways.has(id)) { return undefined; }
                    return { onDidSendMessage: message => {
                        if (!this.gateways.get(id)?.gateway.ready) { return; }
                        if (message.type === 'event' && Number.isInteger(message.body?.threadId)) {
                            this.threadId = message.body.threadId;
                        }
                        if (message.type === 'event' && message.event === 'stopped') {
                            this.model.status = 'Stopped'; this.refresh();
                        } else if (message.type === 'event' && message.event === 'continued') {
                            this.model.status = 'Running'; this.refresh();
                        }
                    }, onError: () => { void this.finishGateway(id); },
                    onExit: () => { void this.finishGateway(id); } };
                }
            }));
        }
        this.registerCommands();
        this.refresh();
    }

    folder() {
        if (this.session?.workspaceFolder) { return this.session.workspaceFolder; }
        const editor = vscode.window.activeTextEditor;
        return (editor && vscode.workspace.getWorkspaceFolder(editor.document.uri)) ?? vscode.workspace.workspaceFolders?.[0];
    }
    rawSettings(folder = this.folder()) {
        const configuration = vscode.workspace.getConfiguration('magicProbe', folder?.uri);
        return Object.fromEntries(['host', 'port', 'clockHz', 'targetIndex', 'connectUnderReset',
            'remoteTimeoutSeconds', 'operationTimeoutSeconds', 'gdbPath', 'elfPath'].map(key => [key, configuration.get(key)]));
    }
    settings() {
        const raw = this.rawSettings();
        return { ...validateSettings(raw), gdbPath: resolveGdb(raw.gdbPath, this.folder()?.uri.fsPath) };
    }
    refresh() {
        const raw = this.rawSettings();
        this.model.endpoint = `${raw.host ?? '192.168.4.1'}:${raw.port ?? 2022}`;
        this.model.elf = raw.elfPath || undefined;
        const pending = [...this.gateways.values()].some(record => !record.gateway.ready);
        const busy = this.operation ?? this.service.busy ?? (pending ? 'Starting debugger' : undefined);
        this.status.text = busy ? '$(sync~spin) MAGIC Probe' : '$(circuit-board) MAGIC Probe';
        this.status.tooltip = `${this.model.endpoint} — ${busy ?? this.model.status}`;
        this.status.show();
        void vscode.commands.executeCommand('setContext', 'magicProbe.busy', Boolean(busy));
        void vscode.commands.executeCommand('setContext', 'magicProbe.debugActive', Boolean(this.session));
        this.tree.refresh();
    }
    assertTrusted() {
        if (!vscode.workspace.isTrusted) { throw new Error('Trust this workspace before running ARM GDB.'); }
    }
    assertIdle() {
        this.assertTrusted();
        if (this.operation || this.service.busy) { throw new Error('Another MAGIC Probe operation is active.'); }
        if (this.session) { throw new Error('Disconnect the debugger before starting a separate probe operation.'); }
        if (this.gateways.size) { throw new Error('A project debugger is starting. Stop it before a separate probe operation.'); }
        const other = vscode.debug.activeDebugSession;
        if (other?.configuration.gdbTarget === this.model.endpoint) {
            throw new Error('An existing debugger is using this probe endpoint. Disconnect it first.');
        }
    }
    async operationRun(label, action) {
        this.assertIdle();
        this.operation = label;
        this.cancelRequested = false;
        this.model.error = undefined;
        this.model.status = label;
        this.refresh();
        let release;
        try {
            release = await acquireProbeLock(validateSettings(this.rawSettings()));
            return await vscode.window.withProgress({ location: vscode.ProgressLocation.Notification,
                title: `MAGIC Probe: ${label}`, cancellable: true }, async (_, token) => {
                const cancellation = token.onCancellationRequested(() => {
                    this.cancelRequested = true;
                    this.service.cancel();
                });
                try { return await action(); }
                finally { cancellation.dispose(); }
            });
        } finally {
            await release?.();
            this.operation = undefined;
            if (!this.session) { this.model.status = 'Idle'; }
            this.refresh();
        }
    }
    async updateSetting(key, value) {
        const folder = this.folder();
        await vscode.workspace.getConfiguration('magicProbe', folder?.uri).update(key, value,
            folder ? vscode.ConfigurationTarget.WorkspaceFolder : vscode.ConfigurationTarget.Global);
    }
    assertNotCancelled() {
        if (this.cancelRequested) { throw new Error('Operation cancelled.'); }
    }
    async selectElf() {
        this.assertTrusted();
        const selected = await vscode.window.showOpenDialog({ canSelectMany: false,
            title: 'Select STM32 ELF', filters: { 'STM32 ELF': ['elf', 'axf'] } });
        if (!selected?.length) { return undefined; }
        parseElf(await fs.readFile(selected[0].fsPath));
        await this.updateSetting('elfPath', selected[0].fsPath);
        this.refresh();
        return selected[0].fsPath;
    }
    async elf() {
        const raw = this.rawSettings();
        const file = raw.elfPath ? resolveFile(raw.elfPath, this.folder()?.uri.fsPath) : await this.selectElf();
        if (!file) { return undefined; }
        requireFile(file);
        parseElf(await fs.readFile(file));
        return file;
    }
    async configure() {
        this.assertIdle();
        const raw = this.rawSettings();
        const host = await vscode.window.showInputBox({ title: 'Probe IPv4 address or hostname', value: raw.host,
            validateInput: text => { try { validateSettings({ ...raw, host: text }); return undefined; } catch (e) { return e.message; } } });
        if (host === undefined) { return; }
        const port = await vscode.window.showInputBox({ title: 'GDB TCP port', value: String(raw.port ?? 2022),
            validateInput: text => /^\d+$/.test(text) && Number(text) > 0 && Number(text) <= 65535 ? undefined : 'Use a TCP port from 1 to 65535.' });
        if (port === undefined) { return; }
        await this.updateSetting('host', host);
        await this.updateSetting('port', Number(port));
        this.refresh();
    }
    checkConnection() {
        return this.operationRun('Check connection', async () => {
            const result = await this.service.checkConnection(this.settings());
            this.output.show(true);
            void vscode.window.showInformationMessage(`MAGIC Probe connected: ${result.endpoint}`);
            return result;
        });
    }
    scan() {
        return this.operationRun('Scan SWD', async () => {
            const targets = await this.service.scan(this.settings());
            this.model.target = targets.map(target => `${target.index}: ${target.driver}`).join(', ');
            this.output.show(true);
            return targets;
        });
    }
    inspect() {
        return this.operationRun('Inspect F103', async () => {
            const target = await this.service.inspect(this.settings());
            this.model.target = `ID 0x${target.deviceId.toString(16)} / ${target.flashKiB} KiB`;
            this.output.show(true);
            return target;
        });
    }
    attach() { return this.startProjectDebug(); }
    flash() {
        return this.operationRun('Flash ELF', async () => {
            const elf = await this.elf();
            if (!elf) { return undefined; }
            this.assertNotCancelled();
            const plan = await this.service.prepareFlash(this.settings(), elf);
            this.assertNotCancelled();
            const choice = await vscode.window.showWarningMessage(
                `Program ${path.basename(elf)} (${plan.image.loadBytes} bytes) to ${plan.config.host}:${plan.config.port}?`,
                { modal: true, detail: `F103 flash signature: ${plan.target.flashKiB} KiB. The existing application will be overwritten. A full flash backup is saved before writing.` },
                'Flash and verify');
            if (choice !== 'Flash and verify') { return undefined; }
            this.assertNotCancelled();
            const result = await this.service.flash(plan);
            this.output.show(true);
            void vscode.window.showInformationMessage(`Programmed and verified ${result.loadBytes} bytes. Backup: ${result.backupPath}`);
            return result;
        });
    }
    projectConfigurations(managedOnly = false) {
        return (vscode.workspace.workspaceFolders ?? []).flatMap(folder => {
            const configurations = vscode.workspace.getConfiguration('launch', folder.uri).get('configurations', []);
            const managed = vscode.workspace.getConfiguration('magicProbe', folder.uri).get('configurations', []);
            return configurations.filter(config => SUPPORTED_TYPES.includes(config.type) &&
                (managedOnly ? managed.includes(config.name) : !managed.includes(config.name)))
                .map(config => ({ label: config.name, description: folder.name,
                    detail: `${config.type} / ${config.request}`, folder, config }));
        });
    }
    async useProbe(selection) {
        this.assertIdle();
        const options = this.projectConfigurations();
        if (!options.length) { throw new Error('Create a standard Cortex-Debug or STM32CubeIDE launch.json configuration first. MAGIC will inherit that project configuration.'); }
        const selected = selection ?? await vscode.window.showQuickPick(options, { title: 'Choose the project debug configuration to use with MAGIC Wi-Fi' });
        if (!selected) { return; }
        const { config, warnings } = createProbeConfiguration(selected.config);
        const dependency = config.type === 'cortex-debug' ? 'marus25.cortex-debug' : 'stmicroelectronics.stm32cube-ide-debug-generic-gdbserver';
        if (!vscode.extensions.getExtension(dependency)) { throw new Error(`Install/enable ${dependency} in this workspace's profile before using this configuration.`); }
        const uri = vscode.Uri.joinPath(selected.folder.uri, '.vscode', 'launch.json');
        try { await vscode.workspace.fs.stat(uri); }
        catch (error) {
            if (error.code !== 'FileNotFound') { throw error; }
            await vscode.workspace.fs.createDirectory(vscode.Uri.joinPath(selected.folder.uri, '.vscode'));
            await vscode.workspace.fs.writeFile(uri, Buffer.from('{\n    "version": "0.2.0",\n    "configurations": []\n}\n'));
        }
        const document = await vscode.workspace.openTextDocument(uri);
        const text = document.getText();
        const next = addConfiguration(text, config, { eol: document.eol === vscode.EndOfLine.CRLF ? '\r\n' : '\n' });
        const edit = new vscode.WorkspaceEdit();
        edit.replace(uri, new vscode.Range(document.positionAt(0), document.positionAt(text.length)), next);
        if (!await vscode.workspace.applyEdit(edit) || !await document.save()) { throw new Error('Could not save the MAGIC project debug configuration.'); }
        const settings = vscode.workspace.getConfiguration('magicProbe', selected.folder.uri);
        await settings.update('configurations', [...new Set([...settings.get('configurations', []), config.name])], vscode.ConfigurationTarget.WorkspaceFolder);
        this.output.appendLine(`Created '${config.name}' using ${config.type}. The original '${selected.config.name}' remains available.`);
        if (warnings.length) { this.output.appendLine(`Features unavailable on this single-connection probe: ${warnings.join(', ')}. Disabled only in the MAGIC profile.`); }
        this.output.appendLine(config.request === 'launch' ?
            'F5 launch programs one validated standalone F103 ELF with full backup and readback verification, then starts the native debugger.' :
            'F5 attach reads project symbols and attaches without programming flash.');
        this.output.show(true);
        void vscode.window.showInformationMessage(`Select '${config.name}' in Run and Debug, then press F5. MAGIC uses the project's ${config.type} debugger. Live Watch, SWO/RTT and RTOS proxy are unavailable on this probe.`);
        return config;
    }
    async startProjectDebug() {
        this.assertIdle();
        const options = this.projectConfigurations(true);
        if (!options.length) { throw new Error('Run MAGIC Probe: Use MAGIC Wi-Fi Probe in Project first, then use Run and Debug / F5.'); }
        const selected = options.length === 1 ? options[0] : await vscode.window.showQuickPick(options, { title: 'Start a project debug configuration' });
        if (!selected) { return false; }
        try {
            const started = await vscode.debug.startDebugging(selected.folder, selected.config.name);
            if (!started) { await this.closePendingGateways(); }
            return started;
        } catch (error) { await this.closePendingGateways(); throw error; }
    }
    async closePendingGateways() {
        for (const [id, record] of this.gateways) { if (!record.session) { await this.finishGateway(id); } }
    }
    async finishGateway(id) {
        const record = this.gateways.get(id);
        if (!record) { return; }
        this.gateways.delete(id);
        await record.gateway.close();
        if (!this.session && !this.gateways.size) { this.model.status = 'Idle'; }
        this.refresh();
    }
    async resolveDebug(folder, configuration) {
        const names = vscode.workspace.getConfiguration('magicProbe', folder?.uri).get('configurations', []);
        if (!names.includes(configuration.name)) { return configuration; }
        let id;
        try {
            this.assertTrusted();
            await this.closePendingGateways();
            if (this.operation || this.service.busy || this.gateways.size) { throw new Error('Another MAGIC operation or project debug session owns the probe.'); }
            validateProbeConfiguration(configuration);
            const raw = this.rawSettings(folder);
            const settings = validateSettings(raw);
            const file = flashImage(configuration);
            const cwd = configuration.cwd ? resolveFile(configuration.cwd, folder?.uri.fsPath) : folder?.uri.fsPath;
            const image = file ? resolveFile(file, cwd) : undefined;
            const gdb = image ? resolveGdb(projectGdb(configuration) || raw.gdbPath, cwd) : undefined;
            id = crypto.randomUUID();
            const record = { folder, configuration, gateway: undefined };
            const gateway = new ProbeGateway(settings, {
                reset: configuration.request === 'launch',
                onOutput: text => this.output.append(text),
                onFailure: error => {
                    this.model.error = error.message;
                    void vscode.window.showErrorMessage(`MAGIC Probe: ${error.message}`);
                    if (record.session) { void vscode.debug.stopDebugging(record.session); }
                    void this.finishGateway(id);
                },
                beforeConnect: async signal => {
                    if (!image) { return; }
                    const cancel = () => this.service.cancel();
                    signal.addEventListener('abort', cancel, { once: true });
                    try {
                        if (signal.aborted) { throw new Error('Project launch cancelled.'); }
                        this.output.appendLine(`\nProject build finished. Preparing ${image} for ${settings.host}:${settings.port}.`);
                        const plan = await this.service.prepareFlash({ ...settings, gdbPath: gdb }, image);
                        if (signal.aborted) { throw new Error('Project launch cancelled before programming.'); }
                        const result = await this.service.flash(plan);
                        this.output.appendLine(`Programmed and verified ${result.loadBytes} bytes. Backup: ${result.backupPath}`);
                        if (parseElf(await fs.readFile(image)).sha256 !== plan.image.sha256) {
                            throw new Error('Project ELF changed during programming. Stop concurrent builds and start a new debug session.');
                        }
                    } finally { signal.removeEventListener('abort', cancel); }
                }
            });
            record.gateway = gateway;
            await gateway.start();
            this.gateways.set(id, record);
            this.model.status = `Starting ${configuration.type}`;
            this.output.appendLine(`\n${configuration.name}: ${configuration.type} → local gateway → ${settings.host}:${settings.port}`);
            this.refresh();
            return runtimeConfiguration(configuration, gateway.port,
                Math.max(settings.remoteTimeoutSeconds, settings.operationTimeoutSeconds * 3), id, settings.remoteTimeoutSeconds);
        } catch (error) {
            if (id) { await this.finishGateway(id); }
            this.output.appendLine(`ERROR: ${error.message}`);
            void vscode.window.showErrorMessage(`MAGIC Probe: ${error.message}`);
            return undefined;
        }
    }
    async control(command) {
        this.assertTrusted();
        if (!this.session) { throw new Error('Start a MAGIC project debug configuration first.'); }
        // Keep pause asynchronous: querying threads first can block behind a running GDB target.
        return this.session.customRequest(command, { threadId: this.threadId });
    }
    async disconnect() {
        if (this.session) { await vscode.debug.stopDebugging(this.session); }
    }
    registerCommands() {
        const commands = {
            configure: () => this.configure(), checkConnection: () => this.checkConnection(),
            scanTarget: () => this.scan(), inspectTarget: () => this.inspect(), selectElf: () => { this.assertIdle(); return this.selectElf(); },
            useProbe: () => this.useProbe(), startProjectDebug: () => this.startProjectDebug(),
            attach: () => this.attach(), flashElf: () => this.flash(), continue: () => this.control('continue'),
            pause: () => this.control('pause'), stepOver: () => this.control('next'),
            stepInto: () => this.control('stepIn'), stepOut: () => this.control('stepOut'),
            disconnect: () => this.disconnect(), showOutput: () => this.output.show(),
            openSettings: () => vscode.commands.executeCommand('workbench.action.openSettings', '@ext:magic-probe.magic-debugger-tool'),
            openControls: () => vscode.commands.executeCommand('workbench.view.extension.magicProbe')
        };
        for (const [name, action] of Object.entries(commands)) {
            this.context.subscriptions.push(vscode.commands.registerCommand(`magicProbe.${name}`, async () => {
                try { return await action(); }
                catch (error) {
                    this.model.error = error.message;
                    this.output.appendLine(`ERROR: ${error.message}`);
                    this.refresh();
                    void vscode.window.showErrorMessage(`MAGIC Probe: ${error.message}`, 'Show Output')
                        .then(choice => { if (choice) { this.output.show(); } });
                    return undefined;
                }
            }));
        }
    }
    async dispose() {
        this.service.cancel();
        await this.disconnect();
        await Promise.all([...this.gateways.keys()].map(id => this.finishGateway(id)));
    }
}

function activate(context) {
    activeController = new Controller(context);
    // The API is also used by the Extension Host smoke test; methods retain the same ownership guards.
    return activeController;
}
async function deactivate() { await activeController?.dispose(); activeController = undefined; }
module.exports = { activate, deactivate };
