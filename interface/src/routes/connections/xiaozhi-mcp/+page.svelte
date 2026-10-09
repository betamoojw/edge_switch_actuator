<script lang="ts">
	import { page } from '$app/state';
	import { t } from '$lib/i18n';
	import XiaozhiMCP from './XiaozhiMCP.svelte';
</script>

{#if page.data.features.xiaozhi_mcp}
	<XiaozhiMCP />
{:else}
	<p class="alert m-4">{$t('Xiaozhi MCP is unavailable in this firmware.')}</p>
{/if}
