<script lang="ts">
	import { languages, locale, t } from '$lib/i18n';
	import { theme, themes } from '$lib/stores/preferences';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Language from '~icons/tabler/language';
	import Palette from '~icons/tabler/palette';
</script>

<section class="mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8">
	<SettingsCard collapsible={false}>
		{#snippet icon()}
			<Palette class="mr-2 h-6 w-6 shrink-0 self-end" aria-hidden="true" />
		{/snippet}
		{#snippet title()}
			<h1 class="text-xl font-medium">{$t('User interface')}</h1>
		{/snippet}
		<p class="mb-2 text-sm opacity-75">
			{$t('Changes apply immediately and are saved in this browser for this device.')}
		</p>
		<div class="rounded-box bg-base-100 flex items-start gap-3 px-4 py-3">
			<div class="mask mask-hexagon bg-primary h-10 w-10 shrink-0">
				<Language class="text-primary-content h-full w-full scale-75" aria-hidden="true" />
			</div>
			<div class="min-w-0 flex-1 space-y-2">
				<label for="ui-language" class="label text-base-content font-bold">{$t('Language')}</label>
				<select id="ui-language" class="select w-full" bind:value={$locale}>
					{#each languages as language}
						<option value={language.value} lang={language.value}>{language.label}</option>
					{/each}
				</select>
			</div>
		</div>
		<div class="rounded-box bg-base-100 flex items-start gap-3 px-4 py-3">
			<div class="mask mask-hexagon bg-primary h-10 w-10 shrink-0">
				<Palette class="text-primary-content h-full w-full scale-75" aria-hidden="true" />
			</div>
			<div class="min-w-0 flex-1 space-y-2">
				<label for="ui-theme" class="label text-base-content font-bold">{$t('Theme')}</label>
				<select id="ui-theme" class="select w-full" bind:value={$theme}>
					{#each themes as option}
						<option value={option.value}>{$t(option.label)}</option>
					{/each}
				</select>
			</div>
		</div>
		<div class="mt-2 space-y-3">
			<p class="text-sm opacity-75">
				{$t('Automatic uses Light by default and Dark when your system prefers dark mode.')}
			</p>
			<div class="flex flex-wrap gap-2" aria-label={$t('Theme preview')}>
				<span class="badge badge-primary">{$t('Active')}</span>
				<span class="badge badge-success">{$t('Connected')}</span>
				<span class="badge badge-warning">{$t('Warning')}</span>
				<span class="badge badge-error">{$t('Fault')}</span>
			</div>
		</div>
	</SettingsCard>
</section>
