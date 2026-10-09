import { chromium } from '@playwright/test';
import { execFileSync } from 'node:child_process';
import { mkdirSync, readdirSync, statSync } from 'node:fs';
import { join, resolve } from 'node:path';

const baseURL = process.env.DEMO_BASE_URL || 'http://127.0.0.1:5173';
const outputDir = resolve(process.env.DEMO_OUTPUT_DIR || 'test-results/marketing');
const outputName = process.env.DEMO_OUTPUT_NAME || 'edge-switching-actuator-demo';
if (!/^[a-z0-9][a-z0-9._-]*$/i.test(outputName))
	throw new Error('DEMO_OUTPUT_NAME must be a filename without a path or extension');
const outputWebm = join(outputDir, `${outputName}.webm`);
const outputMp4 = join(outputDir, `${outputName}.mp4`);
const controlURL = process.env.SIM_CONTROL_URL || 'http://127.0.0.1:3081';
const controlToken = process.env.SIM_CONTROL_TOKEN;
const pause = (ms) => new Promise((done) => setTimeout(done, ms));

mkdirSync(outputDir, { recursive: true });
const health = await fetch(`${baseURL}/rest/features`);
if (!health.ok) throw new Error(`Interface simulator is not ready at ${baseURL}`);

const browser = await chromium.launch({ headless: true });
const auth = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const authPage = await auth.newPage();
await authPage.goto(`${baseURL}/device`);
await authPage.getByLabel('Username', { exact: true }).fill('admin');
await authPage.getByLabel('Password', { exact: true }).fill('sim-admin');
await authPage.getByRole('button', { name: 'Login', exact: true }).click();
await authPage.getByRole('heading', { name: 'Switching Actuator' }).waitFor();
await authPage.goto(`${baseURL}/system/ui`);
await authPage.locator('#ui-language').selectOption('en');
await authPage.locator('#ui-theme').selectOption('dark');
await authPage.locator('html[data-theme="dark"]').waitFor();
const storageState = await auth.storageState();
await auth.close();

const context = await browser.newContext({
	viewport: { width: 1280, height: 720 },
	storageState,
	recordVideo: { dir: outputDir, size: { width: 1280, height: 720 } }
});
const page = await context.newPage();
const video = page.video();
await page.goto(`${baseURL}/device`);
await page.getByRole('heading', { name: 'Switching Actuator' }).waitFor();
await page.locator('html[data-theme="dark"]').waitFor();
await page.evaluate(() => {
	const style = document.createElement('style');
	style.textContent = `
		#demo-pointer { position: fixed; left: 0; top: 0; z-index: 10001; width: 22px; height: 22px;
			border: 3px solid white; border-radius: 50%; background: color-mix(in srgb, var(--color-primary) 80%, black);
			box-shadow: 0 2px 12px #0008; pointer-events: none; transform: translate(-40px, -40px);
			transition: transform 420ms cubic-bezier(.2,.8,.2,1); }
		#demo-disclosure { position: fixed; right: 18px; top: 16px; z-index: 10000; padding: 6px 10px;
			border-radius: 4px; background: #111d; color: white; font: 600 11px/1.2 sans-serif; letter-spacing: .08em; }
		.demo-slate { position: fixed; inset: 0; z-index: 10002; display: grid; place-content: center; gap: 10px;
			background: #10251ff2; color: white; text-align: center; opacity: 0; transition: opacity 450ms ease; }
		.demo-slate.visible { opacity: 1; }
		.demo-slate strong { font: 700 44px/1.05 sans-serif; }
		.demo-slate span { font: 400 20px/1.4 sans-serif; color: #bfe5d7; }
	`;
	document.head.append(style);
	const pointer = document.createElement('div');
	pointer.id = 'demo-pointer';
	document.body.append(pointer);
	const disclosure = document.createElement('div');
	disclosure.id = 'demo-disclosure';
	disclosure.textContent = 'SIMULATOR DEMO';
	document.body.append(disclosure);
});

async function title(primary, secondary, duration = 1900) {
	await page.evaluate(
		({ primary, secondary }) => {
			document.querySelector('.demo-slate')?.remove();
			const slate = document.createElement('div');
			slate.className = 'demo-slate';
			slate.innerHTML = `<strong></strong><span></span>`;
			slate.querySelector('strong').textContent = primary;
			slate.querySelector('span').textContent = secondary;
			document.body.append(slate);
			requestAnimationFrame(() => slate.classList.add('visible'));
		},
		{ primary, secondary }
	);
	await pause(duration);
	await page.evaluate(() => document.querySelector('.demo-slate')?.classList.remove('visible'));
	await pause(500);
	await page.evaluate(() => document.querySelector('.demo-slate')?.remove());
}

