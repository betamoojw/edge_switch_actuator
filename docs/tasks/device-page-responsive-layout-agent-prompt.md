# Agent prompt: Device page responsive layout refinement

Review, analyze, and improve the responsive layout of the switching actuator device page after reproducing the reported visual issues. Preserve existing functionality and the application's established visual language. Treat the observations as symptoms to investigate rather than instructions to apply isolated CSS overrides.

## Observed issues

1. **Excessive section spacing:** The visual gap between the section navigation and the selected section's content is too large. This is most noticeable between **Outputs** and its channel content, and the same spacing should be reviewed for **Indicators**, **Button**, **Protocol**, **KNX**, and **Maintenance**.
2. **KNX on landscape mobile:** The KNX commissioning content adapts poorly to phone-sized landscape viewports. Long labels, controls, the communication-object table, channel parameters, alerts, and action buttons must remain readable and operable without page-level horizontal overflow.
3. **Section navigation on landscape mobile:** The navigation for **Outputs**, **Indicators**, **Button**, **Protocol**, **KNX**, and **Maintenance** does not use limited horizontal space effectively. Labels and icons must remain recognizable, touch targets must remain accessible, and navigation must not clip, overlap, or consume an excessive share of the viewport height.

## Objective

Create a compact, coherent, progressively responsive device page that works from narrow portrait phones through landscape phones, tablets, and desktop screens. Reduce unnecessary whitespace while retaining enough separation to communicate hierarchy. Use progressive disclosure or an alternate small-screen presentation where dense desktop content cannot remain usable through wrapping alone.

## Investigation

- Reproduce each issue in the running actuator simulator before editing. Inspect the page at representative portrait, landscape-phone, tablet, and desktop sizes.
- Identify the actual source of excess spacing, including the shared `SettingsCard`, status and alert regions, section navigation margin/padding, conditional placeholders, card-body gaps, and section-specific margins. Do not compensate for an unknown source with arbitrary negative margins.
- Check page-level and component-level overflow using both visual inspection and measured DOM dimensions. Distinguish intentional local scrolling from accidental page scrolling.
- Review all supported translations, especially longer German, Spanish, French, and Polish labels, before deciding navigation dimensions or truncation behavior.
- Preserve and work with existing uncommitted changes. Reuse established Svelte, Tailwind, daisyUI, icon, and component patterns.

## Implementation plan

1. Establish a consistent responsive spacing scale between the status area, section navigation, alerts, descriptions, and section content. Apply it uniformly across all six sections while allowing intentional exceptions for visible warnings or unsaved-settings feedback.
2. Refine the six-item section navigation for compact viewports. Choose an evidence-based responsive pattern such as a compact grid, horizontally scrollable tab list, or similarly accessible control. Keep all sections discoverable and preserve the active state, keyboard navigation semantics, icons, translated labels, and touch targets.
3. Rework KNX commissioning for narrow and short viewports. Prefer responsive structure over shrinking text. The object editor may use a mobile-specific stacked/card presentation or another accessible layout while retaining a table or denser layout where space permits. If local horizontal scrolling remains appropriate at an intermediate breakpoint, make it obvious, contained, and keyboard/touch usable.
4. Ensure KNX labels, addresses, validation messages, checkboxes, numeric fields, status text, and actions wrap or stack cleanly. Keep input widths bounded by their containers and place primary actions where they remain easy to reach and understand.
5. Verify every device section, not only Outputs and KNX, because the navigation and shared spacing changes affect the whole page. Avoid nested cards, duplicated controls, layout shifts during section changes, and breakpoint-specific dead space.
6. Add or update focused automated coverage for responsive navigation, section spacing, KNX layout, overflow, and translated content. Keep selectors based on roles and accessible names where practical.

## Constraints

- Do not change device APIs, simulator contracts, authorization, state management, command behavior, validation rules, or save/apply semantics.
- Do not hide required KNX information or controls merely to make the layout fit. Progressive disclosure must keep content discoverable and accessible.
- Do not reduce text or touch targets below accessible sizes, rely on hover, or truncate essential translated labels without an accessible full name.
- Avoid fixed widths that only fit one phone and avoid viewport-width font scaling. Prefer intrinsic sizing, wrapping, grid/flex breakpoints, and container-bounded overflow.
- Preserve focus, scroll position, unsaved edits, error messages, and the selected section during normal interaction and responsive changes.
- Maintain valid tab semantics and visible keyboard focus. Respect reduced-motion preferences if any navigation behavior is animated.

## Acceptance criteria

- The selected section begins with a deliberate, compact gap below the section navigation across all six sections; there is no unexplained blank band.
- At widths from 320 px upward, and specifically at landscape-phone viewports such as 667×375, 844×390, and 915×412, the document has no accidental horizontal overflow.
- The section navigation remains fully usable in portrait and landscape orientations. No icon or label is clipped or overlaps another item, the active section is clear, and each target is at least 44×44 CSS pixels where practical.
- KNX commissioning is readable and operable without zooming. Long translated text wraps, fields remain within their containers, validation remains associated with the correct field, and all communication objects and channel parameters remain available.
- Any intentional KNX subregion scrolling is confined to that subregion and does not cause the entire page to pan horizontally.
- Desktop and tablet layouts retain useful information density and do not regress into unnecessarily tall mobile-style stacks.
- Switching sections does not reset form state, trigger unrelated network commands, or introduce visible layout instability.

## Validation

1. Run the Svelte/type checks, relevant simulator tests, localization tests, and focused Playwright tests.
2. Exercise all six sections in Chromium at 320×568, 360×800, 390×844, 667×375, 844×390, 915×412, 768×1024, and a desktop viewport.
3. Repeat key checks with at least one long-label language and both supported light/dark themes where applicable.
4. Capture before-and-after screenshots for Outputs, the section navigation, and KNX at desktop, portrait-mobile, and landscape-mobile sizes.
5. Assert that `document.documentElement.scrollWidth <= document.documentElement.clientWidth` at each target viewport, and separately verify any intentional local overflow container.
6. Report the root causes, changed files, responsive decisions, test results, screenshot locations, and any remaining limitations. Clearly distinguish automated checks from visual inspection.