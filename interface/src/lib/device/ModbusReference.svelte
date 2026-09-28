<script lang="ts">
	import { t } from '$lib/i18n';
	import Book from '~icons/tabler/book-2';
	import Download from '~icons/tabler/download';
	import source from '../../../../docs/actuator-modbus-map.md?raw';

	let { mode }: { mode: 'modbus_rtu' | 'modbus_tcp' } = $props();
	// Keep offsets, limits and access rules tied to the versioned device contract.
	const sections = source.split(/^## /m).slice(1);
	const tables = sections
		.filter((part) => part.includes('| Offset |'))
		.map((part) => {
			const lines = part.split(/\r?\n/);
			const rows = lines
				.filter((line) => line.startsWith('|'))
				.map((line) =>
					line
						.split('|')
						.slice(1, -1)
						.map((cell) => cell.trim())
				);
			return { heading: lines[0], headers: rows[0], rows: rows.slice(2) };
		});
	const titles = ['Coils', 'Discrete inputs', 'Holding registers', 'Input registers'];
	const codes = ['FC01 / FC05 / FC15', 'FC02', 'FC03 / FC06 / FC16', 'FC04'];
	const download = 'data:text/markdown;charset=utf-8,' + encodeURIComponent(source);
	const functions = [
		['FC01', 'Read coils'],
		['FC02', 'Read discrete inputs'],
		['FC03', 'Read holding registers'],
		['FC04', 'Read input registers'],
		['FC05', 'Write single coil'],
		['FC06', 'Write single register'],
		['FC15 (0x0F)', 'Write multiple coils'],
		['FC16 (0x10)', 'Write multiple registers'],
		['FC43/14 (0x2B/0x0E)', 'Read device identification']
	];
</script>

<section
	aria-labelledby="modbus-map-title"
	class="rounded-box border-base-300 bg-base-200 min-w-0 space-y-4 border p-4"
>
	<div class="flex flex-wrap items-center justify-between gap-3">
		<h3 id="modbus-map-title" class="flex items-center gap-2 font-bold">
			<Book class="h-5 w-5 shrink-0" aria-hidden="true" />{$t('Modbus function map')}
		</h3>
		<span class="badge badge-outline">{mode === 'modbus_rtu' ? 'Modbus RTU' : 'Modbus TCP'}</span>
	</div>
	<p class="text-sm">
		{$t('Reference follows the selected protocol. Apply settings to activate it on the device.')}
	</p>
	<div class="rounded-box bg-base-100 space-y-2 p-3 text-sm">
		<p>
			{$t(
				'Use hexadecimal, zero-based PDU offsets. Coils, discrete inputs, holding registers and input registers are separate address spaces; 00001/40001 are not wire addresses.'
			)}
		</p>
		<p>
			{$t(
				'Registers use big-endian byte order. 32-bit counters use two registers, high word first. Relay status is the commanded output, not measured contact feedback.'
			)}
		</p>
	</div>
	<div class="grid gap-2 sm:grid-cols-2">
		{#each functions as [code, meaning]}
			<div
				class="rounded-box bg-base-100 flex flex-wrap items-center gap-x-3 gap-y-1 px-3 py-2 text-sm"
			>
				<code class="font-bold">{code}</code><span>{$t(meaning)}</span>
			</div>
		{/each}
		{#if mode === 'modbus_rtu'}
			<div
				class="rounded-box bg-base-100 flex flex-wrap items-center gap-x-3 gap-y-1 px-3 py-2 text-sm"
			>
				<code class="font-bold">{'FC08 / 0'}</code><span
					>{$t('Diagnostics: return query data (RTU only)')}</span
				>
			</div>
		{/if}
	</div>
	<p class="text-sm">
		{$t(
			'FC05: 0xFF00 = ON, 0x0000 = OFF. Send the command mailbox at 0x0200–0x0203 in one FC16 request; FC06 is not allowed there.'
		)}
	</p>
	<p class="text-sm">
		{$t(
			'Configuration writes are staged in RAM and require an authorized configuration window. Apply commits the staged profile; expiry discards unsaved changes. Access permissions still apply.'
		)}
	</p>
	<p class="text-sm opacity-75">
		{$t('The detailed tables and downloadable contract below are the original English reference.')}
	</p>
	{#each tables as table, i}
		<details class="rounded-box border-base-300 bg-base-100 min-w-0 border" open={i === 0}>
			<summary class="cursor-pointer px-3 py-3 text-sm font-semibold"
				>{$t(titles[i])} · {codes[i]}</summary
			>
			<div class="min-w-0 overflow-x-auto px-3 pb-3">
				<table class="table table-zebra table-sm" lang="en">
					<caption class="sr-only">{table.heading}</caption>
					<thead
						><tr
							>{#each table.headers as header}<th scope="col">{header}</th>{/each}</tr
						></thead
					>
					<tbody
						>{#each table.rows as row}<tr
								>{#each row as cell, column}{#if column === 0}<th
											scope="row"
											class="whitespace-nowrap font-mono font-normal">{cell}</th
										>{:else}<td class="min-w-48 whitespace-normal">{cell}</td>{/if}{/each}</tr
							>{/each}</tbody
					>
				</table>
			</div>
		</details>
	{/each}
	<details class="rounded-box border-base-300 bg-base-100 min-w-0 border">
		<summary class="cursor-pointer px-3 py-3 text-sm font-semibold"
			>{$t('Full reference (English)')}</summary
		>
		<pre
			lang="en"
			class="max-h-96 overflow-auto px-3 pb-3 font-mono text-xs whitespace-pre-wrap break-words">{source}</pre>
	</details>
	<a class="btn btn-outline btn-sm" href={download} download="actuator-modbus-map.md"
		><Download class="h-4 w-4 shrink-0" aria-hidden="true" />{$t('Download reference (English)')}</a
	>
</section>
