/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** @file probe-service.js @brief PC-side probe workflows, exclusive ownership and flash backups. */
const fs = require('node:fs/promises');
const path = require('node:path');
const { validateSettings, requireFile } = require('./config');
const { parseElf, validateF103Image, verifyReadback } = require('./elf');
const workflows = require('./workflows');
const { runGdb } = require('./gdb');

class ProbeService {
    constructor({ storage, onOutput = () => {}, onBusy = () => {}, runner = runGdb }) {
        this.storage = storage;
        this.onOutput = onOutput;
        this.onBusy = onBusy;
        this.runner = runner;
        this.busy = undefined;
        this.abortController = undefined;
    }

    async exclusive(label, action) {
        if (this.busy) { throw new Error(`Probe is busy: ${this.busy}. Wait for it to finish.`); }
        this.busy = label;
        this.abortController = new AbortController();
        this.onBusy(label);
        try { return await action(this.abortController.signal); }
        finally {
            this.busy = undefined;
            this.abortController = undefined;
            this.onBusy(undefined);
        }
    }

    cancel() { this.abortController?.abort(); }

    async jobDirectory() {
        const parent = path.join(this.storage, 'jobs');
        await fs.mkdir(parent, { recursive: true });
        return fs.mkdtemp(path.join(parent, 'probe-'));
    }

    settings(options) {
        const config = validateSettings(options);
        const gdbPath = requireFile(options.gdbPath);
        return { ...config, gdbPath };
    }

    async execute(config, script, signal, directory) {
        const folder = directory ?? await this.jobDirectory();
        this.onOutput(`\n--- ${config.host}:${config.port} / ${this.busy} ---\n`);
        return this.runner({ executable: config.gdbPath, script, directory: folder,
            timeoutMs: config.operationTimeoutSeconds * 1000, signal, onOutput: this.onOutput });
    }

    checkConnection(options) {
        return this.exclusive('Checking connection', async signal => {
            const config = this.settings(options);
            const result = await this.execute(config, workflows.checkCommands(config), signal);
            if (!result.output.includes('MAGIC_CONNECTION_OK') || !result.output.includes('Black Magic Probe')) {
                throw new Error('The endpoint did not identify as a Black Magic Debug probe.');
            }
            return { endpoint: `${config.host}:${config.port}`, output: result.output };
        });
    }

    scan(options) {
        return this.exclusive('Scanning SWD', async signal => {
            const config = this.settings(options);
            const result = await this.execute(config, workflows.scanCommands(config), signal);
            return workflows.parseScan(result.output);
        });
    }

    async inspectInternal(config, signal) {
        const result = await this.execute(config, workflows.inspectCommands(config), signal);
        return workflows.parseInspection(result.output);
    }

    inspect(options) {
        return this.exclusive('Inspecting F103', signal => this.inspectInternal(this.settings(options), signal));
    }

    prepareFlash(options, elfPath) {
        return this.exclusive('Preparing flash', async signal => {
            const config = this.settings(options);
            requireFile(elfPath);
            const image = parseElf(await fs.readFile(elfPath));
            const target = await this.inspectInternal(config, signal);
            validateF103Image(image, target);
            return { elfPath, image, target, config };
        });
    }

    flash(plan) {
        return this.exclusive('Flashing ELF', async signal => {
            const config = this.settings(plan.config);
            const image = parseElf(await fs.readFile(plan.elfPath));
            validateF103Image(image, plan.target);
            if (image.sha256 !== plan.image.sha256) { throw new Error('ELF changed after review; select it again.'); }
            const backupFolder = path.join(this.storage, 'backups');
            await fs.mkdir(backupFolder, { recursive: true });
            const stamp = new Date().toISOString().replaceAll(':', '-');
            // GDB's dump command treats quotes as part of its filename. Run this
            // job inside a permanent backup directory and use safe relative names.
            const directory = await fs.mkdtemp(path.join(backupFolder, `stm32-before-${stamp}-`));
            const files = {
                elf: path.join(directory, 'image.elf'),
                backup: path.join(directory, 'backup.bin'),
                readback: path.join(directory, 'readback.bin')
            };
            // Load an immutable snapshot rather than a file another build can replace during programming.
            await fs.writeFile(files.elf, image.buffer, { mode: 0o600 });
            this.onOutput(`Backup path: ${files.backup}\n`);
            try {
                const result = await this.execute(config,
                    workflows.flashCommands(config, image, plan.target, files), signal, directory);
                if (!result.output.includes('MAGIC_BACKUP_OK') || !result.output.includes('MAGIC_FLASH_DONE')) {
                    throw new Error('Flash workflow did not complete. See Output and the backup path.');
                }
                if ((await fs.stat(files.backup)).size !== plan.target.flashKiB * 1024) {
                    throw new Error('Target backup size is incorrect.');
                }
                verifyReadback(image, await fs.readFile(files.readback));
                return { backupPath: files.backup, loadBytes: image.loadBytes, sha256: image.sha256,
                    directory, target: plan.target };
            } catch (error) {
                error.message += ` Backup location: ${files.backup}`;
                throw error;
            }
        });
    }
}

module.exports = { ProbeService };
