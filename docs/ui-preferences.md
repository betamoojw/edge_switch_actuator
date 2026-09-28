# Browser interface preferences

Open **System → UI** to select a language and theme. Changes take effect
immediately throughout the interface and are saved for this device's origin in
the current browser, including the login screen. Open tabs synchronize changes.
These are browser preferences, not firmware settings or user-account settings.

Languages: English, Deutsch, Español, Français, Polski, 简体中文 and 繁體中文.
Translations are bundled with the firmware frontend and work without internet
access. Protocol names, addresses, user-entered names and raw device diagnostics
retain their original values.

Themes: Light, Dark, Nord, Dim and Sepia (daisyUI Retro). Automatic uses Light
unless the operating system requests Dark. Choosing an explicit theme overrides
the operating-system preference. Nord offers cool colors, Dim a subdued dark
palette, and Sepia a warm light palette.

## Maintaining translations

`interface/src/lib/i18n/translations.tsv` contains an English source key followed
by German, Spanish, French, Polish, Simplified Chinese and Traditional Chinese,
separated by tabs. Use industrial automation terminology. Keep whole sentences
and named parameters such as `{name}` together so each language can reorder them.
Protocol and configuration values must never be translated before API submission.

Run `npm run i18n:build` in `interface/` to regenerate the bundled `messages.json`.
Use the reactive `$t()` store in Svelte at the display boundary; keep internal
identifiers stable. Use `locale` for `Intl` date/number formatting and `duration`
for localized time units. Shared dialogs and notifications translate message
keys and render text safely. Unknown device diagnostics fall back to their
original text.

Run `npm run test:i18n`, `npm run check`, `npm run build`, and the simulator
Playwright suite. `UI-01` through `UI-05` cover preferences, localization,
protocol-value preservation, mobile layout and cross-tab synchronization.
