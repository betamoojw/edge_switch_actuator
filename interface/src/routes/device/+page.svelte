<script lang="ts">
	import { t, locale, duration } from '$lib/i18n';
	import { onMount } from 'svelte';
	import { get } from 'svelte/store';
	import { user } from '$lib/stores/user';
	import { socket } from '$lib/stores/socket';
	import { notifications } from '$lib/components/toasts/notifications';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import ModbusReference from '$lib/device/ModbusReference.svelte';
	import { reconcile } from '$lib/device/reconcile';
	import Device from '~icons/tabler/cpu';
	import Outputs from '~icons/tabler/circuit-switch-open';
	import Indicators from '~icons/tabler/bulb';
	import Button from '~icons/tabler/hand-click';
	import Modbus from '~icons/tabler/plug-connected';
	import KNX from '~icons/tabler/topology-star-3';
	import Maintenance from '~icons/tabler/tool';
	import Network from '~icons/tabler/network';
	import Clock from '~icons/tabler/clock';
	import Power from '~icons/tabler/power';
	import Pulse from '~icons/tabler/activity';
	import Settings from '~icons/tabler/adjustments';
	import Sound from '~icons/tabler/volume';
	import Save from '~icons/tabler/device-floppy';
	import Discard from '~icons/tabler/arrow-back-up';
	import Info from '~icons/tabler/info-circle';
	import Unlock from '~icons/tabler/lock-open';
	import History from '~icons/tabler/history';

	import { individualAddressError, groupAddressError } from '$lib/device/knx-address';
	import type { Config, Status, Knx } from '$lib/device/types';

	let config = $state<Config>();
	let status = $state<Status>();
	let knx = $state<Knx>();
	let section = $state('Outputs');
	let busy = $state(false);
	let pending = $state<string[]>([]);
	let refreshing: Promise<void> | undefined;
	let lastEvent = 0;
	let stateGeneration = 0;
	let active = false;
	let offline = $state('');
	let dirty = $state(false);
	let takeover = $state(false);
	let groupInputs = $state<Record<number, string>>({});
	const addressError = $derived(knx ? individualAddressError(knx.address) : '');
	const groupErrors = $derived(
		Object.fromEntries(
			Object.entries(groupInputs).map(([key, value]) => [key, groupAddressError(value)])
		)
	);
	const invalidKnx = $derived(!!addressError || Object.values(groupErrors).some(Boolean));
	let windowPeer = $state('rtu');
	let resetConfirm = $state('');
	let testColor = $state('#ffffff');
	let testSeconds = $state(5);
	let testHz = $state(2000);
	let testMs = $state(100);
	let testDuty = $state(25);
	const sections = [
		{ title: 'Outputs', icon: Outputs },
		{ title: 'Indicators', icon: Indicators },
		{ title: 'Button', icon: Button },
		{ title: 'Protocol', icon: Modbus },
		{ title: 'KNX', icon: KNX },
		{ title: 'Maintenance', icon: Maintenance }
	];
	const actions = [
		'No action',
		'Relay ON',
		'Relay OFF',
		'Relay toggle',
		'Relay pulse',
		'All OFF',
		'Identify',
		'Acknowledge sound',
		'KNX programming toggle'
	];
	const modes = [
		['off', 'Protocol Interface Off'],
		['modbus_rtu', 'Modbus RTU'],
		['modbus_tcp', 'Modbus TCP'],
		['knx_ip', 'KNXnet/IP']
	];
	async function api(path: string, body?: unknown) {
		const response = await fetch('/rest/' + path, {
			signal: AbortSignal.timeout(30000),
			method: body === undefined ? 'GET' : 'POST',
			headers: {
				Authorization: 'Bearer ' + get(user).bearer_token,
				'Content-Type': 'application/json'
			},
			body: body === undefined ? undefined : JSON.stringify(body)
		});
		if (!response.ok) {
			let message = $t('Request failed ({status})', { status: response.status });
			try {
				message = (await response.json()).error || message;
			} catch {}
			throw new Error(message);
		}
		return response.json();
	}
	function updateStatus(next: Partial<Status>) {
		if (!status) status = next as Status;
		else reconcile(status, next);
	}
	function refresh() {
		if (refreshing) return refreshing;
		const generation = stateGeneration;
		refreshing = (async () => {
			try {
				const next = await api('device/status');
				if (active) {
					// A delayed poll may refresh permissions, but cannot roll back live state.
					updateStatus(
						status && generation !== stateGeneration ? { capabilities: next.capabilities } : next
					);
					offline = '';
				}
			} catch (e) {
				if (active) offline = String(e);
			} finally {
				refreshing = undefined;
			}
		})();
		return refreshing;
	}
	async function loadConfig() {
		try {
			const next = await api('device/config');
			if (config) reconcile(config, next);
			else config = next;
			dirty = false;
		} catch (e) {
			notifications.error(String(e), 5000);
		}
	}
	async function loadKnx() {
		try {
			knx = await api('knx/config');
			groupInputs = Object.fromEntries(
				knx!.objects.map((obj) => [obj.number, obj.groups.join(', ')])
			);
			takeover = false;
		} catch (e) {
			notifications.error(String(e), 5000);
		}
	}
	async function perform(path: string, body: unknown, key = 'settings') {
		if (pending.includes(key)) return false;
		pending = [...pending, key];
		if (key === 'settings') busy = true;
		try {
			const generation = stateGeneration;
			const result = await api(path, body);
			if (result.state) {
				if (generation === stateGeneration) updateStatus(result.state);
				++stateGeneration;
			} else await refresh();
			notifications.success('Applied', 3000);
			return true;
		} catch (e) {
			notifications.error(String(e), 5000);
			return false;
		} finally {
			pending = pending.filter((item) => item !== key);
			if (key === 'settings') busy = false;
		}
	}
	async function command(command: string, extra: Record<string, unknown> = {}) {
		await perform(
			'device/commands',
			{
				command,
				requestId: `${Date.now()}-${Math.random().toString(36).slice(2)}`,
				...extra
			},
			extra.channel !== undefined ? `relay-${extra.channel}` : command
		);
	}
	async function save() {
		if (config && (await perform('device/config', config))) await loadConfig();
	}
	async function selectSection(value: string) {
		section = value;
		if (value === 'KNX') await loadKnx();
	}
	function allowed(channel: number) {
		return !!status?.capabilities.command && !!(status.capabilities.channels & (1 << channel));
	}
	const bulkAllowed = $derived(
		!!status?.capabilities.command &&
			status.relays.some((relay) => relay.enabled) &&
			status.relays.every((relay, i) => !relay.enabled || (allowed(i) && !relay.blocked))
	);
	function relayBusy(channel: number) {
		return (
			busy ||
			pending.includes(`relay-${channel}`) ||
			pending.includes('all_on') ||
			pending.includes('all_off')
		);
	}
	onMount(() => {
		active = true;
		refresh();
		loadConfig();
		const offState = socket.on<Status>('device.state', (next) => {
			lastEvent = Date.now();
			// Events carry shared state; permissions come from the authenticated REST response.
			if (status) {
				const { capabilities: _ignored, ...shared } = next;
				++stateGeneration;
				updateStatus(shared);
			}
		});
		const offOpen = socket.on('open', () => {
			refresh();
		});
		const timer = setInterval(() => {
			if (offline || Date.now() - lastEvent > 5000) refresh();
		}, 5000);
		const permissions = setInterval(refresh, 30000);
		return () => {
			active = false;
			offState();
			offOpen();
			clearInterval(timer);
			clearInterval(permissions);
		};
	});
