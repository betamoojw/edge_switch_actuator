import { defineConfig, devices } from '@playwright/test';

const realDevice = process.env.E2E_TARGET === 'device';
if (realDevice && !process.env.DEVICE_HOST && !process.env.E2E_BASE_URL)
	throw new Error('Device tests require DEVICE_HOST or E2E_BASE_URL');
const port = process.env.FRONTEND_PORT || '5173';
const baseURL = process.env.E2E_BASE_URL || `http://127.0.0.1:${port}`;
export default defineConfig({
	testDir: './tests/e2e',
	testMatch: realDevice ? 'device.spec.ts' : 'frontend.spec.ts',
	fullyParallel: false,
	workers: 1,
	forbidOnly: !!process.env.CI,
	retries: process.env.CI ? 1 : 0,
	timeout: 30000,
	expect: { timeout: 8000 },
	outputDir: process.env.E2E_BUILT ? 'test-results/built' : 'test-results/browser',
	reporter: [
		['list'],
		[
			'html',
			{
				open: 'never',
				outputFolder: process.env.E2E_BUILT
					? 'playwright-report/built'
					: 'playwright-report/browser'
			}
		],
		[
			'junit',
			{ outputFile: process.env.E2E_BUILT ? 'test-results/built.xml' : 'test-results/browser.xml' }
		]
	],
	use: {
		baseURL,
		trace: realDevice ? 'off' : 'retain-on-failure',
		screenshot: 'only-on-failure',
		video: 'retain-on-failure'
	},
	projects: process.env.E2E_BUILT
		? [{ name: 'built-chromium', use: { ...devices['Desktop Chrome'] } }]
		: [
				{ name: 'chromium', use: { ...devices['Desktop Chrome'] } },
				{ name: 'firefox', use: { ...devices['Desktop Firefox'] } },
				{ name: 'webkit', use: { ...devices['Desktop Safari'] } },
				{ name: 'mobile', use: { ...devices['iPhone 13'], defaultBrowserType: 'chromium' } }
			],
	webServer: process.env.E2E_BASE_URL
		? undefined
		: {
				command: realDevice
					? 'npm run dev:device'
					: process.env.E2E_BUILT
						? 'node scripts/dev.mjs built'
						: 'npm run dev:sim',
				url: `${baseURL}/rest/features`,
				reuseExistingServer: false,
				timeout: 60000,
				stdout: 'pipe',
				stderr: 'pipe',
				env: {
					SIM_CONTROL_TOKEN: 'playwright-local-control',
					SIM_PROFILE: 'actuator',
					SIM_STATE_FILE: ''
				}
			}
});
