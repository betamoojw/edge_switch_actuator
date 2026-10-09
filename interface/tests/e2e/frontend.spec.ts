import { test, expect, type Page, type APIRequestContext } from '@playwright/test';

test('UI-01 all languages apply across navigation, reload and login', async ({ page }) => {
	await login(page, 'admin', '/system/ui');
	const languages = [
		['de', 'Benutzeroberfläche', 'Schaltaktor', 'Anmelden'],
		['es', 'Interfaz de usuario', 'Actuador de conmutación', 'Iniciar sesión'],
		['fr', 'Interface utilisateur', 'Actionneur de commutation', 'Connexion'],
		['pl', 'Interfejs użytkownika', 'Aktor przełączający', 'Zaloguj się'],
		['zh-CN', '用户界面', '开关执行器', '登录'],
		['zh-TW', '使用者介面', '開關致動器', '登入'],
		['en', 'User interface', 'Switching Actuator', 'Login']
	];
	for (const [language, heading, actuator] of languages) {
		await page.locator('#ui-language').selectOption(language);
		await expect(page.locator('html')).toHaveAttribute('lang', language);
		await expect(page.getByRole('heading', { name: heading, exact: true })).toBeVisible();
		expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(
			true
		);
		await page.goto('/device');
		await expect(page.getByRole('heading', { name: actuator, exact: true })).toBeVisible();
		await expect(page).toHaveTitle(actuator);
		await page.goto('/system/ui');
		await expect(page.locator('#ui-language')).toHaveValue(language);
	}
	await page.locator('#ui-language').selectOption('de');
	await page.evaluate(() => {
		for (const key of Object.keys(localStorage))
			if (key !== 'actuator.ui') localStorage.removeItem(key);
	});
	await page.reload();
	await expect(page.getByRole('heading', { name: 'Anmelden', exact: true })).toBeVisible();
});

test('UI-02 all themes persist and automatic follows system preference', async ({ page }) => {
	await page.emulateMedia({ colorScheme: 'light' });
	await login(page, 'admin', '/system/ui');
	const background = () =>
		page
			.locator('html')
			.evaluate((el) => getComputedStyle(el).getPropertyValue('--color-base-100'));
	const light = await background();
	await page.emulateMedia({ colorScheme: 'dark' });
	await expect.poll(background).not.toBe(light);
	const dark = await background();
	for (const selected of ['light', 'dark', 'nord', 'dim', 'retro']) {
		await page.locator('#ui-theme').selectOption(selected);
		await expect(page.locator('html')).toHaveAttribute('data-theme', selected);
		await page.reload();
		await expect(page.locator('#ui-theme')).toHaveValue(selected);
		await expect(page.locator('html')).toHaveAttribute('data-theme', selected);
	}
	await page.locator('#ui-theme').selectOption('light');
	await expect.poll(background).toBe(light);
	await page.locator('#ui-theme').selectOption('system');
	await expect(page.locator('html')).not.toHaveAttribute('data-theme');
	await expect.poll(background).toBe(dark);
});

test('UI-03 invalid and blocked storage do not prevent startup', async ({ page }) => {
	await page.addInitScript(() => localStorage.setItem('actuator.ui', '{broken'));
	await login(page, 'admin', '/system/ui');
	await expect(page.locator('#ui-language')).toHaveValue('en');
	await expect(page.locator('#ui-theme')).toHaveValue('system');
	await page.addInitScript(() => {
		const originalGet = Storage.prototype.getItem;
		const originalSet = Storage.prototype.setItem;
		Storage.prototype.getItem = function (key) {
			if (key === 'actuator.ui') throw new Error('blocked');
			return originalGet.call(this, key);
		};
		Storage.prototype.setItem = function (key, value) {
			if (key === 'actuator.ui') throw new Error('blocked');
			return originalSet.call(this, key, value);
		};
	});
	await page.reload();
	await page.locator('#ui-language').selectOption('pl');
	await expect(page.getByRole('heading', { name: 'Interfejs użytkownika' })).toBeVisible();
	await page.locator('#ui-theme').selectOption('dim');
	await expect(page.locator('html')).toHaveAttribute('data-theme', 'dim');
});

test('UI-04 translated KNX controls retain protocol values and validate addresses', async ({
	page,
	request
}) => {
	await login(page, 'admin', '/system/ui');
	await page.locator('#ui-language').selectOption('de');
	await page.goto('/device');
	await page.getByRole('tab', { name: 'Protokoll', exact: true }).click();
	await page.getByRole('combobox', { name: 'Protokoll', exact: true }).selectOption('knx_ip');
	await page.getByRole('button', { name: 'Einstellungen übernehmen', exact: true }).click();
	await page.getByRole('tab', { name: 'KNX', exact: true }).click();
	await page.getByLabel('Physikalische Adresse', { exact: true }).fill('1.1.0');
	await expect(
		page.getByText('Bereich und Linie: 0–15; Teilnehmer: 1–255 (0 für Koppler reserviert).')
	).toBeVisible();
	await expect(page.getByRole('button', { name: 'KNX-Inbetriebnahme übernehmen' })).toBeDisabled();
	expect((await state(request)).config.mode).toBe('knx_ip');
});

test('UI-05 System submenu and cross-tab preferences', async ({ page, context }, testInfo) => {
	await login(page);
	const menuToggle = page.locator('label[for="main-menu"]').first();
	if (await menuToggle.isVisible()) await menuToggle.click();
	await page.locator('summary').filter({ hasText: 'System' }).click();
	await page.getByRole('link', { name: 'UI', exact: true }).click();
	await expect(page).toHaveURL(/\/system\/ui$/);
	const second = await context.newPage();
	await second.goto('/system/ui');
	await page.locator('#ui-language').selectOption('fr');
	await page.locator('#ui-theme').selectOption('nord');
	await expect(second.locator('#ui-language')).toHaveValue('fr');
	await expect(second.locator('html')).toHaveAttribute('data-theme', 'nord');
	await page.screenshot({ path: testInfo.outputPath('ui-french-nord.png'), fullPage: true });
	await second.close();
});

test('HOME-01 root opens Switching Actuator', async ({ page }) => {
	await login(page, 'admin', '/');
	await expect(page).toHaveURL(/\/device$/);
	await expect(
		page.getByRole('heading', { name: 'Switching Actuator', exact: true })
	).toBeVisible();
	await expect(page.getByText(/Welcome to Switching Actuator/)).toBeVisible();
	await page.reload();
	await expect(page).toHaveURL(/\/device$/);
	await expect(page).toHaveTitle(/Switching Actuator/);
});

