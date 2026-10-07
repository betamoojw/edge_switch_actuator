// Patch supplied fields without replacing unchanged arrays or nested reactive objects.
// Omitted fields (notably authenticated capabilities) remain authoritative locally.
export function reconcile(target: any, next: any): void {
	for (const key of Object.keys(next)) {
		const old = target[key];
		const value = next[key];
		if (
			old &&
			value &&
			typeof old === 'object' &&
			typeof value === 'object' &&
			Array.isArray(old) === Array.isArray(value)
		) {
			reconcile(old, value);
			if (Array.isArray(old)) old.length = value.length;
		} else if (old !== value) target[key] = value;
	}
}
