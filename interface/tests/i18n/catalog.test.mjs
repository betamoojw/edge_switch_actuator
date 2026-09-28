import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs';
import { parse } from 'svelte/compiler';

const messages = JSON.parse(readFileSync('src/lib/i18n/messages.json', 'utf8'));
// International protocol names, units and product identifiers deliberately stay unchanged.
const invariant = new Set([
	'',
	'OK',
	'WiFi',
	'Ethernet',
	'MQTT',
	'NTP',
	'KNX',
	'Modbus',
	'Modbus RTU',
	'Modbus TCP',
	'KNXnet/IP',
	'RS485 / Modbus RTU',
	'RSSI',
	'SSID',
	'DHCP',
	'DNS',
	'DNS 1',
	'DNS 2',
	'PSRAM',
	'ESP-IDF',
	'/ Arduino',
	'URI',
	'Hz',
	'MHz',
	'Mbps',
	'dBm',
	'KB',
	'KB /',
	'KB)',
	'v'
]);
const files = (dir) =>
	readdirSync(dir, { withFileTypes: true }).flatMap((entry) =>
		entry.isDirectory() ? files(`${dir}/${entry.name}`) : [`${dir}/${entry.name}`]
	);

test('all six local catalogues are complete and preserve named parameters', () => {
	const source = Object.fromEntries(
		readFileSync('src/lib/i18n/translations.tsv', 'utf8')
			.trimEnd()
			.split(/\r?\n/)
			.map((line) => {
				const [key, ...values] = line.split('\t');
				return [key, values];
			})
	);
	assert.deepEqual(messages, source, 'Run npm run i18n:build after editing translations');
	const parameters = (text) => [...text.matchAll(/\{(\w+)\}/g)].map((match) => match[1]).sort();
	for (const [key, translations] of Object.entries(messages)) {
		assert.equal(translations.length, 6, key);
		for (const translation of translations) {
			assert.ok(translation.trim(), key);
			assert.deepEqual(parameters(translation), parameters(key), key);
		}
	}
});

test('static Svelte UI text uses the catalogue and literal translation keys exist', () => {
	for (const file of files('src').filter((file) => file.endsWith('.svelte'))) {
		const ast = parse(readFileSync(file, 'utf8'));
		function visit(node, parent) {
			if (!node || typeof node !== 'object') return;
			if (node.type === 'Text' && parent?.type !== 'Attribute')
				assert.ok(!/[A-Za-z]/.test(node.data), `Untranslated text in ${file}: ${node.data}`);
			if (
				node.type === 'CallExpression' &&
				node.callee?.name === '$t' &&
				node.arguments[0]?.type === 'Literal'
			) {
				const key = node.arguments[0].value;
				assert.ok(Object.hasOwn(messages, key) || invariant.has(key), `Missing ${key} in ${file}`);
			}
			if (
				node.type === 'Property' &&
				['title', 'label', 'message', 'description', 'text'].includes(node.key?.name)
			) {
				const key =
					node.value?.type === 'Literal'
						? node.value.value
						: node.value?.type === 'TemplateLiteral' && !node.value.expressions.length
							? node.value.quasis[0].value.cooked
							: null;
				if (typeof key === 'string')
					assert.ok(
						Object.hasOwn(messages, key) || invariant.has(key),
						`Missing dynamic label ${key} in ${file}`
					);
			}
			for (const value of Object.values(node)) {
				if (Array.isArray(value)) value.forEach((child) => visit(child, node));
				else if (value && typeof value === 'object') visit(value, node);
			}
		}
		visit(ast.html);
		visit(ast.instance);
	}
});
