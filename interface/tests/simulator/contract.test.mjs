import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { once } from 'node:events';
import { WebSocket } from 'ws';
import msgpack from 'msgpack-lite';
import { startSimulator } from '../../simulator/server.mjs';
import { Device } from '../../simulator/device.mjs';
import { defaults, validateConfig } from '../../simulator/profiles.mjs';
import { deviceTarget } from '../../scripts/target.mjs';

async function setup(t, options = {}) {
	const sim = await startSimulator({
		port: 0,
		controlPort: 0,
		controlToken: 'test-control',
		...options
	});
	t.after(() => sim.close());
	const base = `http://127.0.0.1:${sim.apiPort}`;
	async function request(path, body, token, method = body === undefined ? 'GET' : 'POST') {
		return fetch(`${base}/rest/${path}`, {
			method,
			headers: {
				...(token ? { Authorization: `Bearer ${token}` } : {}),
				'Content-Type': 'application/json'
			},
			body: body === undefined ? undefined : JSON.stringify(body)
		});
	}
	async function login(username = 'admin') {
		const r = await request('signIn', { username, password: `sim-${username}` });
		assert.equal(r.status, 200);
		return (await r.json()).access_token;
	}
	return { ...sim, base, request, login };
}
function admin(d) {
	return d.saved.users[0];
}
function command(d, body, user = admin(d)) {
	return d.execute('device/commands', body, user);
}
function save(d, update = {}) {
	return d.execute('device/config', { ...structuredClone(d.config), ...update }, admin(d));
}

test('target validation does not silently select a hardware host', () => {
	assert.deepEqual(deviceTarget('192.0.2.1:8080'), {
		http: 'http://192.0.2.1:8080',
		ws: 'ws://192.0.2.1:8080'
	});
	assert.equal(deviceTarget('https://device.example').ws, 'wss://device.example');
	for (const input of [
		undefined,
		'http://user:pass@device',
		'http://device/path',
		'http://device?token=a',
		'ftp://device'
	])
		assert.throws(() => deviceTarget(input));
});

test('firmware config boundary vectors and indexing', () => {
	const good = defaults().config;
	assert.equal(validateConfig(good), '');
	for (const [key, value, error] of [
		['schemaVersion', 2, 'Invalid schema/revision'],
		['revision', -1, 'Invalid schema/revision'],
		['baud', 9600, 'Invalid interface or indicator settings'],
		['tcpPort', 80, 'Port 80 is reserved for management'],
		['mode', 'knx', 'Invalid protocol mode'],
		['tcpAllow', '999.1.1.1', 'TCP source must be an IPv4 address or empty']
	])
		assert.equal(validateConfig({ ...good, [key]: value }), error);
	assert.equal(
		validateConfig({ ...good, mode: 'modbus_rtu', rs485: false }),
		'Enable RS485 before selecting RTU'
	);
	const bad = structuredClone(good);
	bad.clicks[0] = { action: 1, target: 0 };
	assert.equal(validateConfig(bad), 'Button action requires enabled channel');
});

test('auth wire responses, masks, redaction and revocation', async (t) => {
	const s = await setup(t);
	assert.equal((await s.request('features')).status, 200);
	assert.equal((await s.request('device/status')).status, 401);
	assert.equal((await s.request('signIn', { username: 'admin', password: 'bad' })).status, 401);
	const token = await s.login(),
		operator = await s.login('operator');
	assert.equal((await s.request('verifyAuthorization', undefined, token)).status, 200);
	assert.equal((await s.request('wifiSettings', undefined, operator)).status, 401);
	assert.equal(
		(await s.request('device/commands', { command: 'relay', channel: 1, value: true }, operator))
			.status,
		403
	);
	assert.equal(
		(await s.request('device/commands', { command: 'relay', channel: 0, value: true }, operator))
			.status,
		200
	);
	assert.equal((await s.request('device/config', s.device.config, operator)).status, 403);
	const users = await (await s.request('securitySettings', undefined, token)).json();
	assert.equal(users.jwt_secret, '');
	assert.ok(users.users.every((u) => u.password === ''));
	assert.equal((await s.request('securitySettings', { users: [] }, token)).status, 400);
	assert.equal((await s.request('securitySettings', users, token)).status, 200);
	assert.equal((await s.request('verifyAuthorization', undefined, token)).status, 401);
	const fresh = await s.login();
	s.device.advance(8 * 60 * 60 * 1000);
	assert.equal((await s.request('verifyAuthorization', undefined, fresh)).status, 401);
});

