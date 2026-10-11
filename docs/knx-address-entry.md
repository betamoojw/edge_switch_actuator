# Switching Actuator startup and KNX address entry

Opening `/` redirects to `/device`, the Switching Actuator dashboard. Login still
protects the page when security is enabled. The welcome text, application title,
and navigation identity describe Switching Actuator. `/demo` remains available
only as an explicitly opened template development page.

The UI and application documentation use **Protocol Interface** for protocol
selection and access settings. Select KNXnet/IP under Protocol → Protocol Interface
selection and apply the profile before commissioning on the KNX tab.

The [live KNX screenshot](interface-tour.md#5-knx-commissioning) shows a configured
unit. Its fixed triple-click helper assumes factory bindings; inspect the Button
tab because saved bindings can differ. Applying commissioning or entering
programming mode is an active operation.

## Individual address

The placeholder is `15.15.255 (Factory Default)`. It is a hint, not a value that
overwrites the device's reported address. Persistent help also shows the factory
default when a configured value hides the placeholder.

Enter `area.line.device`: area and line 0–15, device 1–255. Device number 0 is
reserved for couplers and is rejected for this actuator, matching the firmware.
Malformed, missing and out-of-range values show an inline error as you type.
The [KNX Association describes 15.15.255 as an initial device address](https://support.knx.org/hc/en-us/articles/360011741860-Multiple-devices-with-the-same-Individual-Address).
Choose a unique address appropriate for the installed topology; browser validation
cannot detect another device using the same address.

## Group addresses

The placeholder is `1/0/1, 1/0/2`. This actuator accepts three-level notation:
main 0–31, middle 0–7, sub 0–255. `0/0/0` is reserved for broadcast and cannot
be assigned. See the [KNX Association address ranges](https://support.knx.org/hc/en-us/articles/115001825304-Group-Address-Ranges)
and [Schneider Electric three-level address specification](https://productinfo.se.com/spacelogic_knx_ihc/spacelogic-ihc-interface-sw-user-guide/English/BM_IHC_Interface_Plugin_DD01275013.XML.xml/%24/StandardKNXGroupAddresses-E75BFC63).

Separate addresses with commas; the first is the sending address. Blank means
no association. Each object accepts at most eight unique addresses. Duplicate
numeric addresses, malformed components, and empty comma-separated entries are
rejected. Two-level and free-level representations are not accepted by the
current firmware contract. Firmware also enforces global table limits and
commissioning ownership. Apply remains disabled while any address is invalid;
server-side validation remains authoritative.

## ETS download and web ownership: screenshot walkthrough

The following project-supplied screenshots were added on **11 October 2026**.
They illustrate ETS and the device's web interface; their capture date, exact
firmware build and ETS version were not supplied. No download or configuration
change was performed to prepare this documentation. Open each image for its
full-resolution view. Addresses shown belong to the example installation and
are not defaults to copy into another project.

### Before the ownership change

Private network addresses and unrelated browser bookmarks in the three KNX images
are covered with opaque masks. Commissioning controls and results are otherwise
unchanged. KNX individual/group addresses remain illustrative protocol values.

[![ETS download operation beside the actuator KNX page, which shows programming ON, web ownership, revision 2 and individual address 1.2.1.](with_ets_download_before.png)](with_ets_download_before.png)

The image named `with_ets_download_before.png` already shows an active ETS
download/restart operation. On the right, the web page reports **Programming ON**
and **Owner: web · Revision 2 · Ready**. The ETS group-object list contains the
six channels' Switch, Block and Status objects. The individual address is
`1.2.1`; visible group associations include `8/0/0` for channel 1 Switch and
`8/1/0` for its Status.

Treat this as the state before the illustrated ownership change, not an idle
pre-download screen. Do not edit web commissioning while ETS is downloading.

### After the ownership change

[![ETS application download beside the actuator KNX page now reporting ETS ownership and revision 3, with individual address 1.2.1.](with_ets_download_after.png)](with_ets_download_after.png)

In `with_ets_download_after.png`, the web page reports **Owner: ets · Revision 3 ·
Ready**. The visible individual address and channel 1 associations remain the
same. This illustrates the change from web ownership to ETS ownership and a new
KNX revision.

ETS still displays **Downloading** in the supplied image. The filename and web
Ready label alone do not establish a completed, successful ETS download. Wait
for ETS's final operation result, then reload the device's KNX snapshot and
compare addresses, associations and application parameters with the intended
project. Retain the ETS result and firmware identity for acceptance records.

### Taking over for web editing

[![KNX page with Take over for web editing selected, owner still ETS at revision 3, and an HTTP 401 warning stating that controls are disabled.](knx_config_web_edit.png)](knx_config_web_edit.png)

The checkbox requests explicit takeover when saving an ETS-owned configuration;
selecting it alone does not transfer ownership. A later ETS download can replace
web changes, so keep the installation's ETS project and local edits coordinated.

This screenshot also shows **Connection unavailable. Controls are disabled.
Error: Request failed (401)**. It is an authentication-error example, not proof
of a successful web save. Sign in again and reload the current KNX snapshot
before continuing; values left on a disconnected page may be stale.

For an authorized installer or administrator, the source-derived workflow is:

1. Finish the ETS operation and ensure KNX is active with no download in progress.
2. Load the current snapshot and review its owner and KNX revision.
3. Select **Take over for web editing**, make the intended valid edits and choose
   **Apply KNX commissioning**. Applying changes is an active commissioning action.
4. Verify the successful response and read back the committed configuration and
   web ownership. On a revision conflict, reload and reconcile the changes.

The firmware rejects web saves during an ETS download and requires explicit
takeover for ETS-owned settings. See [REST revisions](restfulapi.md#revisions-and-retries)
and [commissioning acceptance](commissioning.md) for validation boundaries.
These screenshots do not establish relay operation, full ETS interoperability
or KNX certification.

## Historical browser validation

Browser regression cases `HOME-01` and `KNX-04` cover startup, placeholders,
invalid boundaries, reserved and duplicate addresses, and saving valid extremes.

Validation for this update: all 39 production-build Chromium scenarios passed,
and both focused cases passed in development mode on Chromium, WebKit, and mobile
Chromium (six checks). Svelte/TypeScript reported zero errors and warnings;
production build, simulator-marker exclusion, and changed-code formatting passed.
Physical KNX commissioning was not exercised by these browser tests.
