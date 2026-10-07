# Agent prompt: Device UI updates and connection stability

Review and improve the Waveshare ESP32-S3-Relay-6CH firmware and its device web interface. Preserve existing work and settings, and build on any fixes already present. Save this prompt before making implementation changes.

## Reported issues

1. **Outputs:** Clicking one channel's toggle appears to refresh every channel's display. Update only the affected channel's state and controls. Keep unrelated channel cards, focus, scroll position, expanded settings, and unsaved edits stable.
2. **Indicators:** Clicking a control in either the RGB status indicator or Buzzer panel appears to refresh both panels. Update only the affected values and pending/error feedback. Preserve the other panel's state, input values, focus, and layout.
3. **Connection stability:** The web client repeatedly displays “Connection to device lost.” Reproduce and diagnose the actual connection lifecycle, including heartbeat timing, HTTP/WebSocket contention, reconnect handlers, duplicate connections, subscription cleanup, and notification behavior. Correct the underlying causes; do not merely hide the warning or disable genuine disconnect detection.

## Required feature

Add **All enabled channels ON** beside the existing **All enabled channels OFF** control in Outputs. Follow the existing authorization, enabled-channel, block/interlock, and error-handling rules. Validate the whole selection before changing outputs so a rejected bulk command cannot partially execute. Retain a correct existing implementation if this feature is already present.

## Implementation requirements

- Inspect the frontend, firmware APIs, event transport, and existing tests before deciding on a fix. Distinguish DOM replacement, reactive updates, layout shifts, and network reconnects with evidence.
- Use targeted reactive updates and preserve unchanged object/DOM identity. Avoid full-page reloads, whole-panel remounts, global busy effects for unrelated controls, and redundant full-status requests after individual commands.
- Keep device-reported state authoritative. Handle failed commands, slow responses, stale responses, external state changes, and disconnections without displaying an unconfirmed command as successful.
- Keep live updates working while a REST request is pending. Retain permission checks and prevent stale REST responses from overwriting newer event state.
- Keep one intended WebSocket connection, use bounded reconnect backoff, clean up timers/listeners, and report genuine connection loss without repeated notification spam for the same outage.
- Preserve existing localization and simulator contracts. Add focused regression tests for the reported failures and bulk-command authorization/atomicity.

## Validation and deployment

1. Run the relevant frontend, simulator, and firmware checks, then build the embedded web interface and the `waveshare-relay-6ch` firmware. Resolve failures before deployment.
2. Only after the checks and build pass, upload the matching application firmware through OTA to the device. The last known address is `http://192.168.71.28`; verify reachability and device identity before uploading.
3. Verify the running firmware and served UI after reboot, confirm saved configuration is preserved, and test the live interface for targeted updates and connection stability. Clearly distinguish simulator results from physical hardware results.
4. If OTA recovery or hardware access fails, diagnose it and report the exact remaining limitation. Do not claim successful verification solely because the upload returned success.
5. Summarize root causes, changes, test evidence, firmware version, OTA outcome, and any remaining limitations. Provide the path to this saved prompt and the validation results.