test('KNX-04 validates addresses before saving', async ({ page, request }) => {
	await login(page);
	await page.getByRole('tab', { name: 'Protocol', exact: true }).click();
	await expect(page.getByRole('heading', { name: 'Protocol Interface selection' })).toBeVisible();
	await page.getByRole('combobox', { name: 'Protocol', exact: true }).selectOption('knx_ip');
	await apply(page);
	await page.getByRole('tab', { name: 'KNX', exact: true }).click();
	const individual = page.getByLabel('Individual address', { exact: true });
	const group = page.getByRole('textbox', { name: 'Group addresses for object 1', exact: true });
	const saveKnx = page.getByRole('button', { name: 'Apply KNX commissioning' });
	await expect(individual).toHaveAttribute('placeholder', '15.15.255 (Factory Default)');
	await expect(group).toHaveAttribute('placeholder', '1/0/1, 1/0/2');
	for (const value of ['', '16.1.1', '1.16.1', '1.1.0', '1.1.256', '1.1', 'a.b.c', '1.1.1x']) {
		await individual.fill(value);
		await expect(individual).toHaveAttribute('aria-invalid', 'true');
		await expect(saveKnx).toBeDisabled();
	}
	for (const value of ['0.0.1', '15.15.255']) {
		await individual.fill(value);
		await expect(individual).toHaveAttribute('aria-invalid', 'false');
	}
	for (const value of [
		'0/0/0',
		'32/0/1',
		'1/8/1',
		'1/0/256',
		'1/1',
		'x/0/1',
		'1/0/1,',
		'1/0/1, 01/0/001',
		'1/0/1,1/0/2,1/0/3,1/0/4,1/0/5,1/0/6,1/0/7,1/0/8,1/0/9'
	]) {
		await group.fill(value);
		await expect(group).toHaveAttribute('aria-invalid', 'true');
		await expect(saveKnx).toBeDisabled();
	}
	await group.fill('');
	await expect(group).toHaveAttribute('aria-invalid', 'false');
	await group.fill('0/0/1, 31/7/255');
	await individual.fill('1.1.10');
	await page.getByRole('spinbutton', { name: 'Pulse (ms)', exact: true }).first().fill('1500');
	await page.getByRole('checkbox', { name: /Take over for web editing/ }).check();
	await expect(saveKnx).toBeEnabled();
	await saveKnx.click();
	await expect(page.getByText(/Owner: web · Revision 2/)).toBeVisible();
	const snapshot = await state(request);
	expect(snapshot.knx.address).toBe('1.1.10');
	expect(snapshot.knx.objects[0].groups).toEqual(['0/0/1', '31/7/255']);
	expect(snapshot.knx.parameters[0].pulseMs).toBe(1500);
	await expect(individual).toHaveValue('1.1.10');
	// An address-only edit must retain group mappings and application parameters.
	await individual.fill('1.1.11');
	await saveKnx.click();
	await expect(page.getByText(/Owner: web · Revision 3/)).toBeVisible();
	await page.getByRole('button', { name: 'Reload KNX snapshot' }).click();
	await expect(individual).toHaveValue('1.1.11');
	const changed = await state(request);
	expect(changed.knx.objects).toEqual(snapshot.knx.objects);
	expect(changed.knx.parameters).toEqual(snapshot.knx.parameters);
});

const controlURL = `http://127.0.0.1:${process.env.SIM_CONTROL_PORT || 3081}`;
async function control(request: APIRequestContext, path: string, data: unknown = {}) {
	const response = await request.post(`${controlURL}/__sim/${path}`, {
		headers: { Authorization: 'Bearer playwright-local-control' },
		data
	});
	expect(response.ok(), await response.text()).toBeTruthy();
	return response;
}
async function state(request: APIRequestContext) {
	const r = await request.get(`${controlURL}/__sim/state`, {
		headers: { Authorization: 'Bearer playwright-local-control' }
	});
	expect(r.ok()).toBeTruthy();
	return r.json();
}
async function login(page: Page, username = 'admin', path = '/device') {
	await page.goto(path);
	await page.getByLabel('Username', { exact: true }).fill(username);
	await page.getByLabel('Password', { exact: true }).fill(`sim-${username}`);
	await page.getByRole('button', { name: 'Login', exact: true }).click();
	await expect(page.getByRole('heading', { name: 'Login', exact: true })).toBeHidden();
}
const card = (page: Page, n = 1) =>
	page
		.locator('.card')
		.filter({ has: page.getByRole('heading', { name: new RegExp(`^${n}\\. `) }) });
async function apply(page: Page) {
	const response = page.waitForResponse(
		(r) => r.url().endsWith('/rest/device/config') && r.request().method() === 'POST'
	);
	await page.getByRole('button', { name: 'Apply settings', exact: true }).click();
	expect((await response).status()).toBe(200);
	await expect(page.getByRole('button', { name: 'Discard', exact: true })).toBeHidden();
}
test.beforeEach(async ({ page, request }) => {
	await control(request, 'reset', { profile: 'actuator' });
	await page.route('https://api.github.com/**', (route) =>
		route.fulfill({
			json: route.request().url().endsWith('/latest')
				? {
						tag_name: '0.2.0',
						assets: [],
						html_url: 'https://example.invalid/release',
						prerelease: false
					}
				: []
		})
	);
});

test('AUTH-01 login error, authenticated deep link and refresh', async ({ page }) => {
	await page.goto('/device');
	await page.getByLabel('Username', { exact: true }).fill('admin');
	await page.getByLabel('Password', { exact: true }).fill('wrong');
	await page.getByRole('button', { name: 'Login', exact: true }).click();
	await expect(page.getByText('Wrong Username or Password!')).toBeVisible();
	await page.getByLabel('Username', { exact: true }).fill('admin');
	await page.getByLabel('Password', { exact: true }).fill('sim-admin');
	await page.getByRole('button', { name: 'Login', exact: true }).click();
	await expect(page.getByRole('heading', { name: 'Switching Actuator' })).toBeVisible();
	await page.reload();
	await expect(card(page).getByRole('button', { name: 'Turn ON', exact: true })).toBeEnabled();
});

