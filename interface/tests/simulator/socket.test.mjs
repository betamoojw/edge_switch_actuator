import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import ts from 'typescript';
import msgpack from 'msgpack-lite';

function harness() {
	let now = 0,
		id = 0;
	const timers = new Map();
	const instances = [];
	class Socket {
		static OPEN = 1;
		readyState = 0;
		sent = [];
		constructor() {
			instances.push(this);
		}
		open() {
			this.readyState = 1;
			this.onopen?.({});
		}
		close() {
			this.readyState = 3;
			this.onclose?.({});
		}
		send(data) {
			this.sent.push(JSON.parse(data));
		}
		receive(payload) {
			this.onmessage?.({ data: JSON.stringify(payload) });
		}
	}
	const exports = {};
	vm.runInNewContext(
		ts.transpileModule(
			readFileSync(new URL('../../src/lib/stores/socket.ts', import.meta.url), 'utf8'),
			{
				compilerOptions: {
					module: ts.ModuleKind.CommonJS,
					target: ts.ScriptTarget.ES2022,
					esModuleInterop: true
				}
			}
		).outputText,
		{
			exports,
			WebSocket: Socket,
			ArrayBuffer,
			Uint8Array,
			console,
			setTimeout: (fn, ms) => {
				timers.set(++id, { at: now + ms, fn });
				return id;
			},
			clearTimeout: (key) => timers.delete(key),
			require: (name) =>
				name === 'msgpack-lite'
					? msgpack
					: {
							writable: () => ({
								subscribe() {
									return () => {};
								},
								set() {}
							})
						}
		}
	);
	function advance(ms) {
		const end = now + ms;
		for (;;) {
			const next = [...timers].sort((a, b) => a[1].at - b[1].at)[0];
			if (!next || next[1].at > end) break;
			now = next[1].at;
			timers.delete(next[0]);
			next[1].fn();
		}
		now = end;
	}
	const client = exports.createWebSocket();
	client.init('ws://test', true);
	instances[0].open();
	return { client, instances, advance };
}
test('quiet connection survives old two-second timeout and explicit heartbeat', () => {
	const { client, instances, advance } = harness();
	const ws = instances[0];
	ws.receive({ event: 'rssi', data: {} });
	advance(14000);
	assert.equal(ws.readyState, 1);
	advance(1000);
	assert.equal(ws.sent.at(-1).event, 'ping');
	ws.receive({ event: 'pong' });
	advance(14000);
	assert.equal(instances.length, 1);
	assert.equal(ws.readyState, 1);
	client.stop();
	advance(60000);
	assert.equal(instances.length, 1);
});
test('missed heartbeat reconnects once and stale socket callbacks cannot close its replacement', () => {
	const { client, instances, advance } = harness();
	advance(45000);
	assert.equal(instances[0].readyState, 3);
	advance(1000);
	assert.equal(instances.length, 2);
	instances[1].open();
	instances[0].onerror({});
	instances[0].onclose({});
	advance(1000);
	assert.equal(instances[1].readyState, 1);
	assert.equal(instances.length, 2);
	client.stop();
});

test('slow but live device has time to answer a heartbeat', () => {
	const { client, instances, advance } = harness();
	const ws = instances[0];
	advance(15000);
	advance(20000);
	assert.equal(ws.readyState, 1);
	ws.receive({ event: 'pong' });
	advance(14000);
	assert.equal(instances.length, 1);
	assert.equal(ws.readyState, 1);
	client.stop();
});
test('repeated init and subscriptions keep one connection and remove last subscriber', () => {
	const { client, instances } = harness();
	const ws = instances[0];
	client.init('ws://test', true);
	assert.equal(instances.length, 1);
	const offA = client.on('device.state', () => {});
	const offB = client.on('device.state', () => {});
	assert.equal(ws.sent.filter((x) => x.event === 'subscribe').length, 1);
	offA();
	assert.equal(ws.sent.filter((x) => x.event === 'unsubscribe').length, 0);
	offB();
	assert.equal(ws.sent.filter((x) => x.event === 'unsubscribe').length, 1);
	client.on('device.state', () => {});
	assert.equal(ws.sent.filter((x) => x.event === 'subscribe').length, 2);
	client.on('json', () => {});
	assert.ok(!ws.sent.some((x) => x.data === 'json'));
	client.stop();
});

test('failed reconnect attempts report one outage and retain backoff until a pong', () => {
	const { client, instances, advance } = harness();
	let lost = 0;
	client.on('close', () => lost++);
	instances[0].close();
	advance(1000);
	instances[1].open();
	instances[1].close();
	assert.equal(lost, 1);
	advance(1000);
	assert.equal(instances.length, 2);
	advance(1000);
	instances[2].open();
	instances[2].receive({ event: 'pong' });
	instances[2].close();
	assert.equal(lost, 2);
	client.stop();
});
