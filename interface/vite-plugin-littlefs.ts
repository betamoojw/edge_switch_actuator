import type { Plugin } from 'vite';
import type { OutputOptions } from 'rollup';

function stripHash(pattern: string): string {
	return pattern.replace(/\.\[hash(?::\d+)?\]/g, '');
}

function stableFilenames(output: OutputOptions): OutputOptions {
	const { assetFileNames, chunkFileNames, entryFileNames } = output;
	return {
		...output,
		assetFileNames:
			typeof assetFileNames === 'function'
				? (asset) => stripHash(assetFileNames(asset))
				: assetFileNames && stripHash(assetFileNames),
		chunkFileNames:
			typeof chunkFileNames === 'function'
				? (chunk) => stripHash(chunkFileNames(chunk))
				: chunkFileNames && stripHash(chunkFileNames),
		entryFileNames:
			typeof entryFileNames === 'function'
				? (chunk) => stripHash(entryFileNames(chunk))
				: entryFileNames && stripHash(entryFileNames)
	};
}

export default function viteLittleFS(): Plugin[] {
	return [
		{
			name: 'vite-plugin-littlefs',
			enforce: 'post',
			apply: 'build',
			config(config) {
				const output = config.build?.rollupOptions?.output;
				if (!output) return;
				return {
					build: {
						rollupOptions: {
							output: Array.isArray(output) ? output.map(stableFilenames) : stableFilenames(output)
						}
					}
				};
			}
		}
	];
}
