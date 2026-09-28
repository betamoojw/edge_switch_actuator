<script lang="ts">
	import { t } from '$lib/i18n';
	import { onMount } from 'svelte';
	import { goto } from '$app/navigation';

	onMount(() => {
		void goto('/device', { replaceState: true });
	});
</script>

<p class="p-4">{$t('Opening')} <a class="link" href="/device">{$t('Switching Actuator')}</a>…</p>
