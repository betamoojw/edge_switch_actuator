import { createServer, preview } from 'vite';
import { startSimulator } from '../simulator/server.mjs';
import { deviceTarget } from './target.mjs';
const mode = process.argv[2];
if (!['sim', 'device', 'built'].includes(mode)) throw new Error('Expected sim, device or built');
let simulator;
if (mode !== 'device') {
	simulator = await startSimulator({
		port: Number(process.env.SIM_PORT || 3080),
		controlPort: Number(process.env.SIM_CONTROL_PORT || 3081),
		controlToken: process.env.SIM_CONTROL_TOKEN || undefined,
		profile: process.env.SIM_PROFILE || 'actuator',
		stateFile: process.env.SIM_STATE_FILE,
		log: console.log
	});
	process.env.DEVICE_HOST = `http://127.0.0.1:${simulator.apiPort}`;
	console.log(`Simulator control: http://127.0.0.1:${simulator.controlPort}`);
	if (!process.env.SIM_CONTROL_TOKEN) console.log(`Local control token: ${simulator.controlToken}`);
} else deviceTarget(process.env.DEVICE_HOST);
let vite;
try {
	const options = {
		server: {
			host: '127.0.0.1',
			port: Number(process.env.FRONTEND_PORT || 5173),
			strictPort: true
		},
		preview: {
			host: '127.0.0.1',
			port: Number(process.env.FRONTEND_PORT || 5173),
			strictPort: true
		}
	};
	vite = mode === 'built' ? await preview(options) : await createServer(options);
	if (mode !== 'built') await vite.listen();
	vite.printUrls();
} catch (error) {
	await simulator?.close();
	throw error;
}
let stopping = false;
async function stop() {
	if (stopping) return;
	stopping = true;
	if (mode === 'built') await new Promise((resolve) => vite.httpServer.close(resolve));
	else await vite.close();
	await simulator?.close();
	process.exit(0);
}
process.on('SIGINT', stop);
process.on('SIGTERM', stop);
