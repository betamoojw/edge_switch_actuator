import { existsSync, readFileSync, mkdirSync, writeFileSync, renameSync } from 'node:fs';
import { dirname } from 'node:path';
export function readState(file) {
	if (!file || !existsSync(file)) return null;
	const data = JSON.parse(readFileSync(file, 'utf8'));
	if (data.version !== 1 || !data.state) throw new Error('Unsupported simulator state file');
	return data.state;
}
export function writeState(file, state) {
	if (!file) return;
	mkdirSync(dirname(file), { recursive: true });
	writeFileSync(`${file}.tmp`, JSON.stringify({ version: 1, state }), { mode: 0o600 });
	renameSync(`${file}.tmp`, file);
}
