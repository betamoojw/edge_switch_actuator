import { sveltekit } from '@sveltejs/kit/vite';
import { normalizePath, type UserConfig } from 'vite';
import { fileURLToPath } from 'node:url';
import Icons from 'unplugin-icons/vite';
import viteLittleFS from './vite-plugin-littlefs';
import tailwindcss from '@tailwindcss/vite';
import { deviceTarget } from './scripts/target.mjs';

const config: UserConfig = {
	plugins: [
		sveltekit(),
		Icons({
			compiler: 'svelte'
		}),
		tailwindcss(),
		// Shorten file names for LittleFS 32 char limit
		viteLittleFS()
	],
	build: {
		minify: 'terser',
		sourcemap: false,
		rollupOptions: {
			output: {
				manualChunks(id) {
					if (id.includes('node_modules')) return 'vendor';
				}
			}
		},
		cssCodeSplit: true
	}
};

export default ({ command }: { command: string }): UserConfig => {
	if (command === 'build') return config;
	const target = deviceTarget(process.env.DEVICE_HOST);
	const proxy = {
		'/rest': { target: target.http, changeOrigin: true },
		'/ws': { target: target.ws, changeOrigin: true, ws: true }
	};
	return {
		...config,
		server: {
			proxy,
			// The local file dependency resolves outside node_modules on Windows.
			fs: {
				allow: [normalizePath(fileURLToPath(new URL('.', import.meta.url)))],
				deny: [
					'.env',
					'.env.*',
					'*.{crt,pem}',
					'**/.git/**',
					'**/.sim-state/**',
					'**/test-results/**',
					'**/playwright-report/**'
				]
			}
		},
		preview: { proxy }
	};
};
