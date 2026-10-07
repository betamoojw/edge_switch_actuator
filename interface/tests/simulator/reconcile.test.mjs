import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import ts from 'typescript';

test('partial snapshots preserve unrelated values and nested identities', () => {
	const exports = {};
	vm.runInNewContext(
		ts.transpileModule(
			readFileSync(new URL('../../src/lib/device/reconcile.ts', import.meta.url), 'utf8'),
			{
				compilerOptions: { module: ts.ModuleKind.CommonJS }
			}
		).outputText,
		{ exports }
	);
	const state = {
		relays: [{ on: false }, { on: false }],
		rgb: { color: 1 },
		capabilities: { command: true }
	};
	const array = state.relays,
		first = array[0],
		second = array[1],
		rgb = state.rgb;
	exports.reconcile(state, { relays: [{ on: true }, { on: false }], rgb: { color: 1 } });
	assert.equal(state.relays, array);
	assert.equal(state.relays[0], first);
	assert.equal(state.relays[1], second);
	assert.equal(state.rgb, rgb);
	assert.equal(first.on, true);
	assert.equal(state.capabilities.command, true);
});
