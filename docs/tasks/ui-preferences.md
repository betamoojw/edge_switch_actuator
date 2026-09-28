# System UI preferences

## Request and dev-branch review

Add System → UI with application-wide language and daisyUI theme selection.
The dev branch uses Svelte 5, SvelteKit SPA routing, Tailwind 4 and daisyUI 5.
Labels are currently embedded in route components, shared dialogs and charts;
there is no localization service. Themes are currently corporate/business.
Device settings and protocol identifiers must retain their existing API values.

## Implementation requirements

- Offer English, Deutsch, Español, Français, Polski, 简体中文 and 繁體中文.
- Bundle translations locally; no translation service or network dependency.
- Use industrial automation terminology for relay outputs, commissioning,
  individual/group addresses, watchdogs and communication faults.
- Translate navigation, pages, forms, dialogs, notifications, validation,
  accessible labels and charts. Preserve user-entered names, addresses,
  protocol identifiers and raw device diagnostics.
- Offer light (default), dark (prefers-dark), nord, dim and retro (Sepia).
  Automatic follows the OS preference; an explicit theme overrides it.
- Apply changes immediately and retain preferences in localStorage per browser
  and device origin. Invalid/unavailable storage must not prevent startup.
- Update the document language and theme, including the login page.
- Verify locale coverage, settings persistence, navigation, dark preference,
  invalid storage, existing frontend behavior, type checks and production build.

## Progress

- [x] Review dev branch and record implementation task.
- [x] Implement preferences and System → UI.
- [x] Localize the application.
- [x] Run verification and record results.

## Delivered implementation

- `/system/ui` is available under System for all authenticated roles, and when
  firmware security is disabled. No device configuration endpoint is needed.
- Seven languages use 402 locally bundled source messages. Navigation, actuator
  controls, network/system pages, validation, shared dialogs, notifications,
  dates, durations and chart labels use the selected language. Interpolated
  notifications also update while visible when the language changes.
- Five daisyUI themes plus Automatic apply at the document root. Saved
  preferences are applied before first paint; chart colors follow preferences.
- Preferences persist per browser/device origin, synchronize across tabs, and
  tolerate malformed or unavailable localStorage. API values remain unchanged.
- Shared messages render as text so user-provided values are not treated as HTML.
- The core-dump page now uses reactive state for its asynchronous result.

## Validation — 2026-09-28

| Check | Result |
| --- | --- |
| Translation catalogue, parameters and UI key coverage | 2 tests passed; all six translated catalogues complete |
| Svelte / TypeScript | 0 errors, 0 warnings |
| Development Chromium regression suite | 44 passed |
| WebKit preference and main-route checks | 12 passed |
| Mobile Chromium preference and main-route checks | 12 passed |
| Final production static build regression suite | 44 passed, 0 failures, 0 skipped |
| Production build and simulator-marker exclusion | Passed |
| Desktop and mobile visual inspection | French/Nord layout inspected; final build confirms translated sign-in notification |
| Firefox | 12 tests could not launch: Windows `browserType.launch: spawn UNKNOWN`; standalone launch fails too |

Firefox's pre-existing Windows launch limitation is also recorded in
`docs/frontend-testing.md`. It remains enabled in the test configuration and is
not counted as a passing browser. No physical device was flashed or tested.

The existing single-bundle configuration still emits Vite's large-chunk warning.
Final JavaScript is 735.38 kB (238.28 kB gzip); CSS is 98.77 kB (17.36 kB gzip).
All translations are included in that local bundle; no runtime translation
service is used. Test reports and screenshots are in the ignored
`interface/test-results/` and `interface/playwright-report/` directories.

See [UI preferences](../ui-preferences.md) for operation and translation maintenance.
