import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
function inspect(dir) {
	for (const entry of readdirSync(dir, { withFileTypes: true })) {
		const path = join(dir, entry.name);
		if (entry.isDirectory()) {
			inspect(path);
			continue;
		}
		if (!/\.(js|html|json)$/.test(entry.name)) continue;
		const text = readFileSync(path, 'utf8');
		for (const marker of [
			'playwright-local-control',
			'/__sim/',
			'sim-admin',
			'SIM_CONTROL_TOKEN'
		]) {
			if (text.includes(marker))
				throw new Error(`Simulator/test marker ${marker} leaked into ${path}`);
		}
	}
}
inspect('build');
console.log('Production bundle contains no simulator/control/test markers.');
