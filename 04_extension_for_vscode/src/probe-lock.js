/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
/** OS-owned cross-window lease. The kernel releases it when an extension host exits. */
const net = require('node:net');
const crypto = require('node:crypto');

async function acquireProbeLock(config, namespace = 'magic-probe') {
    const endpoint = `${config.host.toLowerCase()}:${config.port}`;
    const hash = crypto.createHash('sha256').update(`${namespace}:${endpoint}`).digest('hex');
    const address = process.platform === 'win32' ? `\\\\.\\pipe\\magic-probe-${hash}` :
        process.platform === 'linux' ? `\0magic-probe-${hash}` :
            { host: '127.0.0.1', port: 20000 + Number.parseInt(hash.slice(0, 8), 16) % 20000, exclusive: true };
    const server = net.createServer(socket => socket.destroy());
    await new Promise((resolve, reject) => {
        server.once('error', error => reject(new Error(
            `Probe ${endpoint} is owned by another MAGIC operation or VS Code window (${error.code}).`)));
        server.listen(address, resolve);
    });
    server.on('error', () => {});
    server.unref();
    let released = false;
    return async () => {
        if (released) { return; }
        released = true;
        await new Promise(resolve => server.close(resolve));
    };
}

module.exports = { acquireProbeLock };
