# Contributing

Start with [architecture](architecture.md) and the [current source review](actuator-source-review.md).
The product target is `waveshare-relay-6ch`; generic framework boards are useful
regressions but do not prove six-channel hardware behavior.

1. Base your change on current `dev` and preserve unrelated work. Describe one
   concrete problem and the behavior that will change.
2. Use the [simulator](frontend-testing.md) for UI work, then inspect the relevant
   source contract. Do not require a physical relay operation to verify layout.
3. Keep relay mutations on the actuator task. Preserve enabled/blocked checks,
   transport permissions, revision handling and bounded retry behavior.
4. Run relevant checks from [validation](validation.md#reproduce-software-checks).
   Firmware changes need the affected PlatformIO builds and actual image-size
   checks; frontend changes need type, simulator and appropriate browser checks.
5. Update user guidance and API examples when behavior changes. Mark whether
   evidence is source-derived, simulated, historical or measured on hardware.
6. Submit a focused PR with trigger, before/after behavior, test results, remaining
   risks and screenshots where useful. Never commit credentials, raw setup labels,
   private endpoint URLs or generated firmware unrelated to the change.

For documentation changes, follow [publishing](documentation.md), run strict
MkDocs validation and inspect both desktop and mobile pages. Include meaningful
alt text and captions; an attractive screenshot is not a test result. Preserve
the project's [upstream attribution](index.md#project-and-license) and license
notices. Historical design records should retain their original context rather
than silently becoming current operating instructions.