</script>

<div class="device-page mx-0 my-1 flex min-w-0 flex-col sm:mx-8 sm:my-8">
	<SettingsCard collapsible={false} maxwidth="max-w-6xl">
		{#snippet icon()}
			<Device class="mr-2 h-6 w-6 shrink-0" aria-hidden="true" />
		{/snippet}
		{#snippet title()}
			<h1 class="text-xl font-medium">{$t('Switching Actuator')}</h1>
		{/snippet}
		<div class="space-y-1 pb-2">
			<p class="text-sm opacity-75">
				{$t('Welcome to Switching Actuator · ESP32-S3 · Six independent relay outputs')}
			</p>
			<p class="text-sm opacity-70">
				{$t('Control relay outputs, configure the protocol interface, and monitor device status.')}
			</p>
		</div>
		{#if status}<div class="mb-3 grid gap-2 sm:grid-cols-3">
				<div class="rounded-box bg-base-100 flex min-w-0 items-center gap-3 px-4 py-3">
					<div class="mask mask-hexagon bg-primary h-10 w-10 shrink-0">
						<Network class="text-primary-content h-full w-full scale-75" aria-hidden="true" />
					</div>
					<div class="min-w-0">
						<div class="text-sm font-bold">{$t('Network')}</div>
						<p class="text-sm break-words opacity-75">
							{status.networkOnline
								? `${status.networkInterface || $t('Network')} · ${status.ip}`
								: status.ap
									? $t('Provisioning AP')
									: $t('Connecting')}
						</p>
					</div>
				</div>
				<div class="rounded-box bg-base-100 flex min-w-0 items-center gap-3 px-4 py-3">
					<div class="mask mask-hexagon bg-primary h-10 w-10 shrink-0">
						<Modbus class="text-primary-content h-full w-full scale-75" aria-hidden="true" />
					</div>
					<div class="min-w-0">
						<div class="text-sm font-bold">{$t('Protocol')}</div>
						<p class="text-sm break-words opacity-75">{status.mode} · {status.protocolState}</p>
					</div>
				</div>
				<div class="rounded-box bg-base-100 flex min-w-0 items-center gap-3 px-4 py-3">
					<div class="mask mask-hexagon bg-primary h-10 w-10 shrink-0">
						<Clock class="text-primary-content h-full w-full scale-75" aria-hidden="true" />
					</div>
					<div class="min-w-0">
						<div class="text-sm font-bold">{$t('Uptime')}</div>
						<p class="text-sm tabular-nums opacity-75">{duration($locale, status.uptime)}</p>
					</div>
				</div>
			</div>{/if}
		{#if offline}<div class="alert alert-warning">
				{$t('Connection unavailable. Controls are disabled.')}
				{offline}
			</div>{/if}
		{#if status?.fault}<div class="alert alert-error">
				{$t('Fault')}
				{status.fault}: {status.error}
			</div>{/if}
		{#if status?.resetArmed}<div class="alert alert-error">
				{$t('Factory reset armed. Release BOOT before 10 seconds to cancel.')}
			</div>{/if}
		<div
			class="tabs tabs-box bg-base-100 mb-3 grid grid-cols-2 gap-1 p-1 sm:grid-cols-3 xl:grid-cols-6"
			role="tablist"
			aria-label={$t('Hardware categories')}
		>
			{#each sections as item}<button
					role="tab"
					class="tab h-auto min-h-11 gap-2 px-3 py-2 text-center whitespace-normal"
					class:tab-active={section === item.title}
					aria-selected={section === item.title}
					onclick={() => selectSection(item.title)}
					><item.icon class="h-5 w-5 shrink-0" aria-hidden="true" />{$t(item.title)}</button
				>{/each}
		</div>
		{#if config && status}
			<div class="alert alert-info" class:invisible={!dirty} aria-hidden={!dirty}>
				<span
					>{$t(
						'Unsaved settings. Apply saves the complete profile and restarts the selected protocol interface.'
					)}</span
				><button class="btn btn-sm" disabled={busy} onclick={loadConfig}
					><Discard class="h-4 w-4 shrink-0" aria-hidden="true" />{$t('Discard')}</button
				><button
					class="btn btn-primary btn-sm"
					disabled={busy || !!offline || !status.capabilities.configure}
					onclick={save}
					><Save class="h-4 w-4 shrink-0" aria-hidden="true" />{$t('Apply settings')}</button
				>
			</div>
			{#if section === 'Outputs'}
				<p class="mb-2 flex items-start gap-2 text-sm opacity-75">
					<Info class="h-5 w-5 shrink-0" aria-hidden="true" />
					{$t('States show the commanded output. This board does not measure contact position.')}
				</p>
				<div class="grid gap-4 md:grid-cols-2 xl:grid-cols-3">
					{#each config.relays as relay, i}
						<div class="card bg-base-100 border-base-300 min-w-0 border shadow-sm">
							<div class="card-body min-w-0 gap-3 p-4 sm:p-5">
								<div class="flex items-center justify-between gap-3">
									<div class="mask mask-hexagon bg-primary h-10 w-10 shrink-0">
										<Outputs
											class="text-primary-content h-full w-full scale-75"
											aria-hidden="true"
										/>
									</div>
									<h2 class="card-title min-w-0 flex-1 text-base break-words">
										{i + 1}. {relay.name}
									</h2>
									<span class="badge shrink-0" class:badge-success={status.relays[i]?.on}
										>{status.relays[i]?.on ? $t('ON') : $t('OFF')}</span
									>
								</div>
								<p class="text-sm">
									{status.relays[i]?.blocked ? $t('Blocked · ') : ''}{status.relays[i]?.enabled
										? $t('Enabled')
										: $t('Disabled')} · {status.relays[i]?.source || $t('Startup')}
								</p>
								<div class="flex flex-wrap gap-2">
									<button
										class="btn btn-primary btn-sm"
										disabled={relayBusy(i) ||
											!!offline ||
											!allowed(i) ||
											!status.relays[i]?.enabled ||
											status.relays[i]?.blocked}
										onclick={() => command('relay', { channel: i, value: !status?.relays[i]?.on })}
										><Power class="h-4 w-4 shrink-0" aria-hidden="true" />{status.relays[i]?.on
											? $t('Turn OFF')
											: $t('Turn ON')}</button
									><button
										class="btn btn-sm"
										disabled={relayBusy(i) ||
											!!offline ||
											!allowed(i) ||
											!status.relays[i]?.enabled ||
											status.relays[i]?.blocked}
										onclick={() => command('pulse', { channel: i })}
										><Pulse class="h-4 w-4 shrink-0" aria-hidden="true" />{$t('Pulse')}</button
									>
									{#if status.relays[i]?.blocked && status.capabilities.configure}<button
											class="btn btn-sm"
											onclick={() => command('unblock', { channel: i })}
											><Unlock class="h-4 w-4 shrink-0" aria-hidden="true" />{$t(
												'Clear block'
											)}</button
										>{/if}
								</div>
								{#if status.capabilities.configure}<details>
										<summary
											class="border-base-300 cursor-pointer border-t pt-3 text-sm font-medium"
											><Settings class="mr-1 inline h-4 w-4" aria-hidden="true" />{$t(
												'Channel settings'
											)}</summary
										>
										<div class="mt-3 space-y-3">
											<label class="fieldset"
												>{$t('Name')}<input
													class="input w-full"
													maxlength="32"
													bind:value={relay.name}
													oninput={() => (dirty = true)}
												/></label
											>
											<label class="label"
												><input
													class="checkbox"
													type="checkbox"
													bind:checked={relay.enabled}
													onchange={() => (dirty = true)}
												/>
												{$t('Enabled (disabling drives OFF)')}</label
											>
											<label class="label"
												><input
													class="checkbox"
													type="checkbox"
													bind:checked={relay.startup}
													onchange={() => (dirty = true)}
												/>
												{$t('ON at startup')}</label
											>
											<label class="fieldset"
												>{$t('Pulse duration (ms)')}<input
													class="input"
													type="number"
													min="10"
													max="60000"
													bind:value={relay.pulseMs}
													oninput={() => (dirty = true)}
												/></label
											>
											<label class="label"
												><input
													class="checkbox"
													type="checkbox"
													bind:checked={relay.disconnectOff}
													onchange={() => (dirty = true)}
												/>
												{$t('OFF on communications loss')}</label
											>
											<label class="fieldset"
												>{$t('Loss timeout (seconds)')}<input
													class="input"
													type="number"
													min="1"
													max="3600"
													bind:value={relay.timeout}
													oninput={() => (dirty = true)}
												/></label
											>
										</div>
									</details>{/if}
							</div>
						</div>
					{/each}
				</div>
				<div class="mt-3 flex flex-wrap gap-3">
					<button
						class="btn btn-outline"
						disabled={busy || pending.length > 0 || !!offline || !bulkAllowed}
						onclick={() => command('all_off')}
						><Power class="h-5 w-5 shrink-0" aria-hidden="true" />{$t(
							'All enabled channels OFF'
						)}</button
					>
					<button
						class="btn btn-primary"
						disabled={busy || pending.length > 0 || !!offline || !bulkAllowed}
						onclick={() => command('all_on')}
					>
						<Power class="h-5 w-5 shrink-0" aria-hidden="true" />{$t('All enabled channels ON')}
					</button>
				</div>
			{:else if section === 'Indicators'}
				<div class="grid gap-4 md:grid-cols-2">
					<div class="card bg-base-100 border-base-300 min-w-0 border shadow-sm">
						<div class="card-body min-w-0 gap-4 p-4 sm:p-5">
							<h2 class="card-title gap-3 text-lg">
								<span class="mask mask-hexagon bg-primary h-10 w-10 shrink-0"
									><Indicators
										class="text-primary-content h-full w-full scale-75"
										aria-hidden="true"
									/></span
								>{$t('RGB status indicator')}
							</h2>
							<p>
								{$t(
									'Blue: AP · Amber: connecting · Green: network connected · Red: fault/programming'
								)}
							</p>
							<p>{$t('Current RGB:')} {status.color.join(', ')}</p>
							<label class="label"
								><input
									class="checkbox"
									type="checkbox"
									bind:checked={config.rgb}
									disabled={!status.capabilities.configure}
									onchange={() => (dirty = true)}
								/>
								{$t('Enabled')}</label
							>
							<label class="fieldset"
								>{$t('Brightness (%)')}<input
									class="range"
									type="range"
									min="0"
									max="100"
									bind:value={config.brightness}
									disabled={!status.capabilities.configure}
									oninput={() => (dirty = true)}
								/>{config.brightness}%</label
							>
							<label class="fieldset"
								>{$t('Test color')}<input
									class="input"
									type="color"
									bind:value={testColor}
								/></label
							>
							<label class="fieldset"
								>{$t('Test duration (seconds)')}<input
									class="input"
									type="number"
									min="1"
									max="30"
									bind:value={testSeconds}
								/></label
							>
							<button
								class="btn"
								disabled={busy ||
									pending.includes('rgb') ||
									!!offline ||
									!status.capabilities.command ||
									!config.rgb}
								onclick={() =>
									command('rgb', {
										red: parseInt(testColor.slice(1, 3), 16),
										green: parseInt(testColor.slice(3, 5), 16),
										blue: parseInt(testColor.slice(5, 7), 16),
										brightness: config?.brightness ?? 10,
										seconds: testSeconds
									})}>{$t('Test color')}</button
							>
							<button
								class="btn"
								disabled={busy ||
									pending.includes('identify') ||
									!!offline ||
									!status.capabilities.command}
								onclick={() => command('identify')}>{$t('Identify for 5 seconds')}</button
							>
						</div>
					</div>
					<div class="card bg-base-100 border-base-300 min-w-0 border shadow-sm">
						<div class="card-body min-w-0 gap-4 p-4 sm:p-5">
							<h2 class="card-title gap-3 text-lg">
								<span class="mask mask-hexagon bg-primary h-10 w-10 shrink-0"
									><Sound
										class="text-primary-content h-full w-full scale-75"
										aria-hidden="true"
									/></span
								>{$t('Buzzer')}
							</h2>
							<p>
								{$t('Connection success and bounded fault tones. Current:')}
								{status.toneHz}
								{$t('Hz')}
							</p>
							<label class="label"
								><input
									class="checkbox"
									type="checkbox"
									bind:checked={config.buzzer}
									disabled={!status.capabilities.configure}
									onchange={() => (dirty = true)}
								/>
								{$t('Enabled')}</label
							>
							<label class="fieldset"
								>{$t('Frequency (Hz)')}<input
									class="input"
									type="number"
									min="500"
									max="4000"
									bind:value={testHz}
								/></label
							>
							<label class="fieldset"
								>{$t('Duration (ms)')}<input
									class="input"
									type="number"
									min="10"
									max="2000"
									bind:value={testMs}
								/></label
							>
							<label class="fieldset"
								>{$t('Duty (%)')}<input
									class="input"
									type="number"
									min="1"
									max="50"
									bind:value={testDuty}
								/></label
							>
							<button
								class="btn"
								disabled={busy ||
									pending.includes('tone') ||
									!!offline ||
									!status.capabilities.command ||
									!config.buzzer}
								onclick={() => command('tone', { hz: testHz, ms: testMs, duty: testDuty })}
								>{$t('Test tone')}</button
							><button
								class="btn"
								disabled={busy ||
									pending.includes('acknowledge') ||
									!!offline ||
									!status.capabilities.command}
								onclick={() => command('acknowledge')}>{$t('Silence current tone')}</button
							>
						</div>
					</div>
				</div>
			{:else if section === 'Button'}
				<div class="card bg-base-100 border-base-300 min-w-0 border shadow-sm">
					<div class="card-body min-w-0 gap-4 p-4 sm:p-5">
						<h2 class="card-title gap-3 text-lg">
							<span class="mask mask-hexagon bg-primary h-10 w-10 shrink-0"
								><Button
									class="text-primary-content h-full w-full scale-75"
									aria-hidden="true"
								/></span
							>{$t('BOOT button')}
						</h2>
						<p>
							{$t('{state} · Last gesture {gesture} · Events {count}', {
								state: $t(status.pressed ? 'Pressed' : 'Released'),
								gesture: status.lastGesture,
								count: status.gestureCount
							})}
						</p>
						<label class="label"
							><input
								class="checkbox"
								type="checkbox"
								bind:checked={config.button}
								disabled={!status.capabilities.configure}
								onchange={() => (dirty = true)}
							/>
							{$t('Enable application click actions')}</label
						>
						{#each config.clicks as binding, i}<div
								class="rounded-box bg-base-200 grid items-end gap-3 p-3 sm:grid-cols-2"
							>
								<label class="fieldset"
									>{[$t('Single click'), $t('Double click'), $t('Triple click')][i]}<select
										class="select"
										bind:value={binding.action}
										disabled={!status.capabilities.configure}
										onchange={() => {
											dirty = true;
											if (binding.action < 1 || binding.action > 4) binding.target = 0;
										}}
										>{#each actions as action, n}<option value={n}>{$t(action)}</option
											>{/each}</select
									></label
								><label class="fieldset"
									>{$t('Target')}<select
										class="select"
										bind:value={binding.target}
										disabled={!status.capabilities.configure ||
											binding.action < 1 ||
											binding.action > 4}
										onchange={() => (dirty = true)}
										><option value={0}
											>{binding.action >= 1 && binding.action <= 4
												? $t('All channels')
												: $t('Device')}</option
										>{#each config.relays as r, c}<option value={c + 1}>{r.name}</option
											>{/each}</select
									></label
								>
							</div>{/each}
						<div class="alert alert-warning mt-4">
							{$t(
								'Holding BOOT for 10 seconds erases user settings and KNX commissioning. This recovery gesture remains active when click actions are disabled. Release before 10 seconds to cancel.'
							)}
						</div>
					</div>
				</div>
			{:else if section === 'Protocol'}
				<div class="card bg-base-100 border-base-300 min-w-0 border shadow-sm">
					<div class="card-body min-w-0 gap-4 p-4 sm:p-5">
						<h2 class="card-title gap-3 text-lg">
							<span class="mask mask-hexagon bg-primary h-10 w-10 shrink-0"
								><Modbus
									class="text-primary-content h-full w-full scale-75"
									aria-hidden="true"
								/></span
							>{$t('Protocol Interface selection')}
						</h2>
						<p>{$t('Only one of RTU, TCP and KNX can operate at a time.')}</p>
						<label class="fieldset"
							>{$t('Protocol')}<select
								class="select"
								bind:value={config.mode}
								disabled={!status.capabilities.configure}
								onchange={() => (dirty = true)}
								>{#each modes as mode}<option value={mode[0]}>{$t(mode[1])}</option>{/each}</select
							></label
						>
						<div class="grid gap-5 md:grid-cols-2">
							<fieldset
								class="rounded-box border-base-300 min-w-0 space-y-3 border p-4"
								disabled={!status.capabilities.configure}
							>
								<legend class="font-bold">{$t('RS485 / Modbus RTU')}</legend>
								<label class="label"
									><input
										class="checkbox"
										type="checkbox"
										bind:checked={config.rs485}
										onchange={() => {
											dirty = true;
											if (!config?.rs485 && config?.mode === 'modbus_rtu') config.mode = 'off';
										}}
									/>
									{$t('RS485 enabled')}</label
								>
								<label class="fieldset"
									>{$t('Unit address')}<input
										class="input"
										type="number"
										min="1"
										max="247"
										bind:value={config.unit}
										oninput={() => (dirty = true)}
									/></label
								>
								<label class="fieldset"
									>{$t('Baud')}<select
										class="select"
										bind:value={config.baud}
										onchange={() => (dirty = true)}
										>{#each [9600, 19200, 38400, 57600, 115200] as baud, i}<option value={i}
												>{baud}</option
											>{/each}</select
									></label
								>
								<label class="fieldset"
									>{$t('Serial format')}<select
										class="select"
										bind:value={config.serialFormat}
										onchange={() => (dirty = true)}
										>{#each ['8E1', '8O1', '8N2', '8N1 (compatibility)'] as format, i}<option
												value={i}>{$t(format)}</option
											>{/each}</select
									></label
								>
								<label class="label"
									><input
										class="checkbox"
										type="checkbox"
										bind:checked={config.busWatchdog}
										onchange={() => (dirty = true)}
									/>
									{$t('Enable RTU polling watchdog')}</label
								>
							</fieldset>
							<fieldset
								class="rounded-box border-base-300 min-w-0 space-y-3 border p-4"
								disabled={!status.capabilities.configure}
							>
								<legend class="font-bold">{$t('Modbus TCP')}</legend>
								<label class="fieldset"
									>{$t('TCP port')}<input
										class="input"
										type="number"
										min="1"
										max="65535"
										bind:value={config.tcpPort}
										oninput={() => (dirty = true)}
									/></label
								>
								<label class="fieldset"
									>{$t('Allowed source IPv4 (empty allows uplink peers)')}<input
										class="input"
										bind:value={config.tcpAllow}
										oninput={() => (dirty = true)}
									/></label
								>
								<p>
									{$t('Active IPv4 uplink only. Up to four clients. Idle timeout: 60 seconds.')}
								</p>
							</fieldset>
						</div>
						<fieldset
							disabled={!status.capabilities.configure}
							class="rounded-box border-base-300 min-w-0 space-y-3 border p-4"
						>
							<legend class="font-bold">{$t('Protocol Interface access')}</legend><label
								class="fieldset"
								>{$t('Relay write mask (0–63)')}<input
									class="input"
									type="number"
									min="0"
									max="63"
									bind:value={config.busMask}
									oninput={() => (dirty = true)}
								/></label
							><label class="label"
								><input
									class="checkbox"
									type="checkbox"
									bind:checked={config.busIndicators}
									onchange={() => (dirty = true)}
								/>
								{$t('Allow manual indicators and diagnostic commands')}</label
							>
						</fieldset>
						<p>
							{$t('Requests')}
							{status.requests}
							{$t('· Frame errors')}
							{status.frameErrors}
							{$t('· Rejected')}
							{status.rejected}
							{$t('· TCP clients')}
							{status.tcpClients}
						</p>
						{#if status.capabilities.configure}<label class="fieldset"
								>{$t('Configuration window source (rtu or exact TCP IPv4)')}<input
									class="input"
									bind:value={windowPeer}
								/></label
							>
							<p class="text-sm">
								{$t(
									'RTU cannot authenticate masters: every physical bus participant can use its configuration window.'
								)}
							</p>
							<button
								class="btn"
								disabled={busy || !!offline}
								onclick={() => command('modbus_window', { seconds: 60, peer: windowPeer })}
								>{$t('Open 60-second configuration window')}</button
							>
							<p>{status.configWindow ? $t('Window open') : $t('Window closed')}</p>{/if}
						{#if config.mode === 'modbus_rtu' || config.mode === 'modbus_tcp'}
							<ModbusReference mode={config.mode} />
						{/if}
					</div>
				</div>
			{:else if section === 'KNX'}
				<div class="card bg-base-100 border-base-300 min-w-0 border shadow-sm">
					<div class="card-body min-w-0 gap-4 p-4 sm:p-5">
						<h2 class="card-title gap-3 text-lg">
							<span class="mask mask-hexagon bg-primary h-10 w-10 shrink-0"
								><KNX
									class="text-primary-content h-full w-full scale-75"
									aria-hidden="true"
								/></span
							>{$t('KNXnet/IP commissioning')}
						</h2>
						<p>
							{status.knxConfigured
								? $t('Application configured')
								: $t('Application not configured')}
							{$t('· Programming')}
							{status.programming ? $t('ON') : $t('OFF')}
						</p>
						<button
							class="btn btn-error"
							disabled={busy ||
								!!offline ||
								!status.capabilities.configure ||
								status.mode !== 'knx_ip'}
							onclick={() => perform('knx/programming', { active: !status?.programming })}
							>{$t(status.programming ? 'Exit programming mode' : 'Enter programming mode')}</button
						>
						<p>
							{$t(
								'Select KNXnet/IP in Protocol Interface selection, then Apply. Hardware triple-click and this control share programming state.'
							)}
						</p>
						{#if knx}<div class="alert alert-warning">
								{$t(
									'Development product identity. ETS interoperability and hardware qualification are required before production release.'
								)}
							</div>
							<p>
								{$t('Owner: {owner} · Revision {revision} · {state}', {
									owner: knx.owner,
									revision: knx.revision,
									state: $t(knx.busy ? 'ETS download busy' : 'Ready')
								})}
							</p>
							<fieldset
								disabled={!status.capabilities.configure || status.mode !== 'knx_ip' || knx.busy}
								class="min-w-0 space-y-4"
							>
								<label class="fieldset"
									>{$t('Individual address')}<input
										class="input"
										bind:value={knx.address}
										placeholder={$t('15.15.255 (Factory Default)')}
										aria-invalid={!!addressError}
										aria-describedby="knx-address-help knx-address-error"
									/></label
								>
								<p id="knx-address-help" class="text-sm">
									{$t('Area.line.device: 0–15.0–15.1–255. Factory default:')}
									<strong>15.15.255</strong>.
								</p>
								<p id="knx-address-error" class="text-error text-sm" aria-live="polite">
									{$t(addressError)}
								</p>
								<label class="label"
									><input class="checkbox" type="checkbox" bind:checked={takeover} />
									{$t(
										'Take over for web editing (a later ETS download can replace these changes)'
									)}</label
								>
								<p id="knx-group-help" class="text-sm">
									{$t(
										'Main/middle/sub: 0–31/0–7/0–255; 0/0/0 is reserved. Leave blank for no association. Up to 8 unique addresses per object; sending address first.'
									)}
								</p>
								<div class="rounded-box border-base-300 min-w-0 overflow-x-auto border">
									<table class="table table-zebra">
										<thead
											><tr
												><th>{$t('Object')}</th><th>{$t('Function')}</th><th
													>{$t('Group addresses (comma separated, sending address first)')}</th
												></tr
											></thead
										><tbody
											>{#each knx.objects as obj}<tr
													><td>{obj.number}</td><td
														>{$t('CH')}
														{Math.ceil(obj.number / 3)}
														{[$t('Status'), $t('Switch'), $t('Block')][obj.number % 3]}</td
													><td
														><input
															class="input w-full min-w-64"
															placeholder="1/0/1, 1/0/2"
															aria-label={$t('Group addresses for object {number}', {
																number: obj.number
															})}
															aria-invalid={!!groupErrors[obj.number]}
															aria-describedby={`knx-group-help knx-group-error-${obj.number}`}
															bind:value={groupInputs[obj.number]}
														/>
														<p
															id={`knx-group-error-${obj.number}`}
															class="text-error text-sm"
															aria-live="polite"
														>
															{$t(groupErrors[obj.number])}
														</p></td
													></tr
												>{/each}</tbody
										>
									</table>
								</div>
								<h3 class="flex items-center gap-2 font-bold">
									<Settings class="h-5 w-5 shrink-0" aria-hidden="true" />{$t(
										'Application parameters'
									)}
								</h3>
								<div class="grid gap-3 md:grid-cols-2">
									{#each knx.parameters as r, i}<div
											class="rounded-box bg-base-200 min-w-0 space-y-3 p-4"
										>
											<h4>{$t('Channel')} {i + 1}</h4>
											<label class="label"
												><input type="checkbox" class="checkbox" bind:checked={r.enabled} />
												{$t('Enabled')}</label
											><label class="label"
												><input type="checkbox" class="checkbox" bind:checked={r.startup} />
												{$t('Startup ON')}</label
											><label class="label"
												><input type="checkbox" class="checkbox" bind:checked={r.disconnectOff} />
												{$t('OFF on loss')}</label
											><label class="fieldset"
												>{$t('Loss timeout (s)')}<input
													class="input"
													type="number"
													min="1"
													max="3600"
													bind:value={r.timeout}
												/></label
											><label class="fieldset"
												>{$t('Pulse (ms)')}<input
													class="input"
													type="number"
													min="10"
													max="60000"
													bind:value={r.pulseMs}
												/></label
											>
										</div>{/each}
								</div>
								<button
									class="btn btn-primary"
									disabled={busy || !!offline || invalidKnx}
									onclick={async () => {
										if (!knx || invalidKnx) return;
										const objects = knx.objects.map((obj) => ({
											...obj,
											groups: groupInputs[obj.number].trim()
												? groupInputs[obj.number].split(',').map((v) => v.trim())
												: []
										}));
										if (await perform('knx/config', { ...knx, objects, takeover })) await loadKnx();
									}}>{$t('Apply KNX commissioning')}</button
								>
							</fieldset>
							<button class="btn" onclick={loadKnx}>{$t('Reload KNX snapshot')}</button>
						{/if}
					</div>
				</div>
			{:else}
				<div class="card bg-base-100 border-base-300 min-w-0 border shadow-sm">
					<div class="card-body min-w-0 gap-4 p-4 sm:p-5">
						<h2 class="card-title gap-3 text-lg">
							<span class="mask mask-hexagon bg-primary h-10 w-10 shrink-0"
								><Maintenance
									class="text-primary-content h-full w-full scale-75"
									aria-hidden="true"
								/></span
							>{$t('Operations')}
						</h2>
						<p>
							{$t('Uptime')}
							{status.uptime}{$t('s · Configuration revision')}
							{status.revision}
						</p>
						{#if status.capabilities.admin}<label class="fieldset"
								>{$t('Type ERASE to factory reset')}<input
									class="input"
									bind:value={resetConfirm}
								/></label
							><button
								class="btn btn-error"
								disabled={busy || !!offline || resetConfirm !== 'ERASE'}
								onclick={() => command('factory_reset', { confirm: resetConfirm })}
								>{$t('Erase configuration and restart')}</button
							>{/if}
						<h3 class="mt-2 flex items-center gap-2 border-t border-base-300 pt-4 font-bold">
							<History class="h-5 w-5 shrink-0" aria-hidden="true" />{$t('Recent activity')}
						</h3>
						<ul
							class="rounded-box bg-base-200 divide-base-300 divide-y px-4 font-mono text-xs leading-relaxed"
						>
							{#each status.audit as line}<li class="break-words py-2">{line}</li>{/each}
						</ul>
					</div>
				</div>
			{/if}
		{:else}<div class="skeleton h-48 w-full"></div>{/if}
	</SettingsCard>
</div>

<style>
	.device-page :global(.label) {
		white-space: normal;
		align-items: flex-start;
		gap: 0.75rem;
	}
	.device-page :global(.checkbox) {
		flex-shrink: 0;
	}
	.device-page :global(.fieldset) {
		min-width: 0;
	}
	.device-page :global(.input),
	.device-page :global(.select),
	.device-page :global(.range) {
		width: 100%;
		max-width: 100%;
	}
	.device-page :global(.btn) {
		height: auto;
		min-height: 2.5rem;
		white-space: normal;
		padding-block: 0.5rem;
	}
	.device-page :global(.btn-sm) {
		min-height: 2rem;
		padding-block: 0.25rem;
	}
	.device-page :global(.card-body > p) {
		flex-grow: 0;
		font-size: 0.875rem;
		overflow-wrap: anywhere;
	}
</style>
