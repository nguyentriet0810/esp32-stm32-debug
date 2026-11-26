// SPDX-License-Identifier: GPL-3.0-or-later
/** @file wifi_portal_test.cjs @brief Run the embedded page against a fake DOM/network, without credentials. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const html = fs.readFileSync(path.join(__dirname, '../components/probe_wifi_setup/wifi_portal.html'), 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const elements = new Map();
const element = () => ({
    textContent: '', value: '', disabled: false, children: [],
    addEventListener(event, callback) { this[event] = callback; },
    replaceChildren() { this.children = []; },
    append(child) { this.children.push(child); }
});
const get = id => {
    if (!elements.has(id)) elements.set(id, element());
    return elements.get(id);
};
let state = {ready: true, busy: false, configuring: false, result: 'ESP_OK', connected: false,
    ssid: '', ip: '', ap: 'MAGIC-PROBE', ping: 0, token: 'fixture-token', scan: 0, scan_revision: 0};
let scan = {state: 0, result: 'ESP_OK', networks: []};
let holdPost, postError, offline = false, releasePost, setupEnabled = true;
const posts = [];
const context = vm.createContext({
    document: {getElementById: get, createElement: element}, TextEncoder, URLSearchParams, AbortController,
    setTimeout, clearTimeout, setInterval() {},
    async fetch(url, options) {
        if (offline) throw new Error('offline');
        if (url === '/api/status') return {ok: setupEnabled, async json() {
            return setupEnabled ? {...state} : {error: 'setup_disabled'};
        }};
        if (url === '/api/scan' && options.method !== 'POST') {
            return {ok: true, async json() { return scan; }};
        }
        posts.push({url, options});
        if (holdPost) await new Promise(resolve => { releasePost = resolve; });
        if (postError) return {ok: false, async json() { return {error: postError}; }};
        return {ok: true, async json() { return {accepted: true}; }};
    }
});
const tick = () => new Promise(resolve => setImmediate(resolve));
const run = text => vm.runInContext(text, context);
const submit = () => get('wifi').submit({preventDefault() {}});

/** @brief Exercise the production script, including concurrent poll/POST and retry paths. */
async function main() {
    run(script);
    await tick();
    assert.equal(get('save').disabled, false);
    assert.equal(get('ping').disabled, true);
    assert.equal(get('ap').textContent, 'Probe AP: MAGIC-PROBE');
    assert.match(html, /<html lang="en">/);
    get('ssid').value = 'Cafe & Lab';
    get('password').value = 'demo-only1';
    holdPost = true;
    const saving = submit();
    await tick();
    assert.equal(posts.length, 1);
    assert.equal(posts[0].options.body.toString(), 'ssid=Cafe+%26+Lab&password=demo-only1');
    assert.equal(posts[0].options.headers['X-Probe-Token'], 'fixture-token');
    await run('refresh()');
    assert.equal(get('save').disabled, true, 'poll must not re-enable an in-flight save');
    await submit();
    assert.equal(posts.length, 1, 'duplicate submit must be ignored');
    holdPost = false;
    releasePost();
    await saving;
    assert.equal(get('password').value, '');
    state = {...state, connected: true, ssid: 'Cafe & Lab', ip: '192.168.1.22',
        ping: 2, sent: 3, received: 3, mean_ms: 2, gateway: '192.168.1.1'};
    await run('refresh()');
    assert.equal(get('ping').disabled, false);
    assert.match(get('ping-result').textContent, /3\/3.*2 ms.*192\.168\.1\.1/);
    await get('ping').click();
    assert.equal(posts.at(-1).url, '/api/ping');
    assert.equal(posts.at(-1).options.body, '');
    get('ssid').value = 'é'.repeat(17);
    get('password').value = 'demo-only1';
    const count = posts.length;
    await submit();
    assert.equal(posts.length, count, 'SSID limit is UTF-8 bytes');
    get('ssid').value = 'Lab';
    postError = 'debug_busy';
    await submit();
    assert.match(get('message').textContent, /active debug session/);
    assert.equal(get('password').value, 'demo-only1', 'failed save allows editing/retry');
    offline = true;
    await run('refresh()');
    assert.equal(get('save').disabled, true);
    assert.equal(get('ping').disabled, true);
    offline = false;
    state.busy = true;
    await run('refresh()');
    assert.equal(get('save').disabled, true);
    state.busy = false;
    await run('refresh()');
    assert.equal(get('save').disabled, false);
    postError = undefined;
    await get('scan').click();
    assert.equal(posts.at(-1).url, '/api/scan');
    assert.equal(posts.at(-1).options.body, '');
    state = {...state, busy: true, scan: 1};
    await run('refresh()');
    assert.equal(get('scan').disabled, true);
    assert.equal(get('scan').textContent, 'Scanning…');
    scan = {state: 2, result: 'ESP_OK', networks: [
        {ssid: '<Lab & "wifi">', rssi: -38, secured: true},
        {ssid: 'Open network', rssi: -70, secured: false}
    ]};
    state = {...state, busy: false, scan: 2, scan_revision: 1};
    await run('refresh()');
    assert.equal(get('networks').children.length, 3);
    assert.equal(get('networks').children[1].value, '<Lab & "wifi">');
    assert.match(get('networks').children[1].textContent, /-38 dBm/);
    assert.equal(get('networks').children[2].disabled, true);
    get('networks').value = '<Lab & "wifi">';
    get('networks').change();
    assert.equal(get('ssid').value, '<Lab & "wifi">');
    scan = {state: 3, result: 'ESP_ERR_WIFI_STATE', networks: []};
    state = {...state, scan: 3, scan_revision: 2};
    await run('refresh()');
    assert.match(get('scan-status').textContent, /Scan failed.*ESP_ERR_WIFI_STATE/);
    scan = {state: 2, result: 'ESP_OK', networks: []};
    state = {...state, scan: 2, scan_revision: 3, ping: 3};
    await run('refresh()');
    assert.match(get('scan-status').textContent, /No networks found/);
    assert.match(get('ping-result').textContent, /may block ping/);
    setupEnabled = false;
    await run('refresh()');
    assert.match(get('state').textContent, /setup is OFF/);
    assert.equal(get('save').disabled, true);
    assert.equal(get('scan').disabled, true);
    setupEnabled = true;
    await run('refresh()');
    assert.equal(get('save').disabled, false);
    console.log('PASS: English Wi-Fi page, UTF-8 limits, save/Ping, scan selection/escaping, empty/error/busy results and retry');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
