/** Address notation supported by the actuator firmware. */
export function individualAddressError(value: string): string {
	if (!/^\d{1,2}\.\d{1,2}\.\d{1,3}$/.test(value))
		return 'Use area.line.device, for example 1.1.10.';
	const [area, line, device] = value.split('.').map(Number);
	return area > 15 || line > 15 || device < 1 || device > 255
		? 'Area and line must be 0–15; device must be 1–255 (0 is reserved for couplers).'
		: '';
}

export function groupAddressError(value: string): string {
	if (!value.trim()) return '';
	const groups = value.split(',').map((group) => group.trim());
	if (groups.length > 8) return 'Use at most 8 group addresses per object.';
	const seen = new Set<string>();
	for (const group of groups) {
		if (!/^\d{1,2}\/\d\/\d{1,3}$/.test(group))
			return 'Use main/middle/sub, for example 1/0/1; separate addresses with commas.';
		const [main, middle, sub] = group.split('/').map(Number);
		if (main > 31 || middle > 7 || sub > 255)
			return 'Main must be 0–31, middle 0–7, and sub 0–255.';
		if (main === 0 && middle === 0 && sub === 0)
			return '0/0/0 is reserved for broadcast and cannot be assigned.';
		const key = `${main}/${middle}/${sub}`;
		if (seen.has(key)) return 'Each group address must be unique within this object.';
		seen.add(key);
	}
	return '';
}
