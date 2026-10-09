import { mcpRoute } from './xiaozhi-mcp.mjs';
import { clone, defaults, integer, ipv4 } from './profiles.mjs';
import { hashPassword, verifyPassword } from './auth.mjs';
import Busboy from 'busboy';
import { createHash } from 'node:crypto';

export const jsonObject = (value) =>
	value !== null && typeof value === 'object' && !Array.isArray(value);
export function analytics(d) {
	return {
		uptime: Math.floor((d.now() - d.started) / 1000),
		free_heap: 180000,
		used_heap: 120000,
		total_heap: 300000,
		min_free_heap: 160000,
		max_alloc_heap: 140000,
		fs_used: 120000,
		fs_total: 1500000,
		core_temp: 42,
		free_psram: 7000000,
		used_psram: 1000000,
		psram_size: 8000000
	};
}
const empty = (status = 200) => ({ status });
const ok = (body) => ({ status: 200, body });
function normalizeSettings(name, body, d) {
	const initial = defaults(d.saved.profile).settings[name];
	const value = Object.fromEntries(
		Object.entries(initial).map(([k, v]) => [k, typeof body[k] === typeof v ? body[k] : v])
	);
	const ips = ['local_ip', 'gateway_ip', 'subnet_mask', 'dns_ip_1', 'dns_ip_2'];
	const network = (v) => {
		const normalized = { static_ip_config: !!v.static_ip_config };
		for (const k of ips)
			if (ipv4(v[k]) && !['0.0.0.0', '255.255.255.255'].includes(v[k])) normalized[k] = v[k];
		if (!normalized.dns_ip_1 && normalized.dns_ip_2) {
			normalized.dns_ip_1 = normalized.dns_ip_2;
			delete normalized.dns_ip_2;
		}
		if (!normalized.local_ip || !normalized.gateway_ip || !normalized.subnet_mask)
			normalized.static_ip_config = false;
		return normalized;
	};
	if (name === 'wifiSettings') {
		value.wifi_networks = [];
		// The firmware increments its counter before validation AND after accepting
		// a network. Preserve that current behavior rather than accepting five.
		let index = 0;
		for (const n of Array.isArray(body.wifi_networks) ? body.wifi_networks : []) {
			if (index++ >= 5) break;
			if (
				!n ||
				typeof n.ssid !== 'string' ||
				n.ssid.length < 1 ||
				n.ssid.length > 31 ||
				typeof n.password !== 'string' ||
				n.password.length > 64
			)
				continue;
			value.wifi_networks.push({ ssid: n.ssid, password: n.password, ...network(n) });
			index++;
		}
	}
	if (name === 'ethernetSettings') Object.assign(value, network(body));
	return value;
}

export async function upload(req, d) {
	let file;
	try {
		file = await new Promise((resolve, reject) => {
			const parser = Busboy({
				headers: req.headers,
				limits: { files: 1, fileSize: 4 * 1024 * 1024, fields: 0 }
			});
			let selected,
				limited = false;
			parser.on('file', (field, stream, info) => {
				const chunks = [];
				stream.on('data', (chunk) => chunks.push(chunk));
				stream.on('limit', () => {
					limited = true;
				});
				stream.on('end', () => {
					selected = { field, name: info.filename, bytes: Buffer.concat(chunks) };
				});
			});
			parser.on('filesLimit', () => {
				limited = true;
			});
			parser.on('error', reject);
			req.on('error', reject);
			parser.on('close', () => resolve(limited ? { tooLarge: true } : selected));
			req.pipe(parser);
		});
	} catch {
		return empty(400);
	}
	const fail = (status, message) => {
		d.emit('event', 'otastatus', { status: 'error', error: message });
		return empty(status);
	};
	if (file?.tooLarge) return fail(413, 'Firmware too large');
	if (!file || file.field !== 'file') return fail(400, 'Upload not initialized');
	if (/\.md5$/i.test(file.name)) {
		if (file.bytes.length !== 32) {
			d.runtime.md5 = '';
			return fail(422, 'MD5 must be exactly 32 bytes');
		}
		d.runtime.md5 = file.bytes.toString();
		return ok({ md5: d.runtime.md5 });
	}
	if (!/\.bin$/i.test(file.name)) return fail(406, 'File not a firmware binary or MD5 hash');
	if (file.bytes.length < 24 || file.bytes[0] !== 0xe9 || file.bytes.readUInt16LE(12) !== 9)
		return fail(503, 'Wrong firmware for this device');
	const expected = d.runtime.md5;
	d.runtime.md5 = '';
	if (expected && createHash('md5').update(file.bytes).digest('hex') !== expected)
		return fail(500, 'MD5 checksum mismatch');
	d.ota();
	return empty();
}

