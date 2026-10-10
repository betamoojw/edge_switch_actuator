import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startSimulator } from '../../simulator/server.mjs';
import { Device } from '../../simulator/device.mjs';
import { mcpRoute, mcpStatus } from '../../simulator/xiaozhi-mcp.mjs';

const edit = (revision = 1) => ({
	revision,
	enabled: true,
	alias: '机房',
	channel_mask: 5,
	endpoint: 'wss://example.invalid/mcp/?token=private-test'
});
test('MCP HTTP authorization, secret redaction, revisions and rollback', async (t) => {
	const sim = await startSimulator({ port: 0, controlPort: 0, controlToken: 'test-control' });
	t.after(() => sim.close());
	const base = `http://127.0.0.1:${sim.apiPort}/rest/`;
	const request = (path, token, body) =>
		fetch(base + path, {
			method: body ? 'POST' : 'GET',
			headers: {
				'Content-Type': 'application/json',
				...(token ? { Authorization: `Bearer ${token}` } : {})
			},
			body: body ? JSON.stringify(body) : undefined
		});
	const login = async (username) =>
		(await (await request('signIn', '', { username, password: `sim-${username}` })).json())
			.access_token;
	const admin = await login('admin'),
		viewer = await login('viewer');
	assert.equal((await request('xiaozhiMcpSettings')).status, 401);
	assert.equal((await request('xiaozhiMcpSettings', viewer)).status, 403);
	assert.equal((await request('xiaozhiMcpStatus', viewer)).status, 200);
	const initial = await (await request('xiaozhiMcpSettings', admin)).json();
	assert.equal(initial.enabled, false);
	assert.equal(initial.endpoint_configured, false);
	let saved = await request('xiaozhiMcpSettings', admin, edit());
	assert.equal(saved.status, 200);
	const value = await saved.json();
	assert.equal(value.revision, 2);
	assert.equal(value.endpoint_configured, true);
	assert.equal((await request('xiaozhiMcpEndpoint', '', { revision: 2 })).status, 401);
	assert.equal((await request('xiaozhiMcpEndpoint', viewer, { revision: 2 })).status, 403);
	assert.equal((await request('xiaozhiMcpEndpoint', admin)).status, 405);
	assert.equal((await request('xiaozhiMcpEndpoint', admin, { revision: 1 })).status, 409);
	assert.equal((await request('xiaozhiMcpEndpoint', admin, { revision: '2' })).status, 422);
	const revealed = await request('xiaozhiMcpEndpoint', admin, { revision: 2 });
	assert.equal(revealed.headers.get('cache-control'), 'no-store');
	assert.deepEqual(await revealed.json(), { revision: 2, endpoint: edit().endpoint });
	assert.deepEqual(await (await request('xiaozhiMcpSettings', admin)).json(), value);
	for (const path of ['xiaozhiMcpSettings', 'xiaozhiMcpStatus', 'xiaozhiMcpTools'])
		assert(!JSON.stringify(await (await request(path, admin)).json()).includes('private-test'));
	assert.equal((await request('xiaozhiMcpSettings', admin, edit())).status, 409);
	assert.equal((await request('xiaozhiMcpSettings', viewer, edit(2))).status, 403);
	saved = await request('xiaozhiMcpSettings', admin, { ...edit(2), endpoint: '' });
	assert.equal((await saved.json()).revision, 2);
	assert.equal((await request('xiaozhiMcpReconnect', admin, {})).status, 202);
	assert.equal(
		(await request('xiaozhiMcpSettings', admin, { ...edit(2), endpoint: '', clear_endpoint: true }))
			.status,
		422
	);
	saved = await request('xiaozhiMcpSettings', admin, {
		...edit(2),
		enabled: false,
		endpoint: '',
		clear_endpoint: true
	});
	assert.equal((await saved.json()).endpoint_configured, false);
	assert.equal(
		(await (await request('xiaozhiMcpEndpoint', admin, { revision: 3 })).json()).endpoint,
		''
	);
});

test('MCP deterministic states, persistence failure, reboot and reset', () => {
	const d = new Device();
	const user = d.saved.users[0];
	const save = (body) => mcpRoute(d, 'xiaozhiMcpSettings', 'POST', body, user);
	d.failures.persistence = true;
	assert.equal(save(edit()).status, 503);
	assert.equal(d.saved.settings.xiaozhiMcpSettings.enabled, false);
	assert.equal(save(edit()).status, 200);
	assert.equal(mcpStatus(d).state, 'connecting');
	d.action({ type: 'mcp', state: 'ready' });
	assert.equal(mcpStatus(d).ready, true);
	d.runtime.network.wifi = false;
	assert.equal(mcpStatus(d).state, 'waiting_network');
	d.runtime.network.wifi = true;
	d.saved.settings.ntpSettings.enabled = false;
	assert.equal(mcpStatus(d).state, 'waiting_time');
	d.reboot(0);
	assert.equal(d.saved.settings.xiaozhiMcpSettings.endpoint.includes('private-test'), true);
	d.reset();
	assert.equal(mcpStatus(d).state, 'disabled');
	assert.equal(d.saved.settings.xiaozhiMcpSettings.endpoint, '');
	for (const profile of ['template', 'security-off', 'mcp-off']) {
		const device = new Device({ profile });
		assert.equal(device.features.xiaozhi_mcp, false);
		assert.equal(
			mcpRoute(device, 'xiaozhiMcpStatus', 'GET', undefined, device.saved.users[0]),
			null
		);
	}
});

test('MCP endpoint and settings boundary vectors', () => {
	for (const changes of [
		{ endpoint: 'ws://a/' },
		{ endpoint: 'wss://user@a/' },
		{ endpoint: 'wss://a:0/' },
		{ endpoint: 'wss://a/#x' },
		{ endpoint: null },
		{ alias: '\n' },
		{ alias: '测'.repeat(22) },
		{ channel_mask: 64 },
		{ enabled: 'true' },
		{ clear_endpoint: 'true' },
		{ unexpected: true }
	]) {
		const d = new Device();
		assert.equal(
			mcpRoute(d, 'xiaozhiMcpSettings', 'POST', { ...edit(), ...changes }, d.saved.users[0]).status,
			422,
			JSON.stringify(changes)
		);
	}
});