test('OUT-01/03 relay, pulse, all-off and device-originated changes', async ({ page, request }) => {
	await login(page);
	await card(page).getByRole('button', { name: 'Turn ON', exact: true }).click();
	await expect(card(page).getByRole('button', { name: 'Turn OFF', exact: true })).toBeVisible();
	expect((await state(request)).status.relays[0].source).toBe('web');
	await page.getByRole('button', { name: 'All enabled channels OFF' }).click();
	await expect(card(page).getByRole('button', { name: 'Turn ON', exact: true })).toBeVisible();
	await card(page).getByRole('button', { name: 'Pulse', exact: true }).click();
	await expect.poll(async () => (await state(request)).status.relays[0].on).toBeTruthy();
	await control(request, 'clock', { advanceMs: 1100 });
	await expect(card(page).getByRole('button', { name: 'Turn ON', exact: true })).toBeVisible();
	await control(request, 'actions', { type: 'relay', channel: 1, value: true, source: 'external' });
	await expect(card(page, 2).getByRole('button', { name: 'Turn OFF', exact: true })).toBeVisible();
	await expect(card(page, 2).getByText('Enabled · external')).toBeVisible();
});

for (const role of ['viewer', 'operator', 'installer'])
	test(`AUTH-02 ${role} UI capabilities`, async ({ page }) => {
		await login(page, role);
		const first = card(page).getByRole('button', { name: 'Turn ON', exact: true });
		if (role === 'viewer') await expect(first).toBeDisabled();
		else await expect(first).toBeEnabled();
		if (role !== 'installer') {
			await expect(
				card(page, 2).getByRole('button', { name: 'Turn ON', exact: true })
			).toBeDisabled();
			await expect(page.getByText('Channel settings', { exact: true })).toHaveCount(0);
		} else await expect(page.getByText('Channel settings', { exact: true })).toHaveCount(6);
	});

test('CFG-01 dirty discard, save, refresh and reboot persistence', async ({ page, request }) => {
	await login(page);
	await card(page).getByText('Channel settings', { exact: true }).click();
	await card(page).getByLabel('Name', { exact: true }).fill('Discarded');
	await page.getByRole('button', { name: 'Discard', exact: true }).click();
	await expect(card(page).getByLabel('Name', { exact: true })).toHaveValue('Channel 1');
	await card(page).getByLabel('Name', { exact: true }).fill('Kitchen');
	await apply(page);
	await page.reload();
	await expect(page.getByRole('heading', { name: '1. Kitchen', exact: true })).toBeVisible();
	await control(request, 'actions', { type: 'reboot', durationMs: 1 });
	await login(page);
	await page.goto('/device');
	await expect(page.getByRole('heading', { name: '1. Kitchen', exact: true })).toBeVisible();
});

test('CFG-02 conflict and failure preserve unsaved profile', async ({ page, request }) => {
	await login(page);
	await card(page).getByText('Channel settings', { exact: true }).click();
	await card(page).getByLabel('Name', { exact: true }).fill('Unsaved');
	await control(request, 'actions', { type: 'failure', target: 'persistence' });
	await page.getByRole('button', { name: 'Apply settings', exact: true }).click();
	await expect(
		page.getByText('Error: Configuration persistence failed', { exact: true })
	).toBeVisible();
	expect((await state(request)).config.relays[0].name).toBe('Channel 1');
	await expect(page.getByRole('button', { name: 'Discard', exact: true })).toBeVisible();
});

test('RGB-01/TONE-01 indicators and bounded commands', async ({ page, request }) => {
	await login(page);
	await page.getByRole('tab', { name: 'Indicators', exact: true }).click();
	await page.getByLabel('Test color', { exact: true }).fill('#123456');
	await page.getByRole('button', { name: 'Test color', exact: true }).click();
	await expect.poll(async () => (await state(request)).status.color).toEqual([1, 5, 8]);
	await page.getByLabel('Duration (ms)', { exact: true }).fill('2000');
	await page.getByRole('button', { name: 'Test tone', exact: true }).click();
	await expect.poll(async () => (await state(request)).status.toneHz).toBe(2000);
	await page.getByRole('button', { name: 'Silence current tone' }).click();
	await expect.poll(async () => (await state(request)).status.toneHz).toBe(0);
});

test('BTN-01 button bindings and external gesture', async ({ page, request }) => {
	await login(page);
	await page.getByRole('tab', { name: 'Button', exact: true }).click();
	await page.getByRole('combobox', { name: 'Single click', exact: true }).selectOption('3');
	await page.getByRole('combobox', { name: 'Target', exact: true }).first().selectOption('1');
	await apply(page);
	await control(request, 'actions', { type: 'gesture', clicks: 1 });
	await expect(page.getByText(/Last gesture 1 · Events 1/)).toBeVisible();
	await page.getByRole('tab', { name: 'Outputs', exact: true }).click();
	await expect(card(page).getByRole('button', { name: 'Turn OFF', exact: true })).toBeVisible();
});

test('BUS-01/MB-01/02 protocol selection, RTU fields, window and network wait', async ({
	page,
	request
}) => {
	await login(page);
	await page.getByRole('tab', { name: 'Protocol', exact: true }).click();
	await page.getByRole('combobox', { name: 'Protocol', exact: true }).selectOption('modbus_rtu');
	await page.getByLabel('Unit address', { exact: true }).fill('12');
	await page.getByRole('combobox', { name: 'Baud', exact: true }).selectOption('3');
	await apply(page);
	expect((await state(request)).config.baud).toBe(3);
	await page.getByRole('button', { name: 'Open 60-second configuration window' }).click();
	await expect(page.getByText('Window open', { exact: true })).toBeVisible();
	await control(request, 'clock', { advanceMs: 61000 });
	await expect(page.getByText('Window closed', { exact: true })).toBeVisible();
	await page.getByRole('combobox', { name: 'Protocol', exact: true }).selectOption('modbus_tcp');
	await apply(page);
	await control(request, 'actions', { type: 'network', wifi: false, ap: true });
	await expect(page.getByText('modbus_tcp · waiting_network', { exact: true })).toBeVisible();
});