export function frameworkRoute(d, path, method, body, user, url) {
	if (path.startsWith('xiaozhiMcp')) return mcpRoute(d, path, method, body, user);
	const r = d.runtime,
		s = d.saved.settings,
		f = d.features;
	if (path === 'features' && method === 'GET') return ok(f);
	if (path === 'signIn' && method === 'POST' && f.security) {
		const found = d.saved.users.find(
			(u) => u.username === body?.username && verifyPassword(body?.password, u.password)
		);
		return found ? ok({ access_token: d.auth.token(found) }) : empty(401);
	}
	if (path === 'verifyAuthorization' && method === 'GET' && f.security)
		return empty(user ? 200 : 401);
	const gate = {
		ethernetStatus: 'ethernet',
		ethernetSettings: 'ethernet',
		mqttStatus: 'mqtt',
		mqttSettings: 'mqtt',
		ntpSettings: 'ntp',
		ntpStatus: 'ntp',
		time: 'ntp',
		sleep: 'sleep',
		downloadUpdate: 'download_firmware',
		securitySettings: 'security',
		generateToken: 'security',
		coreDump: 'coredump'
	};
	if (gate[path] && !f[gate[path]]) return null;
	if (['lightState', 'brokerSettings'].includes(path) && d.actuator) return null;
	const adminPaths = [
		'securitySettings',
		'generateToken',
		'scanNetworks',
		'listNetworks',
		'restart',
		'factoryReset',
		'downloadUpdate',
		'time',
		...Object.keys(s)
	];
	const readPaths = [
		'wifiStatus',
		'apStatus',
		'ethernetStatus',
		'mqttStatus',
		'ntpStatus',
		'systemStatus',
		'coreDump'
	];
	if (![...adminPaths, ...readPaths, 'sleep', 'lightState'].includes(path)) return null;
	if (!user || (adminPaths.includes(path) && !user.admin)) return empty(401);
	if (path.endsWith('Settings') && ['GET', 'POST'].includes(method)) {
		if (path === 'securitySettings') {
			if (method === 'POST') {
				if (
					!jsonObject(body) ||
					!Array.isArray(body.users) ||
					body.users.length > 16 ||
					!body.users.some((u) => u?.admin === true)
				)
					return empty(400);
				const names = new Set(),
					users = [];
				for (const u of body.users) {
					if (
						!u ||
						typeof u.username !== 'string' ||
						!integer(u.username.length, 3, 32) ||
						names.has(u.username) ||
						!integer(u.channels ?? 63, 0, 63) ||
						!['viewer', 'operator', 'installer', 'administrator'].includes(u.role ?? 'viewer')
					)
						return empty(400);
					names.add(u.username);
					const password =
						typeof u.password === 'string' && u.password
							? hashPassword(u.password)
							: d.saved.users.find((x) => x.username === u.username)?.password;
					if (!password) return empty(400);
					users.push({
						username: u.username,
						password,
						admin: u.admin === true,
						role: u.admin ? 'administrator' : (u.role ?? 'viewer'),
						channels: u.channels ?? 63
					});
				}
				d.commit({ ...clone(d.saved), users });
				d.auth.rotate();
			}
			return ok({ jwt_secret: '', users: d.saved.users.map((u) => ({ ...u, password: '' })) });
		}
		if (method === 'POST') {
			if (!jsonObject(body)) return empty(400);
			const value = normalizeSettings(path, body, d);
			d.commit({ ...clone(d.saved), settings: { ...clone(s), [path]: value } });
			if (path === 'wifiSettings') {
				d.emit('event', 'reconnect', { delay_ms: 1000 });
				d.schedule(1000, () =>
					d.action({
						type: 'network',
						wifi: value.connection_mode !== 0 && value.wifi_networks.length > 0
					})
				);
			}
			if (path === 'mqttSettings') r.mqttConnected = value.enabled;
			if (path === 'apSettings')
				r.network.ap =
					value.provision_mode === 0 || (value.provision_mode === 1 && !r.network.wifi);
		}
		return ok(clone(d.saved.settings[path]));
	}
	if (method === 'GET') {
		const ipFields = {
			local_ip: r.network.ip,
			mac_address: '02:00:00:00:00:10',
			subnet_mask: '255.255.255.0',
			gateway_ip: '192.0.2.1',
			dns_ip_1: '192.0.2.1'
		};
		switch (path) {
			case 'wifiStatus':
				return ok(
					r.network.wifi
						? {
								status: 3,
								...ipFields,
								rssi: -45,
								ssid: s.wifiSettings.wifi_networks[0]?.ssid || 'Simulator WiFi',
								bssid: '02:00:00:00:00:01',
								channel: 6
							}
						: { status: 6 }
				);
			case 'apStatus':
				return ok({
					status: r.network.ap ? 0 : 1,
					ip_address: s.apSettings.local_ip,
					mac_address: '02:00:00:00:00:11',
					station_num: r.network.apClients
				});
			case 'ethernetStatus':
				return ok(
					r.network.ethernet
						? { connected: true, ...ipFields, link_speed: 100 }
						: { connected: false }
				);
			case 'mqttStatus':
				return ok({
					enabled: s.mqttSettings.enabled,
					connected: r.mqttConnected,
					client_id: s.mqttSettings.client_id,
					last_error: r.mqttError
				});
			case 'ntpStatus': {
				const date = new Date(d.now()).toISOString();
				return ok({
					status: s.ntpSettings.enabled ? 1 : 0,
					utc_time: date,
					local_time: date.slice(0, -1),
					server: s.ntpSettings.server,
					uptime: Math.floor((d.now() - d.started) / 1000)
				});
			}
			case 'systemStatus':
				return ok({
					...analytics(d),
					esp_platform: 'esp32s3',
					firmware_version: f.firmware_version,
					cpu_freq_mhz: 240,
					cpu_type: 'ESP32-S3',
					cpu_rev: 0,
					cpu_cores: 2,
					sketch_size: 1500000,
					free_sketch_space: 2500000,
					sdk_version: 'simulated',
					arduino_version: '3.2.0',
					flash_chip_size: 8388608,
					flash_chip_speed: 80000000,
					cpu_reset_reason: 'Software reset',
					network: { online: d.online(), interface: r.network.interface, ip: r.network.ip }
				});
			case 'scanNetworks':
				if (r.scanUntil <= d.now()) r.scanUntil = d.now() + 1000;
				return empty(202);
			case 'listNetworks':
				return r.scanUntil > d.now() ? empty(202) : ok({ networks: clone(r.scanResults) });
			case 'coreDump':
				return r.coreDump
					? {
							status: 200,
							raw: Buffer.from('SIMULATED CORE DUMP\n'),
							contentType: 'application/octet-stream'
						}
					: { status: 500, body: { status: 'error', message: 'core dump not available' } };
			case 'generateToken': {
				const u = d.saved.users.find((u) => u.username === url.searchParams.get('username'));
				return u ? ok({ token: d.auth.token(u) }) : empty(401);
			}
			case 'lightState':
				return ok({ led_on: r.led_on });
		}
	}
	if (method === 'POST') {
		switch (path) {
			case 'restart':
			case 'sleep':
				d.schedule(50, () => d.reboot());
				return empty();
			case 'factoryReset':
				d.schedule(50, () => {
					d.reset();
					d.runtime.offlineUntil = d.now() + 1200;
				});
				return empty();
			case 'downloadUpdate':
				if (!jsonObject(body)) return empty(400);
				d.ota();
				return empty();
			case 'time':
				return empty(
					!s.ntpSettings.enabled &&
						typeof body?.local_time === 'string' &&
						/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d/.test(body.local_time)
						? 200
						: 400
				);
			case 'lightState':
				if (!jsonObject(body)) return empty(400);
				r.led_on = body.led_on === true;
				d.emit('event', 'led', { led_on: r.led_on });
				return ok({ led_on: r.led_on });
		}
	}
	return null;
}
