import { createServer } from 'node:http';
import { randomBytes, timingSafeEqual } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import { WebSocketServer, WebSocket } from 'ws';
import msgpack from 'msgpack-lite';
import { Device } from './device.mjs';
import { frameworkRoute, analytics, jsonObject, upload } from './framework.mjs';
import { clone, integer } from './profiles.mjs';

export function reply(res, result = { status: 404 }) {
	if (res.destroyed || res.writableEnded) return;
	res.statusCode = result.status;
	res.setHeader('Cache-Control', 'no-store');
	if (result.raw !== undefined) {
		res.setHeader('Content-Type', result.contentType || 'text/plain');
		res.end(result.raw);
	} else if (result.body !== undefined) {
		res.setHeader('Content-Type', 'application/json');
		res.end(JSON.stringify(result.body));
	} else res.end();
}
async function jsonBody(req, max = 8192) {
	const chunks = [];
	let size = 0;
	for await (const chunk of req) {
		size += chunk.length;
		if (size > max) throw new Error('Body too large');
		chunks.push(chunk);
	}
	if (!size) return undefined;
	return JSON.parse(Buffer.concat(chunks).toString());
}
const listen = (server, port) =>
	new Promise((resolve, reject) => {
		server.once('error', reject);
		server.listen(port, '127.0.0.1', () => {
			server.off('error', reject);
			resolve(server.address().port);
		});
	});