test('relay pulse, channel blocking, all-off atomicity and idempotency', () => {
	let time = 1000;
	const d = new Device({ clock: () => time });
	const operator = d.saved.users.find((u) => u.role === 'operator');
	const request = { command: 'pulse', channel: 0, requestId: 'pulse-once' };
	assert.equal(command(d, request).status, 200);
	time += 500;
	assert.equal(command(d, request).status, 200);
	time += 501;
	d.tick();
	assert.equal(d.snapshot().relays[0].on, false, 'retry must not extend pulse');
	assert.equal(command(d, { ...request, channel: 1 }).status, 409);
	command(d, { command: 'relay', channel: 0, value: true });
	assert.equal(command(d, { command: 'all_off' }, operator).status, 403);
	assert.equal(d.snapshot().relays[0].on, true);
	d.runtime.blocks[0] = true;
	assert.equal(command(d, { command: 'relay', channel: 0, value: false }).status, 409);
	assert.equal(command(d, { command: 'unblock', channel: 0 }, operator).status, 403);
	assert.equal(command(d, { command: 'unblock', channel: 0 }).status, 200);
	assert.equal(command(d, { command: 'all_off' }).status, 200);
	assert.ok(d.snapshot().relays.every((r) => !r.on));
});

test('durable profile, stale revision and rollback, reboot and factory reset', (t) => {
	const dir = mkdtempSync(join(tmpdir(), 'actuator-sim-'));
	t.after(() => rmSync(dir, { recursive: true, force: true }));
	const options = { stateFile: join(dir, 'state.json') };
	const d = new Device(options);
	const old = structuredClone(d.config);
	const next = structuredClone(old);
	next.relays[0].startup = true;
	next.relays[0].name = 'Saved';
	assert.equal(save(d, next).status, 200);
	assert.equal(save(d, old).status, 409);
	d.failures.persistence = true;
	assert.equal(save(d, { mode: 'modbus_tcp' }).status, 409);
	assert.equal(d.config.mode, 'off');
	d.failures.protocol = true;
	assert.equal(save(d, { mode: 'knx_ip' }).status, 409);
	assert.equal(d.config.revision, 2);
	const restored = new Device(options);
	assert.equal(restored.config.relays[0].name, 'Saved');
	assert.equal(restored.snapshot().relays[0].on, true);
	const token = restored.auth.token(admin(restored));
	restored.reboot();
	assert.equal(restored.auth.authenticate(token, restored.saved.users), null);
	restored.reset();
	assert.equal(new Device(options).config.relays[0].name, 'Channel 1');
});

test('KNX commissioning boundaries, ownership, independent revision and object effects', () => {
	const d = new Device();
	assert.equal(save(d, { mode: 'knx_ip' }).status, 200);
	const k = d.knxSnapshot();
	k.address = '1.1.10';
	assert.equal(
		d.execute('knx/config', k, admin(d)).body.error,
		'Explicit web takeover is required'
	);
	k.takeover = true;
	k.objects[0].groups = ['1/0/1', '1/0/1'];
	assert.equal(
		d.execute('knx/config', k, admin(d)).body.error,
		'Reserved or duplicate group address'
	);
	k.objects[0].groups = ['1/0/1'];
	assert.equal(d.execute('knx/config', k, admin(d)).status, 200);
	assert.equal(d.knxSnapshot().revision, 2);
	assert.equal(d.config.revision, 2);
	assert.equal(d.execute('knx/config', k, admin(d)).status, 409);
	d.action({ type: 'knx-object', number: 1, value: true });
	assert.equal(d.snapshot().relays[0].source, 'knx');
	d.action({ type: 'knx-object', number: 2, value: true });
	assert.equal(command(d, { command: 'relay', channel: 0, value: false }).status, 409);
	const cfg = structuredClone(d.config);
	cfg.relays[0].pulseMs++;
	assert.equal(save(d, cfg).body.error, 'Edit commissioned relay parameters in the KNX section');
	d.action({ type: 'network', wifi: false, ap: true });
	assert.equal(d.snapshot().protocolState, 'waiting_network');
	assert.equal(d.execute('knx/programming', { active: true }, admin(d)).status, 409);
});

test('watchdog, network loss, indicators, gestures and configuration window timers', () => {
	const d = new Device();
	const config = structuredClone(d.config);
	Object.assign(config, { mode: 'modbus_rtu', busWatchdog: true });
	config.relays[0].disconnectOff = true;
	config.relays[0].timeout = 1;
	config.clicks[0] = { action: 3, target: 1 };
	save(d, config);
	d.action({ type: 'gesture', clicks: 1 });
	assert.equal(d.snapshot().relays[0].on, true);
	d.advance(1001);
	assert.equal(d.snapshot().relays[0].source, 'disconnect policy');
	command(d, { command: 'tone', hz: 1000, ms: 100 });
	assert.equal(d.snapshot().toneHz, 1000);
	d.advance(101);
	assert.equal(d.snapshot().toneHz, 0);
	command(d, { command: 'modbus_window', seconds: 60 });
	assert.equal(d.snapshot().configWindow, true);
	d.advance(60001);
	assert.equal(d.snapshot().configWindow, false);
	save(d, { mode: 'modbus_tcp' });
	d.action({ type: 'modbus', channel: 0, value: true, clients: 2 });
	d.action({ type: 'network', wifi: false, ap: true });
	d.advance(1001);
	assert.equal(d.snapshot().relays[0].on, false);
	assert.equal(d.snapshot().networkOnline, false);
});

