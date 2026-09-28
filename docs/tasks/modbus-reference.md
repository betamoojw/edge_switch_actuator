# Modbus reference in the device Protocol tab

## Request

Show the device Modbus function map when the protocol dropdown selects Modbus RTU or Modbus TCP, using `docs/actuator-modbus-map.md` as the source.

## Implementation

- Show the reference immediately for the selected RTU/TCP mode; hide it for Off and KNX.
- Explain that selecting a mode does not activate it until settings are applied.
- Provide translated function descriptions, addressing/encoding guidance, and commissioning rules.
- Show FC08 return-query diagnostics only for RTU.
- Render all four address-space tables directly from the versioned Markdown contract; preserve reserved offsets and access restrictions exactly.
- Label the detailed source tables as English. Include the full original contract and an offline Markdown download for command enums, exceptions, transport details, and other notes.
- Bundle the source locally; no network documentation dependency or HTML injection.
- Use existing theme colors, Tabler icons, expandable sections, and contained table scrolling.

## Verification

Svelte check: zero errors/warnings. Translation catalogue: 2 checks passed. Targeted desktop/mobile browser tests: 6 passed. Production build: passed with the existing single-bundle size warning. Desktop table layout visually inspected; mobile page overflow and offline download verified by browser tests.

