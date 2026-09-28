import { derived, writable } from 'svelte/store';
import messages from './messages.json';

export const languages = [
	{ value: 'en', label: 'English' },
	{ value: 'de', label: 'Deutsch' },
	{ value: 'es', label: 'Español' },
	{ value: 'fr', label: 'Français' },
	{ value: 'pl', label: 'Polski' },
	{ value: 'zh-CN', label: '简体中文' },
	{ value: 'zh-TW', label: '繁體中文' }
] as const;
export type Locale = (typeof languages)[number]['value'];
export const locale = writable<Locale>('en');
const catalog: Record<string, string[]> = messages;

// English source messages are stable keys. Unknown device diagnostics stay intact.
export function translate(
	language: Locale,
	message: string,
	params: Record<string, string | number> = {}
) {
	const index = languages.findIndex((entry) => entry.value === language) - 1;
	const translated = index < 0 ? message : (catalog[message]?.[index] ?? message);
	return translated.replace(/\{(\w+)\}/g, (match, key) => String(params[key] ?? match));
}
export const t = derived(
	locale,
	($locale) => (message: string, params?: Record<string, string | number>) =>
		translate($locale, message, params)
);

export function duration(language: Locale, seconds: number) {
	return [
		[86400, 'day'],
		[3600, 'hour'],
		[60, 'minute'],
		[1, 'second']
	]
		.flatMap(([size, unit]) => {
			const count = Math.floor(seconds / Number(size));
			seconds %= Number(size);
			return count || size === 1
				? [
						new Intl.NumberFormat(language, {
							style: 'unit',
							unit: String(unit),
							unitDisplay: 'long'
						}).format(count)
					]
				: [];
		})
		.join(' ');
}