test('KNX-01/02/03 commission, program, block and polling', async ({ page, request }) => {
	await login(page);
	await page.getByRole('tab', { name: 'Protocol', exact: true }).click();
	await page.getByRole('combobox', { name: 'Protocol', exact: true }).selectOption('knx_ip');
	await apply(page);
	await page.getByRole('tab', { name: 'KNX', exact: true }).click();
	await page.getByLabel('Individual address', { exact: true }).fill('1.1.10');
	await page.getByRole('checkbox', { name: /Take over for web editing/ }).check();
	await page.getByRole('button', { name: 'Apply KNX commissioning' }).click();
	await expect(page.getByText(/Owner: web · Revision 2/)).toBeVisible();
	await page.getByRole('button', { name: 'Enter programming mode' }).click();
	await expect(page.getByRole('button', { name: 'Exit programming mode' })).toBeVisible();
	await control(request, 'actions', { type: 'knx-object', number: 1, value: true });
	await control(request, 'actions', { type: 'knx-object', number: 2, value: true });
	await page.getByRole('tab', { name: 'Outputs', exact: true }).click();
	await expect(card(page).getByRole('button', { name: 'Turn OFF', exact: true })).toBeDisabled();
	await card(page).getByRole('button', { name: 'Clear block' }).click();
	await expect(card(page).getByRole('button', { name: 'Turn OFF', exact: true })).toBeEnabled();
});

test('LIVE-01 REST failure, held save and recovery', async ({ page, request }) => {
	await login(page);
	await control(request, 'faults', {
		path: '/rest/device/status',
		effect: 'http',
		status: 503,
		count: 5
	});
	// Live telemetry normally avoids REST polling; a disconnect triggers recovery.
	await control(request, 'socket', { action: 'close' });
	await expect(page.getByText(/Connection unavailable. Controls are disabled/)).toBeVisible();
	await expect(card(page).getByRole('button', { name: 'Turn ON', exact: true })).toBeDisabled();
	await control(request, 'faults', { clear: true });
	await expect(card(page).getByRole('button', { name: 'Turn ON', exact: true })).toBeEnabled();
	await control(request, 'faults', { path: '/rest/device/commands', effect: 'hold' });
	await card(page).getByRole('button', { name: 'Turn ON', exact: true }).click();
	await expect(card(page).getByRole('button', { name: 'Turn ON', exact: true })).toBeDisabled();
	await control(request, 'faults', { clear: true });
	await expect(card(page).getByRole('button', { name: 'Turn ON', exact: true })).toBeEnabled();
});

test('WS-02 socket reconnect keeps telemetry and UI operational', async ({ page, request }) => {
	await login(page);
	await expect.poll(async () => (await state(request)).connections).toBe(1);
	const reconnect = page.waitForEvent('websocket');
	await control(request, 'socket', { action: 'close' });
	await reconnect;
	await control(request, 'actions', {
		type: 'notification',
		level: 'info',
		message: 'After reconnect'
	});
	// Retry notification through the control plane if subscription has not arrived yet.
	await expect(async () => {
		await control(request, 'actions', {
			type: 'notification',
			level: 'info',
			message: 'Telemetry restored'
		});
		await expect(page.getByText('Telemetry restored').first()).toBeVisible();
	}).toPass();
});

for (const [path, text] of [
	['/wifi/sta', 'WiFi'],
	['/wifi/ap', 'Access Point'],
	['/connections/ntp', 'Network Time'],
	['/system/status', 'ESP32-S3'],
	['/system/metrics', 'System Metrics'],
	['/user', 'admin'],
	['/system/update', 'Upload Firmware']
])
	test(`NAV-01 ${path} renders without browser exceptions`, async ({ page }) => {
		const errors: string[] = [];
		page.on('pageerror', (e) => errors.push(e.message));
		await login(page, 'admin', path);
		await expect(page.getByText(text, { exact: false }).first()).toBeVisible();
		if (path === '/system/metrics') await expect(page.locator('canvas')).toHaveCount(4);
		expect(errors).toEqual([]);
	});

test('ETH-01 Ethernet profile and link status', async ({ page, request }) => {
	await control(request, 'reset', { profile: 'ethernet' });
	await control(request, 'actions', {
		type: 'network',
		ethernet: true,
		wifi: false,
		interface: 'eth0'
	});
	await login(page, 'admin', '/ethernet');
	await expect(page.getByText('192.0.2.10', { exact: true })).toBeVisible();
});

test('OTA-01 file validation and MD5 upload', async ({ page }) => {
	await login(page, 'admin', '/system/update');
	await page
		.locator('#binFile')
		.setInputFiles({ name: 'bad.txt', mimeType: 'text/plain', buffer: Buffer.from('bad') });
	await expect(page.getByText(/Invalid file type/)).toBeVisible();
	await page.locator('#binFile').setInputFiles({
		name: 'firmware.md5',
		mimeType: 'text/plain',
		buffer: Buffer.from('0'.repeat(32))
	});
	await expect(page.getByText('MD5 hash uploaded successfully.', { exact: true })).toBeVisible();
});

test('LIFE-01 actuator factory reset requires ERASE and invalidates session', async ({
	page,
	request
}) => {
	await login(page);
	await page.getByRole('tab', { name: 'Maintenance', exact: true }).click();
	const reset = page.getByRole('button', { name: 'Erase configuration and restart' });
	await expect(reset).toBeDisabled();
	await page.getByLabel('Type ERASE to factory reset', { exact: true }).fill('ERASE');
	await reset.click();
	await expect.poll(async () => (await state(request)).config.revision).toBe(1);
	// Reset acknowledges before reboot. Wait through the simulated boot outage.
	await expect
		.poll(async () => {
			const token = await page.evaluate(
				() => JSON.parse(localStorage.getItem('user') || '{}').bearer_token
			);
			return (
				await request.get('/rest/verifyAuthorization', {
					headers: { Authorization: `Bearer ${token}` }
				})
			).status();
		})
		.toBe(401);
	await page.reload();
	await expect(page.getByRole('heading', { name: 'Login', exact: true })).toBeVisible();
});

test('NAV-02 template APIs and JSON/security-off profile switches', async ({ page, request }) => {
	await control(request, 'reset', { profile: 'template' });
	await login(page, 'admin', '/demo');
	await expect(page.getByText(/controls the LED via/).first()).toBeVisible();
	await control(request, 'reset', { profile: 'security-off' });
	await page.goto('/device');
	await expect(page.getByRole('heading', { name: 'Switching Actuator' })).toBeVisible();
});

