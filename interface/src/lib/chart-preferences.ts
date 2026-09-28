import type { Chart } from 'chart.js';
import { get } from 'svelte/store';
import { locale, translate } from '$lib/i18n';
import { theme } from '$lib/stores/preferences';
import { daisyColor } from '$lib/DaisyUiHelper';

/** Keep canvas labels and colors in sync without destroying chart history. */
export function observeChartPreferences(charts: Chart[]) {
	const originals = charts.map((chart) => ({
		labels: chart.data.datasets.map((dataset) => dataset.label ?? ''),
		titles: Object.fromEntries(
			Object.entries(chart.options.scales ?? {}).map(([key, axis]) => [
				key,
				axis && 'title' in axis && typeof axis.title?.text === 'string' ? axis.title.text : ''
			])
		)
	}));
	let frame = 0;
	const refresh = () => {
		cancelAnimationFrame(frame);
		frame = requestAnimationFrame(() => {
			const language = get(locale);
			charts.forEach((chart, i) => {
				chart.options.locale = language;
				chart.options.color = daisyColor('--color-base-content');
				if (chart.options.plugins?.legend?.labels)
					chart.options.plugins.legend.labels.color = daisyColor('--color-base-content');
				chart.data.datasets.forEach((dataset, j) => {
					dataset.label = translate(language, originals[i].labels[j]);
					const color = j ? '--color-secondary' : '--color-primary';
					dataset.borderColor = daisyColor(color);
					dataset.backgroundColor = daisyColor(color, 'fill' in dataset && dataset.fill ? 25 : 50);
				});
				for (const [key, axis] of Object.entries(chart.options.scales ?? {})) {
					if (!axis) continue;
					if ('title' in axis && axis.title) {
						axis.title.text = translate(language, originals[i].titles[key]);
						axis.title.color = daisyColor('--color-base-content');
					}
					if (axis.ticks) axis.ticks.color = daisyColor('--color-base-content');
					if (axis.grid) axis.grid.color = daisyColor('--color-base-content', 10);
					if ('border' in axis && axis.border)
						axis.border.color = daisyColor('--color-base-content', 10);
					if (axis.type === 'time') axis.adapters = { date: { locale: language } };
				}
				chart.update('none');
			});
		});
	};
	const stopLocale = locale.subscribe(refresh);
	const stopTheme = theme.subscribe(refresh);
	const media = matchMedia('(prefers-color-scheme: dark)');
	media.addEventListener('change', refresh);
	return () => {
		stopLocale();
		stopTheme();
		media.removeEventListener('change', refresh);
		cancelAnimationFrame(frame);
	};
}
