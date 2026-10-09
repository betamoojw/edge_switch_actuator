# Agent prompt: Switching actuator marketing demo video

Use the local interface simulator to produce a short, polished marketing demonstration of the Edge Switching Actuator web interface. Record the real application UI and simulator-backed interactions; do not fabricate physical-device footage, measured electrical behavior, certifications, or production readiness.

## Objective

Create a concise product video suitable for a website, social post, or sales presentation. The video should communicate six-channel control, clear live state, responsive interaction, indicator controls, button actions, protocol flexibility, KNX commissioning, and maintenance without becoming a tutorial.

## Creative direction

- Record at 1280×720 with the application in English and its dark theme.
- Use a restrained opening title and closing message over the live interface.
- Keep a small, persistent **Simulator demo** disclosure visible.
- Use deliberate pacing, a visible animated pointer, and smooth scrolling so interactions are easy to follow.
- Show the device overview, individual and bulk output switching, Indicators, Button, Protocol, KNX, and Maintenance. Return outputs and temporary runtime controls to their safe state before ending on a clean product view.
- Avoid login screens, passwords, browser chrome, debug controls, test reports, notifications, errors, unsaved settings, and destructive actions.
- Do not add audio unless licensed source material and attribution requirements are explicitly provided.

## Implementation requirements

1. Add a reproducible Playwright-based recording script under `interface/scripts/` and expose it through an npm script.
2. Authenticate in a non-recorded browser context, then create a fresh recorded context from its storage state.
3. Drive the existing simulator and application through accessible roles and labels. Do not modify application behavior solely for the video.
4. Preserve simulator state by ending with all enabled outputs OFF and avoid saving configuration changes.
5. Save recording artifacts under `interface/test-results/marketing/` by default. Produce WebM directly and MP4 when the available FFmpeg supports conversion.
6. Make the base URL, output directory, output filename, and optional simulator control endpoint configurable through environment variables while retaining useful local defaults.

## Validation

- Confirm the simulator health endpoint is ready before recording.
- Verify the final video exists, has nonzero duration, is 1280×720, and contains changing frames rather than a blank or static canvas.
- Review representative opening, interaction, protocol, KNX, and closing frames visually.
- Report artifact paths, duration, dimensions, file sizes, format availability, and any conversion limitation.
