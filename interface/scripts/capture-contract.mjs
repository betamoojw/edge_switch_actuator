// Read-only HTTP capture. Deliberately excludes mutating endpoints and uploads.
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';
import { deviceTarget } from './target.mjs';
const base = deviceTarget(process.env.DEVICE_HOST).http;
const featuresResponse = await fetch(`${base}/rest/features`);
if (!featuresResponse.ok) throw new Error('Cannot read features');
const features = await featuresResponse.json();
let token;
if (features.security) {
	if (!process.env.DEVICE_USERNAME || !process.env.DEVICE_PASSWORD)
		throw new Error('Set DEVICE_USERNAME and DEVICE_PASSWORD');
	const response = await fetch(`${base}/rest/signIn`, {
		method: 'POST',
		headers: { 'Content-Type': 'application/json' },
		body: JSON.stringify({
			username: process.env.DEVICE_USERNAME,
			password: process.env.DEVICE_PASSWORD
		})
	});
	if (!response.ok) throw new Error(`Login failed: ${response.status}`);
	token = (await response.json()).access_token;
}
function sanitize(value, key = '') {
	if (
		/password|jwt_secret|access_token|bearer_token|username|ssid|bssid|mac_address|(^ip$)|_ip|address|audit|tcpAllow/.test(
			key
		)
	)
		return '<redacted>';
	if (Array.isArray(value)) return value.map((v) => sanitize(v));
	if (value && typeof value === 'object')
		return Object.fromEntries(Object.entries(value).map(([k, v]) => [k, sanitize(v, k)]));
	return value;
}
const paths = [
	'features',
	'device/status',
	'device/config',
	'knx/config',
	'wifiStatus',
	'apStatus',
	'systemStatus'
];
if (features.ethernet) paths.push('ethernetStatus');
if (features.mqtt) paths.push('mqttStatus');
if (features.xiaozhi_mcp) paths.push('xiaozhiMcpStatus');
if (features.ntp) paths.push('ntpStatus');
const responses = {};
for (const path of paths) {
	const response = await fetch(`${base}/rest/${path}`, {
		headers: token ? { Authorization: `Bearer ${token}` } : {}
	});
	const contentType = response.headers.get('content-type');
	responses[path] = {
		method: 'GET',
		status: response.status,
		contentType,
		body: contentType?.includes('application/json') ? sanitize(await response.json()) : '<non-json>'
	};
}
const output = process.argv[2] || 'test-results/contract-capture.json';
mkdirSync(dirname(output), { recursive: true });
writeFileSync(
	output,
	JSON.stringify({ capturedAt: new Date().toISOString(), firmware: features, responses }, null, 2)
);
console.log(`Sanitized read-only capture saved to ${output}`);
