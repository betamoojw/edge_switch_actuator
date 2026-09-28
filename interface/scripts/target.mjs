/** Normalize a server-side device origin without placing it in the UI bundle.
 * @param {string | undefined} host
 */
export function deviceTarget(host) {
	if (!host) throw new Error('Set DEVICE_HOST to a device origin, or use npm run dev:sim');
	const url = new URL(host.includes('://') ? host : `http://${host}`);
	if (
		!['http:', 'https:'].includes(url.protocol) ||
		url.username ||
		url.password ||
		url.pathname !== '/' ||
		url.search ||
		url.hash
	)
		throw new Error('DEVICE_HOST must be an HTTP(S) origin without credentials or a path');
	return { http: url.origin, ws: url.origin.replace(/^http/, 'ws') };
}
