# Frontend structure

The production interface lives in `interface/src/` and uses Svelte 5, SvelteKit
static output, Tailwind CSS 4 and DaisyUI 5. The browser talks to the firmware
through `/rest` and `/ws`; there is no server-side Svelte application on the ESP32.

## Routes and modules

| Location under `interface/src/` | Purpose |
| --- | --- |
| `routes/+layout.ts`, `+layout.svelte` | Feature loading, product metadata, login and shared shell |
| `routes/+page.ts`, `+page.svelte` | Root entry; opens `/device` |
| `routes/device/` | Outputs, indicators, button, protocol, KNX and maintenance |
| `routes/connections/` | MQTT, NTP and Xiaozhi MCP |
| `routes/wifi/`, `routes/ethernet/` | Network status/settings; Ethernet feature gated |
| `routes/user/` | Administrator user management |
| `routes/system/` | UI preferences, status, metrics, core dump and firmware updates |
| `routes/demo/` | Optional template demo; not the actuator startup screen |
| `routes/menu.svelte`, `statusbar.svelte`, `login.svelte` | Navigation, status bar and authentication view |
| `lib/device/types.ts`, `reconcile.ts`, `knx-address.ts` | Actuator contracts, partial-state merging and KNX validation |
| `lib/stores/` | User, socket, preferences and telemetry state |
| `lib/i18n/` | Translation source, generated messages and reactive translation API |
| `lib/components/` | Reusable controls, settings cards, dialogs and notifications |
| `lib/types/models.ts` | Shared framework models |

## Features and permissions

The layout loads `/rest/features` into `page.data.features`. Menu entries and
screens use these flags for compiled capabilities. Actuator status/configuration
also returns per-user capabilities. UI visibility is not authorization: firmware
REST handlers enforce roles and channel masks before mutations.

The layout metadata uses `github: 'betamoojw/edge_switch_actuator'` in 0.6.4.
Release components require this `owner/repository` identifier; the sidebar uses
a separate documentation-site URL. Asset selection remains broad; see
[firmware updates](buildprocess.md#updating-a-device).

## State and interaction

The browser receives MessagePack events and makes REST mutations. Keep saved
configuration, editable drafts and incoming status separate; background telemetry
must not erase an unsaved form. KNX has an independent configuration revision and
returns a committed snapshot after save. Preserve disabled/blocked/channel-mask
semantics in every control, including bulk commands.

`lib/device/reconcile.ts` merges partial updates while preserving existing nested
values. `lib/stores/socket.ts` owns subscriptions, heartbeat, reconnect and stale
socket handling. Never introduce simulator-specific branches into production UI
code; the external simulator serves the same REST/event contracts.

## Customize the main menu

Add route metadata in `+page.ts` and a menu entry in `routes/menu.svelte`. Titles
are translation keys and must match the page title used for active navigation.
Use the appropriate feature and permission checks. New components should use
shared settings/dialog patterns and `$t()` at display boundaries.

For translations, themes and browser preference persistence, see
[UI preferences](ui-preferences.md) and [SvelteKit and theming](sveltekit.md).
For local proxy mode, simulator fixtures and browser tests, see
[frontend testing](frontend-testing.md).
