// Deterministic API simulation only. Never connects to a cloud endpoint.
export const mcpDefaults = () => ({
	schema_version: 1,
	revision: 1,
	enabled: false,
	alias: 'Switching Actuator',
	endpoint: '',
	channel_mask: 0
});
const id = '020000000010';
const states = [
	'disabled',
	'unconfigured',
	'waiting_network',
	'waiting_time',
	'connecting',
	'initializing',
	'ready',
	'backoff',
	'error',
	'paused'
];
const descriptions = [
	'Read exposed relay status',
	'Read device alias',
	'Set a relay on or off',
	'Pulse a relay for its configured duration',
	'Turn off all exposed relays',
	'Identify the device for five seconds'
];
const names = ['status', 'get_alias', 'set_relay', 'pulse_relay', 'all_off', 'identify'];
export function mcpTools(s) {
	return names.flatMap((name, i) => {
		if (!s.channel_mask && i >= 2 && i <= 4) return [];
		const properties = {},
			required = [];
		if (i === 2 || i === 3) {
			properties.channel = {
				type: 'integer',
				enum: Array.from({ length: 6 }, (_, n) => n + 1).filter(
					(n) => s.channel_mask & (1 << (n - 1))
				)
			};
			required.push('channel');
			if (i === 2) {
				properties.on = { type: 'boolean' };
				required.push('on');
			} else {
				properties.request_id = { type: 'string', minLength: 1, maxLength: 64 };
				required.push('request_id');
			}
		}
		return [
			{
				name: `actuator_${name}_${id}`,
				description: `${s.alias}: ${descriptions[i]}`,
				inputSchema: { type: 'object', additionalProperties: false, properties, required }
			}
		];
	});
}
function endpointHost(endpoint) {
	if (!endpoint || Buffer.byteLength(endpoint) > 2048 || /[^\x21-\x7e]|[#\\]/.test(endpoint))
		return null;
	const match = /^wss:\/\/([^/?]+)([/?].*)?$/.exec(endpoint);
	if (!match) return null;
	const authority = /^([^:]+)(?::([0-9]{1,5}))?$/.exec(match[1]);
	if (!authority || (authority[2] && (+authority[2] < 1 || +authority[2] > 65535))) return null;
	const host = authority[1];
	if (
		host.length > 253 ||
		!host.split('.').every((label) => /^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/i.test(label))
	)
		return null;
	return host;
}
export function publicMcp(s) {
	const { endpoint, ...publicFields } = s;
	return {
		...publicFields,
		endpoint_configured: !!endpoint,
		endpoint_host: endpointHost(endpoint) || ''
	};
}
export function mcpStatus(d) {
	const s = d.saved.settings.xiaozhiMcpSettings || mcpDefaults(),
		r = d.runtime.mcp || {};
	const ota = ['preparing', 'progress', 'finished'].includes(d.runtime.ota.status);
	let state = !s.enabled
		? 'disabled'
		: !s.endpoint
			? 'unconfigured'
			: ota
				? 'paused'
				: !d.online()
					? 'waiting_network'
					: !d.saved.settings.ntpSettings.enabled
						? 'waiting_time'
						: r.state || 'connecting';
	return {
		state,
		error_code: r.error || '',
		enabled: s.enabled,
		connected: ['initializing', 'ready'].includes(state),
		ready: state === 'ready',
		alias: s.alias,
		device_id: id,
		network_interface: d.runtime.network.interface,
		tool_count: s.channel_mask ? 6 : 3,
		retry_in_ms: state === 'backoff' ? Math.max(0, (r.retryAt || d.now()) - d.now()) : 0,
		connected_at_ms: ['initializing', 'ready'].includes(state) ? r.connectedAt || 0 : 0
	};
}
export function mcpAction(d, body) {
	if (!states.includes(body.state)) throw new Error('Invalid MCP state');
	d.runtime.mcp = {
		state: body.state,
		error: [
			'connection_failed',
			'authentication_failed',
			'configuration_invalid',
			'worker_unavailable'
		].includes(body.error)
			? body.error
			: '',
		retryAt: d.now() + (body.retryMs || 1000),
		connectedAt: d.now() - d.started
	};
}
export function mcpRoute(d, path, method, body, user) {
	if (
		![
			'xiaozhiMcpSettings',
			'xiaozhiMcpStatus',
			'xiaozhiMcpTools',
			'xiaozhiMcpReconnect',
			'xiaozhiMcpEndpoint'
		].includes(path) ||
		!d.features.xiaozhi_mcp
	)
		return null;
	const fail = (status, error, field = '') => ({ status, body: { error, field } });
	if (!user) return { status: 401 };
	if (path !== 'xiaozhiMcpStatus' && !user.admin) return { status: 403 };
	const s = d.saved.settings.xiaozhiMcpSettings || mcpDefaults();
	if (path === 'xiaozhiMcpEndpoint') {
		if (method !== 'POST') return { status: 405 };
		if (
			!body ||
			typeof body !== 'object' ||
			Array.isArray(body) ||
			Object.keys(body).length !== 1 ||
			!Number.isInteger(body.revision) ||
			body.revision < 0 ||
			body.revision > 0xffffffff
		)
			return fail(422, 'invalid_settings', 'revision');
		if (body.revision !== s.revision) return fail(409, 'endpoint_unavailable');
		return {
			status: 200,
			headers: { 'Cache-Control': 'no-store' },
			body: { endpoint: s.endpoint, revision: s.revision }
		};
	}
	if (method === 'GET') {
		if (path === 'xiaozhiMcpSettings') return { status: 200, body: publicMcp(s) };
		if (path === 'xiaozhiMcpStatus') return { status: 200, body: mcpStatus(d) };
		if (path === 'xiaozhiMcpTools')
			return { status: 200, body: { tools: mcpTools(s), channel_mask: s.channel_mask } };
	}
	if (method === 'POST' && path === 'xiaozhiMcpReconnect') {
		if (!s.enabled || !s.endpoint) return fail(409, 'not_enabled');
		if (mcpStatus(d).state === 'paused') return fail(503, 'service_unavailable');
		d.runtime.mcp = { state: 'connecting' };
		return { status: 202, body: { accepted: true } };
	}
	if (method !== 'POST' || path !== 'xiaozhiMcpSettings') return { status: 405 };
	if (
		!body ||
		typeof body !== 'object' ||
		Array.isArray(body) ||
		Buffer.byteLength(JSON.stringify(body)) > 4096
	)
		return fail(400, 'invalid_body');
	if (mcpStatus(d).state === 'paused') return fail(503, 'service_unavailable');
	if (
		Object.keys(body).some(
			(k) =>
				!['revision', 'enabled', 'alias', 'channel_mask', 'endpoint', 'clear_endpoint'].includes(k)
		)
	)
		return fail(422, 'invalid_settings', 'fields');
	if (!Number.isInteger(body.revision) || body.revision < 0 || body.revision > 0xffffffff)
		return fail(422, 'invalid_settings', 'revision');
	if (body.revision !== s.revision) return fail(409, 'revision_conflict', 'revision');
	if (typeof body.enabled !== 'boolean') return fail(422, 'invalid_settings', 'enabled');
	if (typeof body.alias !== 'string') return fail(422, 'invalid_settings', 'alias');
	const alias = body.alias.replace(/^ +| +$/g, '');
	if (!alias || Buffer.byteLength(alias) > 64 || /[\x00-\x1f\x7f]|[\uD800-\uDFFF]/u.test(alias))
		return fail(422, 'invalid_settings', 'alias');
	if (!Number.isInteger(body.channel_mask) || body.channel_mask < 0 || body.channel_mask > 63)
		return fail(422, 'invalid_settings', 'channel_mask');
	if (
		('endpoint' in body && typeof body.endpoint !== 'string') ||
		('clear_endpoint' in body && typeof body.clear_endpoint !== 'boolean') ||
		(body.clear_endpoint && body.endpoint)
	)
		return fail(422, 'invalid_settings', 'endpoint');
	const endpoint = body.clear_endpoint ? '' : body.endpoint || s.endpoint;
	if ((endpoint && !endpointHost(endpoint)) || (body.enabled && !endpoint))
		return fail(422, 'invalid_settings', 'endpoint');
	const candidate = {
		...s,
		alias,
		endpoint,
		enabled: body.enabled,
		channel_mask: body.channel_mask
	};
	if (JSON.stringify(candidate) !== JSON.stringify(s)) {
		if (s.revision === 0xffffffff) return fail(409, 'revision_conflict', 'revision');
		candidate.revision++;
		try {
			d.commit({
				...structuredClone(d.saved),
				settings: { ...structuredClone(d.saved.settings), xiaozhiMcpSettings: candidate }
			});
		} catch {
			return fail(503, 'storage_failed');
		}
		d.runtime.mcp = { state: candidate.enabled ? 'connecting' : 'disabled' };
	}
	return { status: 200, body: publicMcp(candidate) };
}
