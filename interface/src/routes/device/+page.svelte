<script lang="ts">
	import { onMount } from 'svelte';
	import { get } from 'svelte/store';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';

	import type { Config, Status, Knx } from '$lib/device/types';

	let config = $state<Config>();
	let status = $state<Status>();
	let knx = $state<Knx>();
	let section = $state('Outputs');
	let busy = $state(false);
	let offline = $state('');
	let dirty = $state(false);
	let takeover = $state(false);
	let windowPeer = $state('rtu');
	let resetConfirm = $state('');
	let testColor = $state('#ffffff');
	let testSeconds = $state(5);
	let testHz = $state(2000);
	let testMs = $state(100);
	let testDuty = $state(25);
	const sections = ['Outputs', 'Indicators', 'Button', 'Modbus', 'KNX', 'Maintenance'];
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
		['off', 'Fieldbus off'],
		['modbus_rtu', 'Modbus RTU'],
		['modbus_tcp', 'Modbus TCP'],
		['knx_ip', 'KNXnet/IP']
	];
	async function api(path: string, body?: unknown) {
		const response = await fetch('/rest/' + path, {
			method: body === undefined ? 'GET' : 'POST',
			headers: {
				Authorization: 'Bearer ' + get(user).bearer_token,
				'Content-Type': 'application/json'
			},
			body: body === undefined ? undefined : JSON.stringify(body)
		});
		if (!response.ok) {
			let message = `Request failed (${response.status})`;
			try {
				message = (await response.json()).error || message;
			} catch {}
			throw new Error(message);
		}
		return response.json();
	}
	async function refresh() {
		try {
			status = await api('device/status');
			offline = '';
		} catch (e) {
			offline = String(e);
		}
	}
	async function loadConfig() {
		try {
			config = await api('device/config');
			dirty = false;
		} catch (e) {
			notifications.error(String(e), 5000);
		}
	}
	async function loadKnx() {
		try {
			knx = await api('knx/config');
			takeover = false;
		} catch (e) {
			notifications.error(String(e), 5000);
		}
	}
	async function perform(path: string, body: unknown) {
		busy = true;
		try {
			await api(path, body);
			await refresh();
			notifications.success('Applied', 3000);
			return true;
		} catch (e) {
			notifications.error(String(e), 5000);
			return false;
		} finally {
			busy = false;
		}
	}
	async function command(command: string, extra: Record<string, unknown> = {}) {
		await perform('device/commands', {
			command,
			requestId: `${Date.now()}-${Math.random().toString(36).slice(2)}`,
			...extra
		});
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
	onMount(() => {
		refresh();
		loadConfig();
		const timer = setInterval(refresh, 2000);
		return () => clearInterval(timer);
	});
</script>

<div class="mx-2 my-4 space-y-5 sm:mx-8">
	<div class="flex flex-wrap items-center justify-between gap-3">
		<div>
			<h1 class="text-3xl font-bold">Switching Actuator</h1>
			<p class="opacity-70">ESP32-S3 · Six independent relay outputs</p>
		</div>
		{#if status}<div class="flex flex-wrap gap-2">
				<span class:badge-success={status.networkOnline} class="badge"
					>{status.networkOnline
						? `${status.networkInterface || 'Network'} · ${status.ip}`
						: status.ap
							? 'Provisioning AP'
							: 'Connecting'}</span
				><span class="badge badge-outline">{status.mode} · {status.protocolState}</span>
			</div>{/if}
	</div>
	{#if offline}<div class="alert alert-warning">
			Connection unavailable. Controls are disabled. {offline}
		</div>{/if}
	{#if status?.fault}<div class="alert alert-error">Fault {status.fault}: {status.error}</div>{/if}
	{#if status?.resetArmed}<div class="alert alert-error">
			Factory reset armed. Release BOOT before 10 seconds to cancel.
		</div>{/if}
	<div class="tabs tabs-box flex-wrap" role="tablist" aria-label="Hardware categories">
		{#each sections as item}<button
				role="tab"
				class="tab"
				class:tab-active={section === item}
				aria-selected={section === item}
				onclick={() => selectSection(item)}>{item}</button
			>{/each}
	</div>
	{#if config && status}
		{#if dirty}<div class="alert alert-info">
				<span
					>Unsaved settings. Apply saves the complete profile and restarts the selected fieldbus.</span
				><button class="btn btn-sm" disabled={busy} onclick={loadConfig}>Discard</button><button
					class="btn btn-primary btn-sm"
					disabled={busy || !!offline || !status.capabilities.configure}
					onclick={save}>Apply settings</button
				>
			</div>{/if}
		{#if section === 'Outputs'}
			<p class="text-sm opacity-70">
				States show the commanded output. This board does not measure contact position.
			</p>
			<div class="grid gap-4 md:grid-cols-2 xl:grid-cols-3">
				{#each config.relays as relay, i}
					<div class="card bg-base-200">
						<div class="card-body gap-3">
							<div class="flex items-center justify-between">
								<h2 class="card-title">{i + 1}. {relay.name}</h2>
								<span class="badge" class:badge-success={status.relays[i]?.on}
									>{status.relays[i]?.on ? 'ON' : 'OFF'}</span
								>
							</div>
							<p class="text-sm">
								{status.relays[i]?.blocked ? 'Blocked · ' : ''}{status.relays[i]?.enabled
									? 'Enabled'
									: 'Disabled'} · {status.relays[i]?.source || 'Startup'}
							</p>
							<div class="flex gap-2">
								<button
									class="btn btn-primary btn-sm"
									disabled={busy ||
										!!offline ||
										!allowed(i) ||
										!status.relays[i]?.enabled ||
										status.relays[i]?.blocked}
									onclick={() => command('relay', { channel: i, value: !status?.relays[i]?.on })}
									>{status.relays[i]?.on ? 'Turn OFF' : 'Turn ON'}</button
								><button
									class="btn btn-sm"
									disabled={busy ||
										!!offline ||
										!allowed(i) ||
										!status.relays[i]?.enabled ||
										status.relays[i]?.blocked}
									onclick={() => command('pulse', { channel: i })}>Pulse</button
								>
								{#if status.relays[i]?.blocked && status.capabilities.configure}<button
										class="btn btn-sm"
										onclick={() => command('unblock', { channel: i })}>Clear block</button
									>{/if}
							</div>
							{#if status.capabilities.configure}<details>
									<summary class="cursor-pointer">Channel settings</summary>
									<div class="mt-3 space-y-3">
										<label class="fieldset"
											>Name<input
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
											/> Enabled (disabling drives OFF)</label
										>
										<label class="label"
											><input
												class="checkbox"
												type="checkbox"
												bind:checked={relay.startup}
												onchange={() => (dirty = true)}
											/> ON at startup</label
										>
										<label class="fieldset"
											>Pulse duration (ms)<input
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
											/> OFF on communications loss</label
										>
										<label class="fieldset"
											>Loss timeout (seconds)<input
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
			<button
				class="btn"
				disabled={busy || !!offline || !status.capabilities.command}
				onclick={() => command('all_off')}>All enabled channels OFF</button
			>
		{:else if section === 'Indicators'}
			<div class="grid gap-4 md:grid-cols-2">
				<div class="card bg-base-200">
					<div class="card-body">
						<h2 class="card-title">RGB status indicator</h2>
						<p>Blue: AP · Amber: connecting · Green: network connected · Red: fault/programming</p>
						<p>Current RGB: {status.color.join(', ')}</p>
						<label class="label"
							><input
								class="checkbox"
								type="checkbox"
								bind:checked={config.rgb}
								disabled={!status.capabilities.configure}
								onchange={() => (dirty = true)}
							/> Enabled</label
						>
						<label class="fieldset"
							>Brightness (%)<input
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
							>Test color<input class="input" type="color" bind:value={testColor} /></label
						>
						<label class="fieldset"
							>Test duration (seconds)<input
								class="input"
								type="number"
								min="1"
								max="30"
								bind:value={testSeconds}
							/></label
						>
						<button
							class="btn"
							disabled={busy || !!offline || !status.capabilities.command || !config.rgb}
							onclick={() =>
								command('rgb', {
									red: parseInt(testColor.slice(1, 3), 16),
									green: parseInt(testColor.slice(3, 5), 16),
									blue: parseInt(testColor.slice(5, 7), 16),
									brightness: config?.brightness ?? 10,
									seconds: testSeconds
								})}>Test color</button
						>
						<button
							class="btn"
							disabled={busy || !!offline || !status.capabilities.command}
							onclick={() => command('identify')}>Identify for 5 seconds</button
						>
					</div>
				</div>
				<div class="card bg-base-200">
					<div class="card-body">
						<h2 class="card-title">Buzzer</h2>
						<p>Connection success and bounded fault tones. Current: {status.toneHz} Hz</p>
						<label class="label"
							><input
								class="checkbox"
								type="checkbox"
								bind:checked={config.buzzer}
								disabled={!status.capabilities.configure}
								onchange={() => (dirty = true)}
							/> Enabled</label
						>
						<label class="fieldset"
							>Frequency (Hz)<input
								class="input"
								type="number"
								min="500"
								max="4000"
								bind:value={testHz}
							/></label
						>
						<label class="fieldset"
							>Duration (ms)<input
								class="input"
								type="number"
								min="10"
								max="2000"
								bind:value={testMs}
							/></label
						>
						<label class="fieldset"
							>Duty (%)<input
								class="input"
								type="number"
								min="1"
								max="50"
								bind:value={testDuty}
							/></label
						>
						<button
							class="btn"
							disabled={busy || !!offline || !status.capabilities.command || !config.buzzer}
							onclick={() => command('tone', { hz: testHz, ms: testMs, duty: testDuty })}
							>Test tone</button
						><button
							class="btn"
							disabled={busy || !!offline || !status.capabilities.command}
							onclick={() => command('acknowledge')}>Silence current tone</button
						>
					</div>
				</div>
			</div>
		{:else if section === 'Button'}
			<div class="card bg-base-200">
				<div class="card-body">
					<h2 class="card-title">BOOT button</h2>
					<p>
						{status.pressed ? 'Pressed' : 'Released'} · Last gesture {status.lastGesture} · Events {status.gestureCount}
					</p>
					<label class="label"
						><input
							class="checkbox"
							type="checkbox"
							bind:checked={config.button}
							disabled={!status.capabilities.configure}
							onchange={() => (dirty = true)}
						/> Enable application click actions</label
					>
					{#each config.clicks as binding, i}<div class="flex flex-wrap items-end gap-3">
							<label class="fieldset"
								>{['Single click', 'Double click', 'Triple click'][i]}<select
									class="select"
									bind:value={binding.action}
									disabled={!status.capabilities.configure}
									onchange={() => {
										dirty = true;
										if (binding.action < 1 || binding.action > 4) binding.target = 0;
									}}
									>{#each actions as action, n}<option value={n}>{action}</option>{/each}</select
								></label
							><label class="fieldset"
								>Target<select
									class="select"
									bind:value={binding.target}
									disabled={!status.capabilities.configure ||
										binding.action < 1 ||
										binding.action > 4}
									onchange={() => (dirty = true)}
									><option value={0}>Device</option>{#each config.relays as r, c}<option
											value={c + 1}>{r.name}</option
										>{/each}</select
								></label
							>
						</div>{/each}
					<div class="alert alert-warning mt-4">
						Holding BOOT for 10 seconds erases user settings and KNX commissioning. This recovery
						gesture remains active when click actions are disabled. Release before 10 seconds to
						cancel.
					</div>
				</div>
			</div>
		{:else if section === 'Modbus'}
			<div class="card bg-base-200">
				<div class="card-body">
					<h2 class="card-title">Fieldbus selection</h2>
					<p>Only one of RTU, TCP and KNX can operate at a time.</p>
					<label class="fieldset"
						>Protocol<select
							class="select"
							bind:value={config.mode}
							disabled={!status.capabilities.configure}
							onchange={() => (dirty = true)}
							>{#each modes as mode}<option value={mode[0]}>{mode[1]}</option>{/each}</select
						></label
					>
					<div class="grid gap-5 md:grid-cols-2">
						<fieldset class="space-y-3" disabled={!status.capabilities.configure}>
							<legend class="font-bold">RS485 / Modbus RTU</legend>
							<label class="label"
								><input
									class="checkbox"
									type="checkbox"
									bind:checked={config.rs485}
									onchange={() => {
										dirty = true;
										if (!config?.rs485 && config?.mode === 'modbus_rtu') config.mode = 'off';
									}}
								/> RS485 enabled</label
							>
							<label class="fieldset"
								>Unit address<input
									class="input"
									type="number"
									min="1"
									max="247"
									bind:value={config.unit}
									oninput={() => (dirty = true)}
								/></label
							>
							<label class="fieldset"
								>Baud<select class="select" bind:value={config.baud} onchange={() => (dirty = true)}
									>{#each [9600, 19200, 38400, 57600, 115200] as baud, i}<option value={i}
											>{baud}</option
										>{/each}</select
								></label
							>
							<label class="fieldset"
								>Serial format<select
									class="select"
									bind:value={config.serialFormat}
									onchange={() => (dirty = true)}
									>{#each ['8E1', '8O1', '8N2', '8N1 (compatibility)'] as format, i}<option
											value={i}>{format}</option
										>{/each}</select
								></label
							>
							<label class="label"
								><input
									class="checkbox"
									type="checkbox"
									bind:checked={config.busWatchdog}
									onchange={() => (dirty = true)}
								/> Enable RTU polling watchdog</label
							>
						</fieldset>
						<fieldset class="space-y-3" disabled={!status.capabilities.configure}>
							<legend class="font-bold">Modbus TCP</legend>
							<label class="fieldset"
								>TCP port<input
									class="input"
									type="number"
									min="1"
									max="65535"
									bind:value={config.tcpPort}
									oninput={() => (dirty = true)}
								/></label
							>
							<label class="fieldset"
								>Allowed source IPv4 (empty allows uplink peers)<input
									class="input"
									bind:value={config.tcpAllow}
									oninput={() => (dirty = true)}
								/></label
							>
							<p>Active IPv4 uplink only. Up to four clients. Idle timeout: 60 seconds.</p>
						</fieldset>
					</div>
					<fieldset disabled={!status.capabilities.configure} class="space-y-3">
						<legend class="font-bold">Fieldbus access</legend><label class="fieldset"
							>Relay write mask (0–63)<input
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
							/> Allow manual indicators and diagnostic commands</label
						>
					</fieldset>
					<p>
						Requests {status.requests} · Frame errors {status.frameErrors} · Rejected {status.rejected}
						· TCP clients {status.tcpClients}
					</p>
					{#if status.capabilities.configure}<label class="fieldset"
							>Configuration window source (rtu or exact TCP IPv4)<input
								class="input"
								bind:value={windowPeer}
							/></label
						>
						<p class="text-sm">
							RTU cannot authenticate masters: every physical bus participant can use its
							configuration window.
						</p>
						<button
							class="btn"
							disabled={busy || !!offline}
							onclick={() => command('modbus_window', { seconds: 60, peer: windowPeer })}
							>Open 60-second configuration window</button
						>
						<p>{status.configWindow ? 'Window open' : 'Window closed'}</p>{/if}
				</div>
			</div>
		{:else if section === 'KNX'}
			<div class="card bg-base-200">
				<div class="card-body">
					<h2 class="card-title">KNXnet/IP commissioning</h2>
					<p>
						{status.knxConfigured ? 'Application configured' : 'Application not configured'} · Programming
						{status.programming ? 'ON' : 'OFF'}
					</p>
					<button
						class="btn btn-error"
						disabled={busy ||
							!!offline ||
							!status.capabilities.configure ||
							status.mode !== 'knx_ip'}
						onclick={() => perform('knx/programming', { active: !status?.programming })}
						>{status.programming ? 'Exit' : 'Enter'} programming mode</button
					>
					<p>
						Select KNXnet/IP in Fieldbus selection, then Apply. Hardware triple-click and this
						control share programming state.
					</p>
					{#if knx}<div class="alert alert-warning">
							Development product identity. ETS interoperability and hardware qualification are
							required before production release.
						</div>
						<p>
							Owner: {knx.owner} · Revision {knx.revision} · {knx.busy
								? 'ETS download busy'
								: 'Ready'}
						</p>
						<fieldset
							disabled={!status.capabilities.configure || status.mode !== 'knx_ip' || knx.busy}
							class="space-y-4"
						>
							<label class="fieldset"
								>Individual address<input
									class="input"
									bind:value={knx.address}
									placeholder="1.1.10"
								/></label
							>
							<label class="label"
								><input class="checkbox" type="checkbox" bind:checked={takeover} /> Take over for web
								editing (a later ETS download can replace these changes)</label
							>
							<div class="overflow-x-auto">
								<table class="table">
									<thead
										><tr
											><th>Object</th><th>Function</th><th
												>Group addresses (comma separated, sending address first)</th
											></tr
										></thead
									><tbody
										>{#each knx.objects as obj}<tr
												><td>{obj.number}</td><td
													>CH {Math.ceil(obj.number / 3)}
													{['Status', 'Switch', 'Block'][obj.number % 3]}</td
												><td
													><input
														class="input w-full min-w-64"
														value={obj.groups.join(', ')}
														onchange={(e) =>
															(obj.groups = e.currentTarget.value
																.split(',')
																.map((v) => v.trim())
																.filter(Boolean))}
													/></td
												></tr
											>{/each}</tbody
									>
								</table>
							</div>
							<h3 class="font-bold">Application parameters</h3>
							<div class="grid gap-3 md:grid-cols-2">
								{#each knx.parameters as r, i}<div class="rounded-box border border-base-300 p-3">
										<h4>Channel {i + 1}</h4>
										<label class="label"
											><input type="checkbox" class="checkbox" bind:checked={r.enabled} /> Enabled</label
										><label class="label"
											><input type="checkbox" class="checkbox" bind:checked={r.startup} /> Startup ON</label
										><label class="label"
											><input type="checkbox" class="checkbox" bind:checked={r.disconnectOff} /> OFF
											on loss</label
										><label class="fieldset"
											>Loss timeout (s)<input
												class="input"
												type="number"
												min="1"
												max="3600"
												bind:value={r.timeout}
											/></label
										><label class="fieldset"
											>Pulse (ms)<input
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
								disabled={busy || !!offline}
								onclick={async () => {
									if (await perform('knx/config', { ...knx, takeover })) await loadKnx();
								}}>Apply KNX commissioning</button
							>
						</fieldset>
						<button class="btn" onclick={loadKnx}>Reload KNX snapshot</button>
					{/if}
				</div>
			</div>
		{:else}
			<div class="card bg-base-200">
				<div class="card-body">
					<h2 class="card-title">Operations</h2>
					<p>Uptime {status.uptime}s · Configuration revision {status.revision}</p>
					{#if status.capabilities.admin}<label class="fieldset"
							>Type ERASE to factory reset<input class="input" bind:value={resetConfirm} /></label
						><button
							class="btn btn-error"
							disabled={busy || !!offline || resetConfirm !== 'ERASE'}
							onclick={() => command('factory_reset', { confirm: resetConfirm })}
							>Erase configuration and restart</button
						>{/if}
					<h3 class="font-bold">Recent activity</h3>
					<ul class="font-mono text-sm">
						{#each status.audit as line}<li>{line}</li>{/each}
					</ul>
				</div>
			</div>
		{/if}
	{:else}<div class="skeleton h-48 w-full"></div>{/if}
</div>
