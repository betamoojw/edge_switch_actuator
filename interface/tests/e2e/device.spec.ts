import { test, expect } from '@playwright/test';
// Deliberately read-only. No simulator control, relay writes, network changes,
// reset or OTA operations may be added to this default hardware suite.
test('device-safe features and authenticated actuator snapshot', async ({ page, request }) => {
	const features = await request.get('/rest/features');
	expect(features.ok()).toBeTruthy();
	const info = await features.json();
	expect(typeof info.event_use_json).toBe('boolean');
	await page.goto('/device');
	if (info.security) {
		const username = process.env.DEVICE_USERNAME,
			password = process.env.DEVICE_PASSWORD;
		if (!username || !password)
			throw new Error(
				'DEVICE_USERNAME and DEVICE_PASSWORD are required for authenticated device smoke tests'
			);
		await page.getByLabel('Username', { exact: true }).fill(username);
		await page.getByLabel('Password', { exact: true }).fill(password);
		await page.getByRole('button', { name: 'Login', exact: true }).click();
	}
	await expect(page.getByRole('heading', { name: 'Switching Actuator' })).toBeVisible();
	await expect(page.getByRole('button', { name: /Turn ON|Turn OFF/ })).toHaveCount(6);
});