async function point(locator, click = false) {
	await locator.scrollIntoViewIfNeeded();
	const box = await locator.boundingBox();
	if (!box) throw new Error('Demo target is not visible');
	await page.evaluate(
		({ x, y }) => {
			document.querySelector('#demo-pointer').style.transform =
				`translate(${x - 11}px, ${y - 11}px)`;
		},
		{ x: box.x + box.width / 2, y: box.y + box.height / 2 }
	);
	await pause(500);
	if (click) {
		await locator.click();
		await pause(900);
	}
}

async function clearToasts() {
	await page.evaluate(() => document.querySelector('.toast')?.replaceChildren());
}

async function simulatorAction(body) {
	if (!controlToken) return false;
	const response = await fetch(`${controlURL}/__sim/actions`, {
		method: 'POST',
		headers: {
			Authorization: `Bearer ${controlToken}`,
			'Content-Type': 'application/json'
		},
		body: JSON.stringify(body)
	});
	if (!response.ok) throw new Error(`Simulator action failed: ${response.status}`);
	return true;
}

await title('Edge Switching Actuator', 'Complete six-channel control in one responsive interface.');
await clearToasts();
await pause(1000);

const channelCard = (number) =>
	page
		.locator('.card')
		.filter({ has: page.getByRole('heading', { name: new RegExp(`^${number}\\. `) }) });
await point(channelCard(1).getByRole('button', { name: 'Turn ON', exact: true }), true);
await point(channelCard(1).getByRole('button', { name: 'Turn OFF', exact: true }), true);
await point(page.getByRole('button', { name: 'All enabled channels ON' }), true);
await point(page.getByRole('button', { name: 'All enabled channels OFF' }), true);
await clearToasts();

await point(page.getByRole('tab', { name: 'Indicators', exact: true }), true);
await point(page.getByRole('button', { name: 'Identify for 5 seconds' }), true);
await point(page.getByRole('button', { name: 'Test tone', exact: true }), true);
await clearToasts();

await point(page.getByRole('tab', { name: 'Button', exact: true }), true);
await page.getByRole('heading', { name: 'BOOT button' }).scrollIntoViewIfNeeded();
await simulatorAction({ type: 'gesture', clicks: 2 });
await pause(2000);
await clearToasts();

await point(page.getByRole('tab', { name: 'Protocol', exact: true }), true);
await page.getByRole('heading', { name: 'Protocol Interface selection' }).scrollIntoViewIfNeeded();
await pause(2400);

await point(page.getByRole('tab', { name: 'KNX', exact: true }), true);
await page.getByRole('heading', { name: 'KNXnet/IP commissioning' }).scrollIntoViewIfNeeded();
const enterProgramming = page.getByRole('button', { name: 'Enter programming mode', exact: true });
if (await enterProgramming.isEnabled()) {
	await point(enterProgramming, true);
	await point(page.getByRole('button', { name: 'Exit programming mode', exact: true }), true);
}
await pause(2200);

await point(page.getByRole('tab', { name: 'Maintenance', exact: true }), true);
await page.getByRole('heading', { name: 'Operations' }).scrollIntoViewIfNeeded();
await pause(2400);

await title(
	'Control with confidence',
	'Outputs, diagnostics, button actions, protocols, KNX, and maintenance.',
	1800
);
await pause(2500);
await context.close();
await video.saveAs(outputWebm);
await browser.close();

function findFfmpeg(root) {
	if (!root) return;
	for (const entry of readdirSync(root, { withFileTypes: true })) {
		const path = join(root, entry.name);
		if (entry.isFile() && /^ffmpeg(?:-win64)?\.exe$/i.test(entry.name)) return path;
		if (entry.isDirectory() && entry.name.startsWith('ffmpeg')) {
			for (const child of readdirSync(path, { withFileTypes: true }))
				if (child.isFile() && /^ffmpeg(?:-win64)?\.exe$/i.test(child.name))
					return join(path, child.name);
		}
	}
}

const ffmpeg = findFfmpeg(join(process.env.LOCALAPPDATA || '', 'ms-playwright'));
if (ffmpeg) {
	try {
		execFileSync(
			ffmpeg,
			[
				'-y',
				'-i',
				outputWebm,
				'-an',
				'-c:v',
				'libx264',
				'-pix_fmt',
				'yuv420p',
				'-movflags',
				'+faststart',
				outputMp4
			],
			{ stdio: 'pipe' }
		);
		console.log(`MP4 saved to ${outputMp4}`);
	} catch {
		console.warn('MP4 conversion is unavailable in the bundled FFmpeg; WebM remains available.');
	}
}
console.log(`WebM saved to ${outputWebm} (${statSync(outputWebm).size} bytes)`);