export async function startSimulator({
	port = 3080,
	controlPort = 3081,
	controlToken = randomBytes(24).toString('hex'),
	profile = 'actuator',
	stateFile,
	log = () => {}
} = {}) {
	if (!controlToken) throw new Error('A nonempty control token is required');
	const device = new Device({ profile, stateFile });
	let faults = [],
		paused = false,
		refuse = false;
	const held = new Set(),
		pendingTimers = new Set();
	const wss = new WebSocketServer({ noServer: true, maxPayload: 8192 });
	const authenticate = (req, url) =>
		!device.features.security
			? device.saved.users.find((u) => u.admin)
			: device.auth.authenticate(
					req.headers.authorization
						? req.headers.authorization.startsWith('Bearer ')
							? req.headers.authorization.slice(7)
							: ''
						: url.searchParams.get('access_token') || '',
					device.saved.users
				);
	const eventNames = () =>
		new Set([
			'features',
			'rssi',
			'notification',
			'reconnect',
			...(device.features.analytics ? ['analytics'] : []),
			...(device.features.battery ? ['battery'] : []),
			...(device.features.ethernet ? ['ethernet'] : []),
			...(device.features.download_firmware || device.features.upload_firmware
				? ['otastatus']
				: []),
			device.actuator ? 'device.state' : 'led'
		]);
	const send = (ws, event, data) => {
		if (ws.readyState === WebSocket.OPEN)
			ws.send(
				device.features.event_use_json
					? JSON.stringify({ event, data })
					: msgpack.encode({ event, data })
			);
	};
	const emit = (event, data, origin) => {
		for (const ws of wss.clients)
			if (ws !== origin && ws.subscriptions.has(event)) send(ws, event, data);
	};
	device.on('event', emit);
	device.on('boot', () => {
		for (const ws of wss.clients) ws.close(1012, 'Device reboot');
	});
	wss.on('connection', (ws) => {
		ws.subscriptions = new Set();
		ws.on('error', () => {});
		ws.on('message', (bytes, binary) => {
			if (binary === device.features.event_use_json) return;
			try {
				const { event, data } = binary ? msgpack.decode(bytes) : JSON.parse(bytes.toString());
				if (event === 'subscribe' && eventNames().has(data)) {
					ws.subscriptions.add(data);
					if (data === 'led') send(ws, 'led', { led_on: device.runtime.led_on });
					if (data === 'features') emit('features', device.features);
				} else if (event === 'unsubscribe') ws.subscriptions.delete(data);
				else if (event === 'led' && !device.actuator && jsonObject(data)) {
					device.runtime.led_on = data.led_on === true;
					emit('led', { led_on: device.runtime.led_on }, ws);
				}
			} catch {
				/* Firmware ignores malformed application frames. */
			}
		});
	});
	const api = createServer(async (req, res) => {
		const url = new URL(req.url, 'http://localhost');
		const path = url.pathname.replace(/^\/rest\//, '');
		res.once('finish', () => log(`${req.method} ${url.pathname} ${res.statusCode}`));
		try {
			device.tick();
			if (device.runtime.offlineUntil > device.now()) return reply(res, { status: 503 });
			const fault = faults.find(
				(f) => f.count > 0 && f.path === url.pathname && (!f.method || f.method === req.method)
			);
			if (fault) {
				fault.count--;
				if (fault.effect === 'latency')
					await new Promise((resolve) => {
						const timer = setTimeout(() => {
							pendingTimers.delete(timer);
							resolve();
						}, fault.ms);
						pendingTimers.add(timer);
					});
				if (fault.effect === 'http') return reply(res, { status: fault.status, body: fault.body });
				if (fault.effect === 'malformed')
					return reply(res, { status: 200, raw: '{invalid', contentType: 'application/json' });
				if (fault.effect === 'disconnect') return req.socket.destroy();
				if (fault.effect === 'hold') {
					held.add(res);
					res.on('close', () => held.delete(res));
					return;
				}
			}
			if (!url.pathname.startsWith('/rest/')) return reply(res, { status: 404 });
			const user = authenticate(req, url);
			if (path === 'uploadFirmware' && req.method === 'POST' && device.features.upload_firmware) {
				if (!user?.admin) {
					device.emit('event', 'otastatus', {
						status: 'error',
						error: 'Insufficient permissions to upload firmware'
					});
					return reply(res, { status: 403 });
				}
				return reply(res, await upload(req, device));
			}
			let body;
			if (req.method === 'POST') {
				try {
					body = await jsonBody(req);
				} catch {
					return reply(res, { status: 400 });
				}
			}
			if (
				device.actuator &&
				[
					'device/status',
					'device/config',
					'device/commands',
					'protocol/transition',
					'knx/config',
					'knx/programming'
				].includes(path)
			) {
				if (!user) return reply(res, { status: 401 });
				if (req.method === 'GET')
					return reply(res, {
						status: 200,
						body:
							path === 'device/config'
								? { ...clone(device.config), capabilities: device.capabilities(user) }
								: path === 'knx/config'
									? device.knxSnapshot(user)
									: device.snapshot(user)
					});
				if (req.method === 'POST' && path !== 'device/status') {
					if (!jsonObject(body)) return reply(res, { status: 400 });
					return reply(res, device.execute(path, body, user));
				}
			}
			const result = frameworkRoute(device, path, req.method, body, user, url);
			// Device fallback serves index.html for unregistered paths. A small HTML
			// sentinel preserves the non-JSON failure without bundling a second UI.
			reply(
				res,
				result || {
					status: 200,
					contentType: 'text/html',
					raw: '<!doctype html><title>Unregistered firmware route</title>'
				}
			);
		} catch (error) {
			log(`Simulator request error: ${error.message}`);
			reply(res, { status: 500 });
		}
	});
	api.on('upgrade', (req, socket, head) => {
		const url = new URL(req.url, 'http://localhost');
		if (
			url.pathname !== '/ws/events' ||
			refuse ||
			device.runtime.offlineUntil > device.now() ||
			!authenticate(req, url)
		) {
			socket.end('HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n');
			return;
		}
		wss.handleUpgrade(req, socket, head, (ws) => wss.emit('connection', ws, req));
	});
	const clearFaults = () => {
		faults = [];
		paused = false;
		refuse = false;
		for (const res of held) reply(res, { status: 503 });
		held.clear();
	};
	const control = createServer(async (req, res) => {
		const path = new URL(req.url, 'http://localhost').pathname;
		if (path === '/__sim/health' && req.method === 'GET')
			return reply(res, { status: 200, body: { ready: true, profile: device.saved.profile } });
		const expected = Buffer.from(`Bearer ${controlToken}`),
			actual = Buffer.from(req.headers.authorization || '');
		if (
			req.headers.origin ||
			expected.length !== actual.length ||
			!timingSafeEqual(expected, actual)
		)
			return reply(res, { status: 403 });
		try {
			const body = req.method === 'POST' ? await jsonBody(req, 65536) : undefined;
			if (path === '/__sim/state' && req.method === 'GET')
				return reply(res, {
					status: 200,
					body: {
						profile: device.saved.profile,
						status: device.snapshot(),
						config: device.config,
						knx: device.knxSnapshot(),
						connections: wss.clients.size,
						ota: device.runtime.ota
					}
				});
			if (req.method !== 'POST' || !jsonObject(body)) return reply(res, { status: 400 });
			switch (path) {
				case '/__sim/reset':
					device.reset(body.profile || profile);
					clearFaults();
					break;
				case '/__sim/actions':
					device.action(body);
					break;
				case '/__sim/clock':
					device.advance(body.advanceMs);
					break;
				case '/__sim/faults':
					if (body.clear) {
						clearFaults();
						break;
					}
					if (
						typeof body.path !== 'string' ||
						!body.path.startsWith('/rest/') ||
						!['latency', 'http', 'malformed', 'disconnect', 'hold'].includes(body.effect) ||
						!integer(body.count ?? 1, 1, 10000)
					)
						throw new Error('Invalid fault rule');
					if (body.effect === 'latency' && !integer(body.ms, 0, 60000))
						throw new Error('Invalid latency');
					if (body.effect === 'http' && !integer(body.status, 400, 599))
						throw new Error('Invalid error status');
					faults.push({ ...body, count: body.count ?? 1 });
					break;
				case '/__sim/socket':
					if (body.action === 'close') {
						for (const ws of wss.clients) ws.close(1012, 'Injected disconnect');
					} else if (body.action === 'pause') paused = body.active !== false;
					else if (body.action === 'refuse') refuse = body.active !== false;
					else if (body.action === 'malformed') {
						for (const ws of wss.clients) ws.send('{invalid');
					} else throw new Error('Unknown socket action');
					break;
				default:
					return reply(res, { status: 404 });
			}
			reply(res, { status: 200, body: { ok: true } });
		} catch (error) {
			reply(res, { status: 400, body: { error: error.message } });
		}
	});
	let apiPort, adminPort;
	try {
		apiPort = await listen(api, port);
		adminPort = await listen(control, controlPort);
	} catch (error) {
		api.close();
		control.close();
		wss.close();
		throw error;
	}
	let lastTelemetry = 0,
		lastAnalytics = 0,
		lastDevice = 0;
	const timer = setInterval(() => {
		device.tick();
		const now = device.now();
		if (paused || device.runtime.offlineUntil > now) return;
		if (now - lastTelemetry >= 500) {
			lastTelemetry = now;
			emit('rssi', {
				rssi: device.runtime.network.wifi ? -45 : 0,
				ssid: device.runtime.network.wifi
					? device.saved.settings.wifiSettings.wifi_networks[0]?.ssid || 'Simulator WiFi'
					: 'disconnected'
			});
			if (device.features.ethernet)
				emit('ethernet', { connected: device.runtime.network.ethernet });
			if (device.features.battery) emit('battery', device.runtime.battery);
		}
		if (now - lastAnalytics >= 2000) {
			lastAnalytics = now;
			if (device.features.analytics) emit('analytics', analytics(device));
		}
		if (now - lastDevice >= 1000) {
			lastDevice = now;
			if (device.actuator) emit('device.state', device.snapshot());
		}
	}, 25);
	return {
		device,
		apiPort,
		controlPort: adminPort,
		controlToken,
		async close() {
			clearInterval(timer);
			clearFaults();
			for (const t of pendingTimers) clearTimeout(t);
			for (const ws of wss.clients) ws.terminate();
			wss.close();
			await Promise.all(
				[api, control].map(
					(s) =>
						new Promise((resolve) => {
							s.closeAllConnections();
							s.close(resolve);
						})
				)
			);
		}
	};
}
if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
	const sim = await startSimulator({
		port: Number(process.env.SIM_PORT || 3080),
		controlPort: Number(process.env.SIM_CONTROL_PORT || 3081),
		controlToken: process.env.SIM_CONTROL_TOKEN || undefined,
		profile: process.env.SIM_PROFILE || 'actuator',
		stateFile: process.env.SIM_STATE_FILE,
		log: console.log
	});
	console.log(
		`Simulator API http://127.0.0.1:${sim.apiPort}; control http://127.0.0.1:${sim.controlPort}`
	);
	if (!process.env.SIM_CONTROL_TOKEN) console.log(`Local control token: ${sim.controlToken}`);
	const stop = async () => {
		await sim.close();
		process.exit(0);
	};
	process.on('SIGINT', stop);
	process.on('SIGTERM', stop);
}
