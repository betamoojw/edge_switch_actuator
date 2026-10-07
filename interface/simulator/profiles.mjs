// Wire fixtures derived from dev fd848362: DeviceConfig.h, FeaturesService.cpp,
// factory_settings.ini and framework *SettingsService.h. Credentials are local-only.
export const profileNames = [
	'actuator',
	'template',
	'ethernet',
	'knx',
	'battery',
	'security-off',
	'json'
];
export const clone = (value) => structuredClone(value);
export const integer = (value, min, max) => Number.isInteger(value) && value >= min && value <= max;
export const ipv4 = (value) =>
	typeof value === 'string' &&
	/^\d+\.\d+\.\d+\.\d+$/.test(value) &&
	value.split('.').every((n) => integer(Number(n), 0, 255));
export function defaults(profile = 'actuator') {
	if (!profileNames.includes(profile)) throw new Error(`Unknown simulator profile: ${profile}`);
	const config = {
		schemaVersion: 1,
		revision: 1,
		relays: Array.from({ length: 6 }, (_, i) => ({
			name: `Channel ${i + 1}`,
			enabled: true,
			startup: false,
			pulseMs: 1000,
			disconnectOff: false,
			timeout: 30
		})),
		rgb: true,
		buzzer: true,
		button: true,
		rs485: true,
		brightness: 10,
		clicks: [
			{ action: 0, target: 0 },
			{ action: 6, target: 0 },
			{ action: 8, target: 0 }
		],
		mode: 'off',
		unit: 1,
		baud: 1,
		serialFormat: 0,
		tcpPort: 502,
		busMask: 63,
		busIndicators: false,
		busWatchdog: false,
		tcpAllow: ''
	};
	const knx = {
		revision: 1,
		address: '0.0.0',
		owner: 'ets',
		configured: false,
		objects: Array.from({ length: 18 }, (_, i) => ({ number: i + 1, groups: [] })),
		parameters: config.relays.map(({ name, ...rest }) => rest)
	};
	if (profile === 'knx') {
		config.mode = 'knx_ip';
		Object.assign(knx, { revision: 1, address: '1.1.10', owner: 'ets', configured: true });
		knx.objects.forEach((obj) => {
			obj.groups = [`1/0/${obj.number}`];
		});
	}
	return {
		profile,
		config,
		knx,
		users: [
			{
				username: 'admin',
				password: 'sim-admin',
				admin: true,
				role: 'administrator',
				channels: 63
			},
			{
				username: 'installer',
				password: 'sim-installer',
				admin: false,
				role: 'installer',
				channels: 63
			},
			{
				username: 'operator',
				password: 'sim-operator',
				admin: false,
				role: 'operator',
				channels: 1
			},
			{ username: 'viewer', password: 'sim-viewer', admin: false, role: 'viewer', channels: 0 }
		],
		settings: {
			wifiSettings: { hostname: 'esp32-simulator', connection_mode: 1, wifi_networks: [] },
			apSettings: {
				provision_mode: 1,
				ssid: 'ESP32-Simulator',
				password: 'sim-admin',
				channel: 1,
				ssid_hidden: false,
				max_clients: 4,
				local_ip: '192.168.4.1',
				gateway_ip: '192.168.4.1',
				subnet_mask: '255.255.255.0'
			},
			ethernetSettings: { hostname: 'esp32-simulator', static_ip_config: false },
			mqttSettings: {
				enabled: false,
				...(profile === 'template' ? {} : { home_assistant_discovery: false }),
				uri: 'mqtts://broker.hivemq.com:8883',
				username: '',
				password: '',
				client_id: 'esp32-simulator',
				keep_alive: 120,
				clean_session: true,
				message_interval_ms: 0
			},
			ntpSettings: {
				enabled: true,
				server: 'time.windows.com',
				tz_label: 'Europe/Berlin',
				tz_format: 'GMT0BST,M3.5.0/1,M10.5.0'
			},
			brokerSettings: {
				mqtt_path: 'homeassistant/light/simulator',
				name: 'light-simulator',
				unique_id: 'simulator',
				status_topic: 'esp32sveltekit/simulator/status'
			}
		}
	};
}
export function features(profile) {
	return {
		security: profile !== 'security-off',
		mqtt: true,
		ntp: true,
		upload_firmware: true,
		download_firmware: true,
		sleep: profile === 'template',
		battery: profile === 'battery',
		analytics: true,
		coredump: true,
		event_use_json: profile === 'json',
		ethernet: profile === 'ethernet',
		firmware_version: '0.2.0',
		firmware_name: 'ESP32-SvelteKit',
		firmware_built_target: profile === 'template' ? 'esp32-s3-devkitc-1' : 'waveshare-relay-6ch'
	};
}
export function validateConfig(c) {
	if (c.schemaVersion !== 1 || !integer(c.revision, 0, 0xffffffff))
		return 'Invalid schema/revision';
	if (
		!Array.isArray(c.relays) ||
		c.relays.length !== 6 ||
		!Array.isArray(c.clicks) ||
		c.clicks.length !== 3
	)
		return 'Expected six channels and three bindings';
	for (const r of c.relays) {
		if (
			!r ||
			typeof r.name !== 'string' ||
			r.name.length > 32 ||
			['enabled', 'startup', 'disconnectOff'].some((k) => typeof r[k] !== 'boolean') ||
			!integer(r.pulseMs, 10, 60000) ||
			!integer(r.timeout, 1, 3600)
		)
			return 'Invalid relay settings';
	}
	if (
		['rgb', 'buzzer', 'button', 'rs485', 'busIndicators', 'busWatchdog'].some(
			(k) => typeof c[k] !== 'boolean'
		)
	)
		return 'Expected boolean setting';
	for (const [key, lo, hi] of [
		['brightness', 0, 100],
		['unit', 1, 247],
		['baud', 0, 4],
		['serialFormat', 0, 3],
		['tcpPort', 1, 65535],
		['busMask', 0, 63]
	]) {
		if (!integer(c[key], lo, hi)) return 'Invalid interface or indicator settings';
	}
	if (c.tcpPort === 80) return 'Port 80 is reserved for management';
	if (!['off', 'modbus_rtu', 'modbus_tcp', 'knx_ip'].includes(c.mode))
		return 'Invalid protocol mode';
	if (c.mode === 'modbus_rtu' && !c.rs485) return 'Enable RS485 before selecting RTU';
	if (c.tcpAllow && !ipv4(c.tcpAllow)) return 'TCP source must be an IPv4 address or empty';
	for (const b of c.clicks) {
		if (!b || !integer(b.action, 0, 8) || !integer(b.target, 0, 6)) return 'Invalid button binding';
		if (b.action >= 1 && b.action <= 4) {
			if (b.target && !c.relays[b.target - 1].enabled)
				return 'Button action requires enabled channel';
		} else if (b.target) return 'Device action target must be zero';
	}
	return '';
}
export function validateKnx(k, current, running) {
	if (!running || current.busy) return 'KNX is inactive or ETS download is busy';
	if (!integer(k.revision, 0, 0xffffffff) || k.revision !== current.revision)
		return 'KNX revision conflict';
	if (current.owner === 'ets' && k.takeover !== true) return 'Explicit web takeover is required';
	const address =
		typeof k.address === 'string' && /^(\d{1,2})\.(\d{1,2})\.(\d{1,3})$/.exec(k.address);
	if (
		!address ||
		!integer(+address[1], 0, 15) ||
		!integer(+address[2], 0, 15) ||
		!integer(+address[3], 1, 255)
	)
		return 'Invalid individual address';
	if (
		!Array.isArray(k.objects) ||
		k.objects.length !== 18 ||
		!Array.isArray(k.parameters) ||
		k.parameters.length !== 6
	)
		return 'Expected 18 objects and six parameter records';
	const all = new Set();
	let associations = 0;
	for (const [i, o] of k.objects.entries()) {
		if (!o || o.number !== i + 1 || !Array.isArray(o.groups) || o.groups.length > 8)
			return 'Invalid object mapping';
		const seen = new Set();
		for (const group of o.groups) {
			const m = typeof group === 'string' && /^(\d+)\/(\d+)\/(\d+)$/.exec(group);
			if (!m || !integer(+m[1], 0, 31) || !integer(+m[2], 0, 7) || !integer(+m[3], 0, 255))
				return 'Invalid group address';
			const n = (+m[1] << 11) | (+m[2] << 8) | +m[3];
			if (!n || seen.has(n)) return 'Reserved or duplicate group address';
			seen.add(n);
			all.add(n);
			associations++;
		}
	}
	if (all.size > 64 || associations > 96) return 'Group table capacity exceeded';
	for (const r of k.parameters) {
		if (
			!r ||
			['enabled', 'startup', 'disconnectOff'].some((key) => typeof r[key] !== 'boolean') ||
			!Number.isInteger(r.timeout) ||
			!Number.isInteger(r.pulseMs)
		)
			return 'Invalid KNX parameter types';
		if (!integer(r.timeout, 1, 3600) || !integer(r.pulseMs, 10, 60000))
			return 'Invalid parameter range';
	}
	return '';
}
