# Device page visual refinement

## Request

Improve the device page layout and add appropriate icons while matching the application's existing design style.

## Implementation

- Reuse SettingsCard, daisyUI theme colors, rounded panels, and primary hexagonal Tabler icons.
- Add network, protocol, and localized uptime summaries above six responsive icon tabs.
- Refine relay cards with status badges, action icons, and expandable channel settings.
- Give indicator, button, Modbus, KNX, and maintenance sections consistent headings, spacing, and form layouts.
- Wrap translated labels and buttons on narrow screens; keep the KNX table horizontally scrollable within its panel.
- Preserve existing API requests, permissions, validation, and control behavior.

## Validation

- Svelte check: zero errors and warnings.
- Production build: passed (existing single-bundle size warning remains).
- Translation catalogue checks: 2 passed.
- Browser regression: 90 passed (45 desktop Chromium, 45 mobile Chromium).
- Layout coverage: all six sections in English/light and German/dim, with no page overflow; desktop and mobile screenshots visually inspected.

