# Switching Actuator startup and KNX address entry

Opening `/` redirects to `/device`, the Switching Actuator dashboard. Login still
protects the page when security is enabled. The welcome text, application title,
and navigation identity describe Switching Actuator. `/demo` remains available
only as an explicitly opened template development page.

The UI and application documentation use **Protocol Interface** for protocol
selection and access settings. Select KNXnet/IP under Modbus → Protocol Interface
selection and apply the profile before commissioning on the KNX tab.

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

Browser regression cases `HOME-01` and `KNX-04` cover startup, placeholders,
invalid boundaries, reserved and duplicate addresses, and saving valid extremes.

Validation for this update: all 39 production-build Chromium scenarios passed,
and both focused cases passed in development mode on Chromium, WebKit, and mobile
Chromium (six checks). Svelte/TypeScript reported zero errors and warnings;
production build, simulator-marker exclusion, and changed-code formatting passed.
Physical KNX commissioning was not exercised by these browser tests.
