<script lang="ts">
	import { onMount } from 'svelte';
	import { t } from '$lib/i18n';
	import { user } from '$lib/stores/user';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import type { XiaozhiMcpSettings, XiaozhiMcpStatus, XiaozhiMcpTool } from '$lib/types/models';

	let saved = $state<XiaozhiMcpSettings>();
	let status = $state<XiaozhiMcpStatus>();
	let tools = $state<XiaozhiMcpTool[]>([]);
	let names = $state<string[]>([]);
	let enabled = $state(false),
		alias = $state(''),
		mask = $state(0),
		endpoint = $state('');
	let clearEndpoint = $state(false),
		reveal = $state(false),
		busy = $state(false),
		loaded = $state(false);
	let error = $state(''),
		message = $state(''),
		statusError = $state('');
	let controller = new AbortController();
	const dirty = $derived(
		!!saved &&
			(enabled !== saved.enabled ||
				alias !== saved.alias ||
				mask !== saved.channel_mask ||
				endpoint !== '' ||
				clearEndpoint)
	);
	const labels: Record<string, string> = {
		disabled: 'Disabled',
		unconfigured: 'Not configured',
		waiting_network: 'Waiting for network',
		waiting_time: 'Waiting for time synchronization',
		connecting: 'Connecting',
		initializing: 'Initializing',
		ready: 'Ready',
		backoff: 'Retrying',
		error: 'Connection error',
		paused: 'Paused'
	};
	async function api<T>(path: string, body?: unknown): Promise<T> {
		const response = await fetch(`/rest/${path}`, {
			method: body === undefined ? 'GET' : 'POST',
			signal: controller.signal,
			headers: {
				Authorization: `Bearer ${$user.bearer_token}`,
				'Content-Type': 'application/json'
			},
			body: body === undefined ? undefined : JSON.stringify(body)
		});
		if (!response.ok) {
			if (response.status === 401) {
				user.invalidate();
				throw new Error('Session expired. Please sign in again.');
			}
			if (response.status === 409)
				throw new Error('Settings changed. Reload saved settings before trying again.');
			if (response.status === 403) throw new Error('Administrator access required.');
			if (response.status === 422)
				throw new Error('Check the alias, endpoint and allowed channels.');
			throw new Error('Request failed. Please try again.');
		}
		return response.json();
	}
	function accept(value: XiaozhiMcpSettings) {
		saved = value;
		enabled = value.enabled;
		alias = value.alias;
		mask = value.channel_mask;
		endpoint = '';
		clearEndpoint = false;
		reveal = false;
	}
	async function refresh() {
		if (controller.signal.aborted || document.hidden || !$user.bearer_token) return;
		try {
			status = await api<XiaozhiMcpStatus>('xiaozhiMcpStatus');
			statusError = '';
		} catch {
			if (!controller.signal.aborted) statusError = 'Connection status could not be refreshed.';
		}
	}
	async function loadSettings() {
		busy = true;
		error = '';
		try {
			const [settings, listing, device] = await Promise.all([
				api<XiaozhiMcpSettings>('xiaozhiMcpSettings'),
				api<{ tools: XiaozhiMcpTool[] }>('xiaozhiMcpTools'),
				api<{ relays: { name: string }[] }>('device/config')
			]);
			accept(settings);
			tools = listing.tools;
			names = device.relays.map((relay) => relay.name);
		} catch (e) {
			if (!controller.signal.aborted) error = e instanceof Error ? e.message : 'Request failed.';
		} finally {
			busy = false;
			loaded = true;
		}
	}
	onMount(() => {
		void refresh();
		if ($user.admin) void loadSettings();
		else loaded = true;
		let polling = false;
		const tick = async () => {
			if (polling) return;
			polling = true;
			try {
				await refresh();
			} finally {
				polling = false;
			}
		};
		const timer = setInterval(tick, 3000);
		document.addEventListener('visibilitychange', tick);
		return () => {
			controller.abort();
			endpoint = '';
			clearInterval(timer);
			document.removeEventListener('visibilitychange', tick);
		};
	});
	async function save(event: SubmitEvent) {
		event.preventDefault();
		if (!saved || busy) return;
		error = '';
		message = '';
		const bytes = new TextEncoder().encode(alias.trim()).length;
		if (!bytes || bytes > 64 || /[\x00-\x1f\x7f]/.test(alias)) {
			error = 'Alias must contain 1–64 UTF-8 bytes without control characters.';
			return;
		}
		if (enabled && ((!saved.endpoint_configured && !endpoint) || clearEndpoint)) {
			error = 'An MCP endpoint is required before enabling.';
			return;
		}
		if (
			endpoint &&
			(!endpoint.startsWith('wss://') ||
				new TextEncoder().encode(endpoint).length > 2048 ||
				/[\s#\\]/.test(endpoint))
		) {
			error = 'Enter a secure wss:// endpoint, up to 2048 bytes.';
			return;
		}
		busy = true;
		try {
			accept(
				await api<XiaozhiMcpSettings>('xiaozhiMcpSettings', {
					revision: saved.revision,
					enabled,
					alias: alias.trim(),
					channel_mask: mask,
					endpoint,
					clear_endpoint: clearEndpoint
				})
			);
			message = 'Settings saved. Connection status updates separately.';
			tools = (await api<{ tools: XiaozhiMcpTool[] }>('xiaozhiMcpTools')).tools;
			await refresh();
		} catch (e) {
			if (!controller.signal.aborted) error = e instanceof Error ? e.message : 'Request failed.';
		} finally {
			busy = false;
		}
	}
	async function reconnect() {
		busy = true;
		error = '';
		message = '';
		try {
			await api('xiaozhiMcpReconnect', {});
			message = 'Reconnection requested.';
			await refresh();
		} catch (e) {
			if (!controller.signal.aborted) error = e instanceof Error ? e.message : 'Request failed.';
		} finally {
			busy = false;
		}
	}
</script>

<div class="mx-2 my-4 flex flex-col gap-4 sm:mx-8">
	<SettingsCard collapsible={false}>
		{#snippet title()}<h2>{$t('Xiaozhi MCP status')}</h2>{/snippet}
		<div class="space-y-3 px-4 pb-4">
			<p role="status">
				<span class:badge-success={status?.ready} class="badge"
					>{$t(status ? labels[status.state] || 'Connection error' : 'Loading...')}</span
				>
			</p>
			{#if status}<p class="break-words">{status.alias} · {status.device_id}</p>
				<p>{$t('Available tools')}: {status.tool_count}</p>
				{#if status.retry_in_ms > 0}<p>
						{$t('Retry delay')}: {Math.ceil(status.retry_in_ms / 1000)}
						{$t('Seconds')}
					</p>{/if}
				{#if status.state === 'waiting_time'}<a class="link" href="/connections/ntp"
						>{$t('Configure NTP')}</a
					>{/if}
				{#if status.error_code}<p class="text-warning">
						{$t('Check the endpoint, network and certificate trust.')}
						<code>{status.error_code}</code>
					</p>{/if}
			{/if}
			{#if statusError}<p class="text-warning">{$t(statusError)}</p>{/if}
			{#if !$user.admin}<p>{$t('Connection settings are managed by an administrator.')}</p>{/if}
		</div>
	</SettingsCard>
	{#if error}<p class="alert alert-error break-words" role="alert">{$t(error)}</p>{/if}
	{#if message}<p class="alert alert-success" role="status">{$t(message)}</p>{/if}
	{#if $user.admin}
		<SettingsCard collapsible={false} isDirty={dirty}>
			{#snippet title()}<h2>{$t('Xiaozhi MCP settings')}</h2>{/snippet}
			{#if saved}
				<form class="space-y-4 px-4 pb-4" onsubmit={save}>
					<fieldset disabled={busy} class="space-y-4">
						<label class="flex items-center gap-3"
							><input class="checkbox" type="checkbox" bind:checked={enabled} />{$t(
								'Enable Xiaozhi MCP'
							)}</label
						>
						<div>
							<label class="label" for="mcp-alias">{$t('Device alias')}</label><input
								class="input w-full"
								id="mcp-alias"
								bind:value={alias}
								required
							/>
						</div>
						<div>
							<label class="label" for="mcp-endpoint">{$t('MCP endpoint')}</label>
							<input
								class="input w-full"
								id="mcp-endpoint"
								type={reveal ? 'text' : 'password'}
								bind:value={endpoint}
								autocomplete="off"
								spellcheck={false}
								disabled={clearEndpoint}
								placeholder="wss://…"
							/>
							<label class="mt-2 flex items-center gap-2"
								><input class="checkbox checkbox-sm" type="checkbox" bind:checked={reveal} />{$t(
									'Show endpoint'
								)}</label
							>
							<p class="mt-2 text-sm">
								{$t(
									saved.endpoint_configured
										? 'Endpoint saved. Leave blank to keep it.'
										: 'No endpoint configured.'
								)}
							</p>
							{#if saved.endpoint_configured}<p class="break-all text-sm">{saved.endpoint_host}</p>
								<label class="mt-2 flex items-center gap-2"
									><input
										class="checkbox checkbox-sm"
										type="checkbox"
										bind:checked={clearEndpoint}
										onchange={() => {
											if (clearEndpoint) endpoint = '';
										}}
									/>{$t('Remove saved endpoint')}</label
								>{/if}
						</div>
						<fieldset>
							<legend class="font-semibold">{$t('Allowed relay channels')}</legend>
							<p class="my-2 text-sm">
								{$t(
									'Selected channels can be controlled by Xiaozhi. Unselected channels remain private.'
								)}
							</p>
							<div class="grid gap-3 sm:grid-cols-2">
								{#each Array(6) as _, index}<label class="flex items-center gap-2"
										><input
											class="checkbox"
											type="checkbox"
											checked={!!(mask & (1 << index))}
											onchange={(e) => {
												mask = e.currentTarget.checked ? mask | (1 << index) : mask & ~(1 << index);
											}}
										/>{index + 1}. {names[index] || $t('Channel')}</label
									>{/each}
							</div>
						</fieldset>
					</fieldset>
					<div class="flex flex-wrap justify-end gap-2">
						<button class="btn" type="button" disabled={busy} onclick={loadSettings}
							>{$t('Reload saved settings')}</button
						>
						<button
							class="btn"
							type="button"
							disabled={busy || dirty || !saved.enabled || !saved.endpoint_configured}
							onclick={reconnect}>{$t('Reconnect')}</button
						>
						<button class="btn btn-primary" type="submit" disabled={busy || !dirty}
							>{$t('Apply Settings')}</button
						>
					</div>
				</form>
			{:else}<div class="px-4 pb-4">
					<p>{$t(loaded ? 'Settings could not be loaded.' : 'Loading...')}</p>
					<button class="btn mt-2" disabled={busy} onclick={loadSettings}
						>{$t('Reload saved settings')}</button
					>
				</div>{/if}
		</SettingsCard>
		<SettingsCard collapsible={false}>
			{#snippet title()}<h2>{$t('Available tools')}</h2>{/snippet}
			<div class="space-y-3 px-4 pb-4">
				{#each tools as tool}<details class="rounded-box border border-base-300 p-3">
						<summary class="cursor-pointer break-all font-mono text-sm">{tool.name}</summary>
						<p class="my-2 break-words">{tool.description}</p>
						<pre class="overflow-x-auto text-xs">{JSON.stringify(tool.inputSchema, null, 2)}</pre>
					</details>{/each}
			</div>
		</SettingsCard>
	{/if}
</div>
