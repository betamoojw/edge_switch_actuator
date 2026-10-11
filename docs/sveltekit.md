# SvelteKit and theming

The project builds a static browser application with `@sveltejs/adapter-static`.
`routes/+layout.ts` sets `ssr = false` and `prerender = false`; the ESP32 serves
the resulting files and implements the REST/WebSocket backend in C++.

## Browser-only application

Use client routes and shared components. SvelteKit server endpoints and
`+page.server.ts` are not a replacement for firmware APIs in this deployment.
Keep browser-only globals inside browser-safe lifecycle code, especially for
modules that tooling may evaluate during builds.

The real UI can run against the simulator or a selected device through Vite's
proxy. Use `npm run dev:sim` for an isolated local instance, or set `DEVICE_HOST`
and run `npm run dev:device`. Production builds do not require a target host.
See [frontend development](frontend-testing.md).

## Changing the app name

Edit display metadata (`title`, `appName`, `copyright`) in
`interface/src/routes/+layout.ts`. Browser assets live under `interface/static/`;
keep display changes consistent with the shared layout and manifest.

Firmware 0.6.4 corrects the shared GitHub identifier to
`betamoojw/edge_switch_actuator`. Keep browser links separate from the
`owner/repository` value consumed by release APIs; see the
[source review](actuator-source-review.md).

Firmware identity comes from `APP_NAME`, `APP_VERSION` and `BUILD_TARGET` in
`platformio.ini`. These are distinct from display labels and the `edge_switch_actuator`
release-package filename prefix.

## Themes and localization

Users select preferences under **System → UI**. The supported themes are Light,
Dark, Nord, Dim and Sepia; Automatic follows the operating system for light/dark.
Seven languages are bundled locally. Preferences belong to the browser's device
origin and synchronize across tabs, including the login screen.

Maintain theme definitions in `app.css`, preference logic in
`lib/stores/preferences.ts`, and translations in `lib/i18n/translations.tsv`.
Regenerate messages with `npm run i18n:build`. Keep protocol values, addresses and
user-entered names out of translation. See [UI preferences](ui-preferences.md)
for the full workflow.

`lib/DaisyUiHelper.ts` and `lib/chart-preferences.ts` support chart colors and
preference changes. Use shared theme tokens for components instead of introducing
an unrelated hard-coded palette.

## Validation

From `interface/`, run `npm run test:i18n`, `npm run check`, `npm run build` and
`npm run test:bundle`. Use simulator and browser regressions for interaction
changes. Native and hardware tests remain necessary when a UI change also changes
firmware contracts or physical behavior.