// Some existing icon-only buttons have no accessible name. Scope those selectors
// to their visible section until accessibility improvements are separately made.
async function expand(page: Page, title: string) {
	await page.getByText(title, { exact: true }).locator('../..').getByRole('button').click();
}
async function submitSettings(page: Page, endpoint: string) {
	const response = page.waitForResponse(
		(r) => r.url().endsWith(`/rest/${endpoint}`) && r.request().method() === 'POST'
	);
	await page.getByRole('button', { name: 'Apply Settings', exact: true }).first().click();
	expect((await response).status()).toBe(200);
}

test('NTP-01 edit server and timezone and reload', async ({ page }) => {
	await login(page, 'admin', '/connections/ntp');
	await expand(page, 'Change NTP Settings');
	await page.getByLabel('Server', { exact: true }).fill('time.example.com');
	await page.getByLabel('Pick Time Zone', { exact: true }).selectOption('Asia/Shanghai');
	await submitSettings(page, 'ntpSettings');
	await page.reload();
	await expand(page, 'Change NTP Settings');
	await expect(page.getByLabel('Server', { exact: true })).toHaveValue('time.example.com');
	await expect(page.getByLabel('Pick Time Zone', { exact: true })).toHaveValue('Asia/Shanghai');
});

test('AP-01 AP settings save and persist', async ({ page }) => {
	await login(page, 'admin', '/wifi/ap');
	await page.getByLabel('SSID', { exact: true }).fill('Test Access Point');
	// Actuator fixture setup password is intentionally distinct from real hardware.
	await page.getByLabel('Password', { exact: true }).fill('sim-password');
	await page.getByLabel('Preferred Channel', { exact: true }).fill('11');
	await submitSettings(page, 'apSettings');
	await page.reload();
	await expect(page.getByLabel('SSID', { exact: true })).toHaveValue('Test Access Point');
	await expect(page.getByLabel('Preferred Channel', { exact: true })).toHaveValue('11');
});

test('WIFI-01 station hostname and connection mode persist', async ({ page }) => {
	await login(page, 'admin', '/wifi/sta');
	await page.getByLabel('Host Name (mDNS)', { exact: true }).fill('test-actuator');
	await page.getByLabel('WiFi Connection Mode', { exact: true }).selectOption('2');
	await submitSettings(page, 'wifiSettings');
	await page.reload();
	await expect(page.getByLabel('Host Name (mDNS)', { exact: true })).toHaveValue('test-actuator');
	await expect(page.getByLabel('WiFi Connection Mode', { exact: true })).toHaveValue('2');
});

test('WIFI-02 scan modal receives asynchronous results and closes', async ({ page }) => {
	await login(page, 'admin', '/wifi/sta');
	// The two adjacent icon-only buttons following hostname/mode are add and scan.
	await page.locator('.flex.justify-end.w-full.gap-x-2 button').nth(1).click();
	await expect(page.getByRole('dialog').getByText('Simulator WiFi', { exact: true })).toBeVisible();
	await page
		.getByRole('dialog')
		.getByRole('button', { name: /Cancel|Close/ })
		.click();
	await expect(page.getByRole('dialog')).toBeHidden();
});

test('ETH-01 hostname discard and save', async ({ page, request }) => {
	await control(request, 'reset', { profile: 'ethernet' });
	await login(page, 'admin', '/ethernet');
	await page.getByLabel('Host Name (mDNS)', { exact: true }).fill('discard-this');
	await page.getByRole('button', { name: 'Discard Changes' }).click();
	await expect(page.getByLabel('Host Name (mDNS)', { exact: true })).toHaveValue('esp32-simulator');
	await page.getByLabel('Host Name (mDNS)', { exact: true }).fill('ethernet-test');
	await submitSettings(page, 'ethernetSettings');
	await page.reload();
	await expect(page.getByLabel('Host Name (mDNS)', { exact: true })).toHaveValue('ethernet-test');
});

test('MQTT-02 actuator Home Assistant discovery opt-in persists', async ({ page, request }) => {
	await control(request, 'reset', { profile: 'actuator' });
	await login(page, 'admin', '/connections/mqtt');
	await expand(page, 'Change MQTT Settings');
	const discovery = page.getByLabel('Home Assistant discovery', { exact: true });
	await expect(discovery).not.toBeChecked();
	await discovery.check();
	await submitSettings(page, 'mqttSettings');
	await page.reload();
	await expand(page, 'Change MQTT Settings');
	await expect(discovery).toBeChecked();
	await discovery.uncheck();
	await submitSettings(page, 'mqttSettings');
});

test('MQTT-01 template connection and discovery settings', async ({ page, request }) => {
	await control(request, 'reset', { profile: 'template' });
	await login(page, 'admin', '/connections/mqtt');
	await expand(page, 'Change MQTT Settings');
	await expect(page.getByLabel('Home Assistant discovery', { exact: true })).toHaveCount(0);
	await page.getByLabel('Enable MQTT', { exact: true }).check();
	await page.getByLabel('URI', { exact: true }).fill('mqtt://broker.example.com:1883');
	await submitSettings(page, 'mqttSettings');
	await page.reload();
	await expand(page, 'Change MQTT Settings');
	await expect(page.getByLabel('URI', { exact: true })).toHaveValue(
		'mqtt://broker.example.com:1883'
	);
	await expand(page, 'MQTT Broker Settings');
	await expect(page.getByLabel('Unique ID', { exact: true })).toHaveValue('simulator');
});

test('USER-01 edit role/channel permissions revokes sessions', async ({ page }) => {
	await login(page, 'admin', '/user');
	await page
		.getByRole('row')
		.filter({ has: page.getByRole('cell', { name: 'viewer', exact: true }) })
		.getByRole('button')
		.first()
		.click();
	await page.getByRole('dialog').getByLabel('Actuator role').selectOption('operator');
	await page
		.getByRole('dialog')
		.getByLabel('Allowed relay bit mask (1–63; 0 denies all)')
		.fill('2');
	await page.getByRole('dialog').getByRole('button', { name: 'Save', exact: true }).click();
	await expect(page.getByRole('heading', { name: 'Login', exact: true })).toBeVisible();
	await login(page, 'viewer');
	await expect(card(page).getByRole('button', { name: 'Turn ON', exact: true })).toBeDisabled();
	await expect(card(page, 2).getByRole('button', { name: 'Turn ON', exact: true })).toBeEnabled();
});

