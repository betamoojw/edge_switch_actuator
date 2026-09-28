import { get, writable } from 'svelte/store';
import { languages, locale, type Locale } from '$lib/i18n';

export const themes = [
	{ value: 'system', label: 'Automatic (system)' },
	{ value: 'light', label: 'Light' },
	{ value: 'dark', label: 'Dark' },
	{ value: 'nord', label: 'Nord' },
	{ value: 'dim', label: 'Dim' },
	{ value: 'retro', label: 'Sepia' }
] as const;
export type Theme = (typeof themes)[number]['value'];
export const theme = writable<Theme>('system');
export const preferenceKey = 'actuator.ui';
function decode(raw: string | null): { language: Locale; theme: Theme } {
	let value;
	try {
		value = JSON.parse(raw ?? '{}');
	} catch {
		value = {};
	}
	return {
		language: languages.some((entry) => entry.value === value?.language) ? value.language : 'en',
		theme: themes.some((entry) => entry.value === value?.theme) ? value.theme : 'system'
	};
}

/** Called once by the root layout; returns all subscription cleanup. */
export function initializePreferences() {
	let raw = null;
	try {
		raw = localStorage.getItem(preferenceKey);
	} catch {
		/* Private/blocked storage. */
	}
	const initial = decode(raw);
	locale.set(initial.language);
	theme.set(initial.theme);
	let receivingStorage = false;
	const save = () => {
		if (receivingStorage) return;
		try {
			localStorage.setItem(
				preferenceKey,
				JSON.stringify({ language: get(locale), theme: get(theme) })
			);
		} catch {
			/* Preferences still work for this session. */
		}
	};
	const stopLocale = locale.subscribe((language) => {
		document.documentElement.lang = language;
		save();
	});
	const stopTheme = theme.subscribe((value) => {
		if (value === 'system') delete document.documentElement.dataset.theme;
		else document.documentElement.dataset.theme = value;
		save();
	});
	const onStorage = (event: StorageEvent) => {
		if (event.key !== preferenceKey && event.key !== null) return;
		const next = decode(event.newValue);
		receivingStorage = true;
		locale.set(next.language);
		theme.set(next.theme);
		receivingStorage = false;
	};
	window.addEventListener('storage', onStorage);
	return () => {
		stopLocale();
		stopTheme();
		window.removeEventListener('storage', onStorage);
	};
}
