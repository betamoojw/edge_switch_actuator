import { spawn } from 'node:child_process';
process.env.E2E_BUILT = '1';
const child = spawn(
	process.execPath,
	['node_modules/@playwright/test/cli.js', 'test', ...process.argv.slice(2)],
	{ stdio: 'inherit', env: process.env }
);
child.on('exit', (code) => process.exit(code ?? 1));