test('DEMO-01 template REST and event LED controls', async ({ page, request }) => {
	await control(request, 'reset', { profile: 'template' });
	await login(page, 'admin', '/demo');
	await page.getByRole('checkbox', { name: 'Light State?' }).first().check();
	await page.getByRole('button', { name: 'Save', exact: true }).click();
	await expect(page.getByRole('checkbox', { name: 'Light State?' }).nth(1)).toBeChecked();
	await page.getByRole('checkbox', { name: 'Light State?' }).nth(1).uncheck();
	await page.getByRole('button', { name: 'Reload', exact: true }).click();
	await expect(page.getByRole('checkbox', { name: 'Light State?' }).first()).not.toBeChecked();
});

test('OTA-01 BIN upload progress and success', async ({ page }) => {
	await login(page, 'admin', '/system/update');
	const binary = Buffer.alloc(128);
	binary[0] = 0xe9;
	binary.writeUInt16LE(9, 12);
	await page
		.locator('#binFile')
		.setInputFiles({ name: 'simulator.bin', mimeType: 'application/octet-stream', buffer: binary });
	await page.getByRole('dialog').getByRole('button', { name: 'Upload', exact: true }).click();
	await expect(page.getByText('Update finished successfully.', { exact: true })).toBeVisible();
});

test('OTA-01 wrong target produces firmware error dialog', async ({ page }) => {
	await login(page, 'admin', '/system/update');
	await page.locator('#binFile').setInputFiles({
		name: 'wrong.bin',
		mimeType: 'application/octet-stream',
		buffer: Buffer.alloc(128)
	});
	await page.getByRole('dialog').getByRole('button', { name: 'Upload', exact: true }).click();
	await expect(page.getByText('Wrong firmware for this device', { exact: true })).toBeVisible();
});

test('LIFE-01 restart confirmation can be aborted', async ({ page, request }) => {
	await login(page, 'admin', '/system/status');
	const before = (await state(request)).status.uptime;
	await page.getByRole('button', { name: 'Restart', exact: true }).click();
	await page.getByRole('dialog').getByRole('button', { name: 'Abort', exact: true }).click();
	expect((await state(request)).status.uptime).toBeGreaterThanOrEqual(before);
	await expect(page.getByRole('dialog')).toBeHidden();
});

test('WS-01 JSON profile emits notifications to real browser', async ({ page, request }) => {
	await control(request, 'reset', { profile: 'json' });
	await login(page);
	await expect.poll(async () => (await state(request)).connections).toBe(1);
	await expect(async () => {
		await control(request, 'actions', { type: 'notification', message: 'JSON transport works' });
		await expect(page.getByText('JSON transport works').first()).toBeVisible();
	}).toPass();
});

test('CORE-01 coredump download is available after async response', async ({ page, request }) => {
	await control(request, 'actions', { type: 'coredump', available: true });
	await login(page, 'admin', '/system/coredump');
	await expect(
		page.getByRole('button', { name: 'Download Core Dump (coredump.bin)' })
	).toBeVisible();
});

test('DEVICE-UI responsive sections retain readable controls across languages and themes', async ({
	page
}, testInfo) => {
	await login(page, 'admin', '/system/ui');
	for (const [language, theme] of [
		['en', 'light'],
		['de', 'dim']
	]) {
		await page.locator('#ui-language').selectOption(language);
		await page.locator('#ui-theme').selectOption(theme);
		await page.goto('/device');
		await expect(page.locator('.device-page .card').first()).toBeVisible();
		for (let i = 0; i < 6; i++) {
			await page.getByRole('tab').nth(i).click();
			await expect(page.locator('.device-page .card').first()).toBeVisible();
			if (i === 0) await page.locator('.device-page summary').first().click();
			if (i === 4) await expect(page.locator('table')).toBeVisible();
			expect(
				await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth),
				`${language} section ${i}`
			).toBe(true);
			await page.screenshot({
				path: testInfo.outputPath(`device-${language}-${i}.png`),
				fullPage: true
			});
		}
		await page.goto('/system/ui');
	}
});

test('DEVICE-UI compact navigation and KNX editor adapt across target viewports', async ({
	page
}) => {
	await login(page);
	const viewports = [
		{ width: 320, height: 568 },
		{ width: 360, height: 800 },
		{ width: 390, height: 844 },
		{ width: 667, height: 375 },
		{ width: 844, height: 390 },
		{ width: 915, height: 412 },
		{ width: 768, height: 1024 },
		{ width: 1280, height: 720 }
	];
	for (const viewport of viewports) {
		await page.setViewportSize(viewport);
		await page.getByRole('tab', { name: 'Outputs', exact: true }).click();
		const layout = await page.evaluate(() => {
			const tablist = document.querySelector<HTMLElement>('[role="tablist"]')!,
				tabs = [...tablist.querySelectorAll<HTMLElement>('[role="tab"]')],
				content = tablist.nextElementSibling!.getBoundingClientRect(),
				navigation = tablist.getBoundingClientRect();
			return {
				pageOverflow: document.documentElement.scrollWidth > document.documentElement.clientWidth,
				contentGap: Math.round(content.top - navigation.bottom),
				tabRows: new Set(tabs.map((tab) => Math.round(tab.getBoundingClientRect().top))).size,
				minTabHeight: Math.min(...tabs.map((tab) => tab.getBoundingClientRect().height)),
				navigationOverflow: tablist.scrollWidth > tablist.clientWidth
			};
		});
		expect(layout.pageOverflow, JSON.stringify(viewport)).toBe(false);
		expect(layout.contentGap, JSON.stringify(viewport)).toBeLessThanOrEqual(8);
		expect(layout.tabRows, JSON.stringify(viewport)).toBe(1);
		expect(layout.minTabHeight, JSON.stringify(viewport)).toBeGreaterThanOrEqual(44);
		expect(layout.navigationOverflow, JSON.stringify(viewport)).toBe(viewport.width < 768);

		await page.getByRole('tab', { name: 'KNX', exact: true }).click();
		await expect(page.getByRole('heading', { name: 'KNXnet/IP commissioning' })).toBeVisible();
		const knx = await page.evaluate(() => {
			const editor = document.querySelector<HTMLElement>('.knx-object-editor')!,
				row = editor.querySelector('tbody tr')!;
			return {
				pageOverflow: document.documentElement.scrollWidth > document.documentElement.clientWidth,
				localOverflow: editor.scrollWidth > editor.clientWidth,
				rowDisplay: getComputedStyle(row).display
			};
		});
		expect(knx.pageOverflow, JSON.stringify(viewport)).toBe(false);
		expect(knx.localOverflow, JSON.stringify(viewport)).toBe(false);
		expect(knx.rowDisplay, JSON.stringify(viewport)).toBe(
			viewport.width < 768 ? 'grid' : 'table-row'
		);
	}
});