test('framework settings, async scan, binary download and profile absence', async (t) => {
	const s = await setup(t),
		token = await s.login();
	for (const path of ['wifiSettings', 'apSettings', 'mqttSettings', 'ntpSettings']) {
		const body = await (await s.request(path, undefined, token)).json();
		assert.deepEqual(await (await s.request(path, body, token)).json(), body);
	}
	assert.equal((await s.request('scanNetworks', undefined, token)).status, 202);
	assert.equal((await s.request('listNetworks', undefined, token)).status, 202);
	s.device.advance(1001);
	assert.equal(
		(await (await s.request('listNetworks', undefined, token)).json()).networks.length,
		1
	);
	assert.equal((await s.request('coreDump', undefined, token)).status, 500);
	s.device.action({ type: 'coredump', available: true });
	assert.equal(
		(await s.request('coreDump', undefined, token)).headers.get('content-type'),
		'application/octet-stream'
	);
	for (const path of ['brokerSettings', 'lightState', 'ethernetStatus', 'sleep'])
		assert.match(
			(await s.request(path, undefined, token)).headers.get('content-type'),
			/text\/html/
		);
	assert.equal((await s.request('device/config', [], token)).status, 400);
	assert.equal(
		(await s.request('device/commands', { command: 'identify', requestId: 'x'.repeat(65) }, token))
			.status,
		422
	);
});

for (const profile of ['template', 'json'])
	test(`WebSocket ${profile} codec, subscriptions and disconnect`, async (t) => {
		const s = await setup(t, { profile }),
			token = await s.login();
		const ws = new WebSocket(`ws://127.0.0.1:${s.apiPort}/ws/events?access_token=${token}`);
		t.after(() => ws.terminate());
		await once(ws, 'open');
		const event = profile === 'template' ? 'led' : 'features';
		const message = once(ws, 'message');
		ws.send(
			profile === 'json'
				? JSON.stringify({ event: 'subscribe', data: event })
				: msgpack.encode({ event: 'subscribe', data: event })
		);
		const [bytes, binary] = await message;
		assert.equal(binary, profile !== 'json');
		assert.equal((binary ? msgpack.decode(bytes) : JSON.parse(bytes)).event, event);
		const closed = once(ws, 'close');
		s.device.reboot();
		await closed;
	});

test('control isolation, one-shot faults and held-response release', async (t) => {
	const s = await setup(t),
		token = await s.login();
	const control = `http://127.0.0.1:${s.controlPort}`;
	const post = (path, body) =>
		fetch(control + path, {
			method: 'POST',
			headers: { Authorization: 'Bearer test-control', 'Content-Type': 'application/json' },
			body: JSON.stringify(body)
		});
	assert.equal((await fetch(control + '/__sim/state')).status, 403);
	assert.equal((await fetch(s.base + '/__sim/state')).status, 404);
	assert.equal(
		(await post('/__sim/faults', { path: '/rest/device/status', effect: 'http', status: 503 }))
			.status,
		200
	);
	assert.equal((await s.request('device/status', undefined, token)).status, 503);
	assert.equal((await s.request('device/status', undefined, token)).status, 200);
	await post('/__sim/faults', { path: '/rest/device/status', effect: 'hold' });
	const pending = s.request('device/status', undefined, token);
	// A separate GET confirms the server has processed the held request first.
	await s.request('features');
	await post('/__sim/faults', { clear: true });
	assert.equal((await pending).status, 503);
});

test('multipart firmware MD5, wrong-image errors and simulated OTA', async (t) => {
	const s = await setup(t),
		token = await s.login();
	const send = (name, bytes) => {
		const form = new FormData();
		form.append('file', new Blob([bytes]), name);
		return fetch(s.base + '/rest/uploadFirmware', {
			method: 'POST',
			headers: { Authorization: `Bearer ${token}` },
			body: form
		});
	};
	assert.equal((await send('bad.md5', 'short')).status, 422);
	assert.equal((await send('bad.txt', 'x')).status, 406);
	assert.equal((await send('bad.bin', Buffer.alloc(24))).status, 503);
	const bin = Buffer.alloc(100);
	bin[0] = 0xe9;
	bin.writeUInt16LE(9, 12);
	assert.equal((await send('firmware.bin', bin)).status, 200);
	assert.equal(s.device.runtime.ota.status, 'preparing');
	s.device.advance(2600);
	assert.equal(s.device.runtime.ota.status, 'finished');
});
