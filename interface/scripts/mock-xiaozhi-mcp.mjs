// Local WSS protocol peer for an isolated development board. Requires a certificate
// trusted by that test firmware; never disables verification on production firmware.
import { createServer } from 'node:https';
import { readFileSync } from 'node:fs';
import { WebSocketServer } from 'ws';

if (!process.env.MCP_TEST_CERT || !process.env.MCP_TEST_KEY)
	throw new Error(
		'Set MCP_TEST_CERT and MCP_TEST_KEY to the local test certificate and key files.'
	);
const server = createServer({
	cert: readFileSync(process.env.MCP_TEST_CERT),
	key: readFileSync(process.env.MCP_TEST_KEY)
});
const wss = new WebSocketServer({ server, maxPayload: 16384 });
wss.on('connection', (ws) => {
	const send = (message) => ws.send(JSON.stringify(message));
	send({
		jsonrpc: '2.0',
		id: 'init',
		method: 'initialize',
		params: {
			protocolVersion: '2024-11-05',
			capabilities: {},
			clientInfo: { name: 'local-mcp-test', version: '1' }
		}
	});
	ws.on('message', (data) => {
		const message = JSON.parse(data.toString());
		if (message.id === 'init') {
			if (message.result?.protocolVersion !== '2024-11-05')
				throw new Error('Initialization failed');
			send({ jsonrpc: '2.0', method: 'notifications/initialized' });
			send({ jsonrpc: '2.0', id: 'list', method: 'tools/list' });
		} else if (message.id === 'list') {
			const tools = message.result?.tools;
			if (!Array.isArray(tools)) throw new Error('Tool listing failed');
			console.log('Tools:', tools.map((tool) => tool.name).join(', '));
			const status = tools.find((tool) => tool.name.startsWith('actuator_status_'));
			if (status)
				send({
					jsonrpc: '2.0',
					id: 'status',
					method: 'tools/call',
					params: { name: status.name, arguments: {} }
				});
		} else if (message.id === 'status') {
			console.log(
				'Read-only status result received:',
				message.result?.isError === false ? 'PASS' : 'FAIL'
			);
		}
	});
	ws.on('error', () => console.error('Test connection failed'));
});
server.listen(
	Number(process.env.MCP_TEST_PORT || 8443),
	process.env.MCP_TEST_BIND || '127.0.0.1',
	() => console.log('Local MCP WSS peer listening. No relay mutation tools are invoked.')
);