test('MODBUS-MAP reference follows selection and distinguishes RTU diagnostics', async ({
	page
}, testInfo) => {
	await login(page);
	await page.getByRole('tab', { name: 'Protocol', exact: true }).click();
	const map = page.getByRole('region', { name: 'Modbus function map' });
	const protocol = page.getByRole('combobox', { name: 'Protocol', exact: true });
	await expect(map).toBeHidden();
	await protocol.selectOption('modbus_rtu');
	await expect(map).toBeVisible();
	await expect(
		map.getByText('Diagnostics: return query data (RTU only)', { exact: true })
	).toBeVisible();
	await expect(map.getByRole('rowheader', { name: '0x0023', exact: true }).first()).toBeVisible();
	await expect(
		map.getByText('Read status only through DI 0x0023; this coil address is reserved/unmapped', {
			exact: true
		})
	).toBeVisible();
	for (const section of ['Discrete inputs', 'Holding registers', 'Input registers']) {
		await map.locator('summary').filter({ hasText: section }).click();
	}
	await expect(map.locator('table')).toHaveCount(4);
	await expect(
		map.getByText('Write 0xA55A, read returns 0; no action for 0', { exact: true })
	).toBeVisible();
	expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
	await page.screenshot({ path: testInfo.outputPath('modbus-map.png'), fullPage: true });
	const download = page.waitForEvent('download');
	await map.getByRole('link', { name: 'Download reference (English)' }).click();
	expect((await download).suggestedFilename()).toBe('actuator-modbus-map.md');
	await protocol.selectOption('modbus_tcp');
	await expect(map).toBeVisible();
	await expect(
		map.getByText('Diagnostics: return query data (RTU only)', { exact: true })
	).toBeHidden();
	await protocol.selectOption('knx_ip');
	await expect(map).toBeHidden();
	await protocol.selectOption('off');
	await expect(map).toBeHidden();
});

test('FIX-01 commands keep other controls, DOM nodes and unsaved settings stable', async ({
	page,
	request
}) => {
	await login(page);
	const first = card(page, 1),
		second = card(page, 2);
	await first.getByText('Channel settings', { exact: true }).click();
	const name = first.getByRole('textbox', { name: 'Name', exact: true });
	await name.fill('Unsaved channel');
	let statusRequests = 0;
	page.on('request', (req) => {
		if (req.url().endsWith('/rest/device/status')) statusRequests++;
	});
	const handle = await name.elementHandle();
	await control(request, 'faults', {
		path: '/rest/device/commands',
		effect: 'latency',
		ms: 700,
		count: 1
	});
	await first.getByRole('button', { name: 'Turn ON', exact: true }).click();
	await expect(second.getByRole('button', { name: 'Turn ON', exact: true })).toBeEnabled();
	await expect(first.getByRole('button', { name: 'Turn OFF', exact: true })).toBeVisible();
	expect(statusRequests).toBe(0);
	expect(await handle!.evaluate((el) => el.isConnected)).toBe(true);
	await expect(name).toHaveValue('Unsaved channel');
	await expect(first.locator('details')).toHaveAttribute('open', '');
	await page.getByRole('button', { name: 'Discard', exact: true }).click();
	await page.getByRole('button', { name: 'All enabled channels ON', exact: true }).click();
	await expect
		.poll(async () => (await state(request)).status.relays.every((r: any) => r.on))
		.toBe(true);
	await page.getByRole('button', { name: 'All enabled channels OFF', exact: true }).click();
	await expect
		.poll(async () => (await state(request)).status.relays.every((r: any) => !r.on))
		.toBe(true);
	await page.getByRole('tab', { name: 'Indicators', exact: true }).click();
	const rgb = page
		.locator('.card')
		.filter({ has: page.getByRole('heading', { name: 'RGB status indicator' }) });
	const before = await rgb.boundingBox();
	await rgb.getByRole('checkbox', { name: 'Enabled', exact: true }).uncheck();
	const after = await rgb.boundingBox();
	expect(Math.abs(after!.y - before!.y)).toBeLessThan(2);
	await page.getByRole('button', { name: 'Discard', exact: true }).click();
});

test('FIX-02 all-channel target round trips and executes gesture', async ({ page, request }) => {
	await login(page);
	await page.getByRole('tab', { name: 'Button', exact: true }).click();
	await page.getByRole('combobox', { name: 'Single click', exact: true }).selectOption('1');
	await page
		.getByRole('combobox', { name: 'Target', exact: true })
		.first()
		.selectOption({ label: 'All channels' });
	await apply(page);
	expect((await state(request)).config.clicks[0]).toEqual({ action: 1, target: 0 });
	await control(request, 'actions', { type: 'gesture', clicks: 1 });
	await page.getByRole('tab', { name: 'Outputs', exact: true }).click();
	await expect(page.getByRole('button', { name: 'Turn OFF', exact: true })).toHaveCount(6);
});

test('FIX-03 telemetry pause does not cause two-second reconnect churn', async ({
	page,
	request
}) => {
	let opened = 0;
	page.on('websocket', (ws) => {
		if (ws.url().includes('/ws/events')) opened++;
	});
	await login(page);
	await expect.poll(async () => (await state(request)).connections).toBe(1);
	await control(request, 'socket', { action: 'pause' });
	await page.waitForTimeout(5000);
	expect(opened).toBe(1);
	await expect(page.getByText('Connection to device lost', { exact: true })).toBeHidden();
	await control(request, 'socket', { action: 'pause', active: false });
	await expect.poll(async () => (await state(request)).connections).toBe(1);
});

