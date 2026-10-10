# Saved MCP endpoint reveal — 2026-10-10

## Cause

The settings API intentionally redacted the saved credential. The UI reset its
endpoint input to an empty string after load/save; Show endpoint only switched
that empty input from password to text. It could reveal a newly typed draft,
but had no way to retrieve the saved endpoint.

## Change

Added administrator-only `POST /rest/xiaozhiMcpEndpoint` with a current-revision
request and `Cache-Control: no-store` response. Anonymous callers receive 401,
non-admin callers 403, invalid requests 422 and stale revisions 409. The API
copies the endpoint under the settings mutex and sends after releasing it;
revealing performs no persistence, generation increment or reconnection.

Checking Show endpoint fetches the saved credential only if the input is empty
and a saved endpoint exists. An existing draft is simply unmasked. Retrieved
values are tracked separately from edits, so revealing does not make the form
dirty and subsequent unrelated saves omit the unchanged endpoint. Hiding,
reloading, successful saving, hiding the tab and navigation clear the retrieved
value. Edited drafts are retained and masked on hide. Stale asynchronous
responses cannot reveal the value after the checkbox is unchecked.

Ordinary settings/status/tool responses stay redacted. The revealed credential
is not written to local storage, logs or documentation. It is displayed to the
authenticated administrator explicitly requesting it.

## Software validation

- Svelte/TypeScript: 0 errors, 0 warnings.
- Three MCP simulator tests passed, including explicit reveal, authorization,
  stale revisions, no-store headers, redacted ordinary reads, unchanged
  settings and empty response after endpoint removal.
- Three Chromium MCP regression cases passed. Added saved-value reveal/hide,
  reload clearing, no false dirty state, enabled reconnect, denied reveal and
  delayed-response cancellation checks. Existing save/removal, viewer and
  failed-save draft tests still pass.

Tests use synthetic endpoint tokens. No physical relay commands are needed to
validate this change.

## Device deployment and validation

Built and deployed firmware 0.6.3 to `192.168.71.28`
(`CC:BA:97:34:D9:AC`). Application image: 2,162,160 bytes; SHA-256:
`68cf767b7dea3885e0e1b2ad269da70121d8cf2ea06120e011970117aed3a9f4`.
OTA returned HTTP 200, and all 11 embedded frontend assets fetched directly
from the board matched the build.

Live API checks passed: saved endpoint reveal, no-store response, anonymous
rejection, invalid/stale revision rejection, redacted ordinary reads, unchanged
MCP settings, no MCP reconnect and no reboot during reveal. MCP remained ready;
HTTP stack minimum free space was 4,956 bytes. Endpoint values were neither
printed nor persisted in validation records.

The in-app browser still used its previously cached frontend on the mDNS
origin. Its Show endpoint checkbox continued to leave the input empty. Thus
live visual verification of the new UI is **not complete**; clear the site's
cached assets or perform a cache-bypassing reload before retesting. Automated
Chromium tests above exercised the new UI successfully.

## Separate post-OTA finding: KNX configuration

The general device configuration survived unchanged and device fault remained
zero, but the KNX configuration did not survive the OTA reboot. The snapshot
changed from configured, address `1.2.1`, revision 26 and populated group
mappings to unconfigured, address `15.15.255`, revision 1 and empty mappings.
Both snapshots report ETS ownership. The protocol mode remains `knx_ip` and
all six relay outputs are off. Overall deployment validation is therefore
not a full pass, despite successful endpoint validation.

Before/after snapshots are retained locally under
`.pio/ota-mcp-endpoint-reveal/`. No KNX configuration write was performed by
the OTA or endpoint validators. Root cause of this separate persistence issue
has not been established. Restoring the snapshot via the web API would change
ownership to web.

### Recovery follow-up

The user authorized restoration through the web API. A revision-checked POST
with explicit takeover was submitted using the saved address, 18 object
records and six parameter records. The request timed out; it was not blindly
resubmitted. Readback then found the device had rebooted and recovered the
original configuration: address `1.2.1`, populated mappings and parameters
matching the saved snapshot, configured true, revision 26, owner **ETS**.
Therefore successful completion of the web write/takeover is not established.
The device appears to have recovered its previous persisted configuration on
restart; the earlier unconfigured readback does not establish durable data loss.

KNX reports running, fault zero, and all six relays remain off. The recovered
configuration was checked again across a ten-second observation interval.
No further configuration write or intentional reboot was performed. The
unexpected reboot and intermittent configuration recovery require separate
root-cause investigation; this recovery does not establish a persistence fix.
Readback evidence is stored in `knx-restored.json` and
`knx-recovery-validation.json` under the local deployment-record directory.
