<script lang="ts">
	import { onMount } from 'svelte';
	import { goto } from '$app/navigation';

	onMount(() => {
		void goto('/device', { replaceState: true });
	});
</script>

<p class="p-4">Opening <a class="link" href="/device">Switching Actuator</a>…</p>