test('FIX-04 transient REST failure recovers while WebSocket state remains live', async ({
	page,
	request
}) => {
	await login(page);
	await page.getByRole('tab', { name: 'Indicators', exact: true }).click();
	await control(request, 'faults', {
		path: '/rest/device/status',
		effect: 'http',
		status: 503,
		count: 1
	});
	await control(request, 'socket', { action: 'close' });
	await expect(page.getByText(/Connection unavailable\. Controls are disabled\./)).toBeVisible();
	await expect(page.getByText(/Connection unavailable\. Controls are disabled\./)).toBeHidden({
		timeout: 10000
	});
	await expect(
		page.getByRole('button', { name: 'Identify for 5 seconds', exact: true })
	).toBeEnabled();
});

test('FIX-05 live updates continue during a poll and its old response cannot revert them', async ({
	page,
	request
}) => {
	await login(page);
	let release!: () => void;
	const held = new Promise<void>((resolve) => {
		release = resolve;
	});
	let captured!: () => void;
	const ready = new Promise<void>((resolve) => {
		captured = resolve;
	});
	await page.route(
		'**/rest/device/status',
		async (route) => {
			const response = await route.fetch();
			captured();
			await held;
			await route.fulfill({ response });
		},
		{ times: 1 }
	);
	await control(request, 'socket', { action: 'close' });
	await ready;
	try {
		await control(request, 'actions', { type: 'relay', channel: 0, value: true });
		await expect(
			card(page, 1).getByRole('button', { name: 'Turn OFF', exact: true })
		).toBeVisible();
	} finally {
		release();
	}
	await page.waitForTimeout(500);
	await expect(card(page, 1).getByRole('button', { name: 'Turn OFF', exact: true })).toBeVisible();
});

test('MCP-01 configure, preserve secret, reconnect and remove endpoint', async ({
	page,
	request
}) => {
	await login(page, 'admin', '/connections/xiaozhi-mcp');
	await expect(
		page.getByRole('heading', { name: 'Xiaozhi MCP settings', exact: true })
	).toBeVisible();
	await page.getByLabel('Enable Xiaozhi MCP', { exact: true }).check();
	await page.getByLabel('Device alias', { exact: true }).fill('Plant relays');
	await page
		.getByLabel('MCP endpoint', { exact: true })
		.fill('wss://example.invalid/mcp/?token=e2e-private');
	await page.getByRole('checkbox', { name: '1. Channel 1', exact: true }).check();
	await page.getByRole('button', { name: 'Apply Settings', exact: true }).click();
	await expect(
		page.getByText('Settings saved. Connection status updates separately.', { exact: true })
	).toBeVisible();
	await expect(page.getByLabel('MCP endpoint', { exact: true })).toHaveValue('');
	await expect(page.getByText('actuator_set_relay_020000000010', { exact: true })).toBeVisible();
	await page.reload();
	await expect(page.getByLabel('Device alias', { exact: true })).toHaveValue('Plant relays');
	await expect(page.getByLabel('MCP endpoint', { exact: true })).toHaveValue('');
	expect(await page.evaluate(() => JSON.stringify(localStorage))).not.toContain('e2e-private');
	await page.getByRole('button', { name: 'Reconnect', exact: true }).click();
	await expect(page.getByText('Reconnection requested.', { exact: true })).toBeVisible();
	await control(request, 'actions', { type: 'mcp', state: 'ready' });
	await expect(page.getByText('Ready', { exact: true })).toBeVisible();
	await page.getByLabel('Enable Xiaozhi MCP', { exact: true }).uncheck();
	await page.getByLabel('Remove saved endpoint', { exact: true }).check();
	await page.getByRole('button', { name: 'Apply Settings', exact: true }).click();
	await expect(page.getByText('No endpoint configured.', { exact: true })).toBeVisible();
});

test('MCP-02 viewer status and feature-unavailable route', async ({ page, request }) => {
	await login(page, 'viewer', '/connections/xiaozhi-mcp');
	await expect(
		page.getByText('Connection settings are managed by an administrator.', { exact: true })
	).toBeVisible();
	await expect(page.getByLabel('MCP endpoint', { exact: true })).toHaveCount(0);
	await control(request, 'reset', { profile: 'mcp-off' });
	await page.evaluate(() => localStorage.removeItem('user'));
	await login(page, 'admin', '/connections/xiaozhi-mcp');
	await expect(
		page.getByText('Xiaozhi MCP is unavailable in this firmware.', { exact: true })
	).toBeVisible();
});

test('MCP-03 standalone navigation and failed saves preserve the draft', async ({
	page,
	request
}) => {
	await control(request, 'reset', { profile: 'mcp-only' });
	await login(page, 'admin', '/connections/xiaozhi-mcp');
	const menuToggle = page.locator('label[for="main-menu"]').first();
	if (await menuToggle.isVisible()) await menuToggle.click();
	await expect(page.getByRole('link', { name: 'Xiaozhi MCP', exact: true })).toBeVisible();
	await expect(page.getByRole('link', { name: 'MQTT', exact: true })).toHaveCount(0);
	await expect(page.getByRole('link', { name: 'NTP', exact: true })).toHaveCount(0);
	await page.getByRole('link', { name: 'Xiaozhi MCP', exact: true }).click();
	await page.getByLabel('Device alias', { exact: true }).fill('Unsaved draft');
	await page.getByLabel('MCP endpoint', { exact: true }).fill('wss://example.invalid/?token=draft');
	await page.route('**/rest/xiaozhiMcpSettings', (route) =>
		route.request().method() === 'POST'
			? route.fulfill({ status: 503, json: { error: 'storage_failed' } })
			: route.continue()
	);
	await page.getByRole('button', { name: 'Apply Settings', exact: true }).click();
	await expect(page.getByText('Request failed. Please try again.', { exact: true })).toBeVisible();
	await expect(page.getByLabel('Device alias', { exact: true })).toHaveValue('Unsaved draft');
	await expect(page.getByLabel('MCP endpoint', { exact: true })).toHaveValue(
		'wss://example.invalid/?token=draft'
	);
	await expect(page.getByRole('button', { name: 'Reconnect', exact: true })).toBeDisabled();
	await page.unroute('**/rest/xiaozhiMcpSettings');
	await page.getByRole('button', { name: 'Reload saved settings', exact: true }).click();
	await expect(page.getByLabel('Device alias', { exact: true })).toHaveValue('Switching Actuator');
	await expect(page.getByLabel('MCP endpoint', { exact: true })).toHaveValue('');
});
