# Modbus TCP relay FAT/SAT test plan

## 1. Purpose and standards basis

This procedure defines a repeatable factory/site acceptance test for the six
relay outputs of the Edge S3 Relay 6CH using Modbus TCP.

It is **aligned with** the acceptance-test approach in IEC 62381, *Automation
systems in the process industry — Factory acceptance test (FAT), site
acceptance test (SAT), and site integration test (SIT)*, and uses the Modbus
Application Protocol Specification V1.1b3 for wire-level function and
exception behavior. This is a project test procedure, not an IEC or Modbus
Organization conformance certificate, a safety assessment, or an electrical
installation approval. IEC 62381 provides a process-industry acceptance
framework; it does not prescribe this product's relay timing or test values.

References:

- IEC 62381, *Automation systems in the process industry — Factory acceptance test (FAT), site acceptance test (SAT), and site integration test (SIT)*.
- [Modbus specifications](https://www.modbus.org/specs.php)
- [Modbus Application Protocol Specification V1.1b3 (PDF)](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf)
- [Actuator Modbus map](actuator-modbus-map.md)

## 2. Scope and limitations

The procedure checks TCP reachability, device identity, map version, selected
transport state/configuration, safe initial state, FC05 write echoes, FC01
coil state, FC02 applied-output state, single-channel exclusivity, a 30-second
ON hold per channel, short repeated toggles, an OFF dwell, restoration to OFF,
and the final all-OFF state. An optional device-I/O case exercises the Modbus
identify mailbox (five-second white RGB indication) and observes one
operator-generated physical double-click through the button status and gesture
counter.

The firmware's discrete-input values report commanded/applied software
outputs; they are **not contact feedback**. Physical contacts must be
independently observed with an appropriately rated meter, isolated indicator,
or data logger when physical operation is part of the acceptance claim. This
host tool alone cannot certify that a relay contact changed.

The test is serial, one channel at a time. Do not use mains loads for
commissioning. Test timing, switching lifetime, dielectric strength,
temperature rise, EMC, functional safety, process safety, and full formal
protocol conformance are out of scope.

## 3. Roles, equipment, and preconditions

The operator controls the authenticated device session and has authority to
energize the test load. A second observer is recommended for contact
instrumentation and emergency-stop monitoring.

Required:

- Production-representative six-channel board and identified firmware.
- Host connected to the intended dedicated test network.
- Six isolated, low-voltage test loads or safe indicators appropriate to the
  board output ratings; no mains loads.
- Independent contact-state observation if physical relay switching is being
  accepted.
- An accessible emergency disconnect and a person monitoring the complete
  run.
- For device-I/O testing, direct visual access to the board's RGB LED and an
  authenticated installer session to enable **Allow manual indicators and
  diagnostic commands** for the duration of the test. This permission is
  separate from relay access.
- `scripts/verify_modbus.py` and a writable evidence destination.

Before starting:

1. Record date/time (with timezone), tester/observer, device model/serial and
   board revision, firmware version/hash, test network, host source IPv4,
   test-load/instrument details, and evidence location.
2. Check wiring, ratings, protective measures, and the emergency disconnect.
   Confirm all test loads are safe to energize for the entire test.
3. In the device UI, verify **Modbus TCP / running**, address, TCP port, unit
   ID, channel enablement, and the Modbus write mask. Confirm the host is
   allowed by the source policy.
4. Disable unrelated automation/masters that could change relay states.
5. Independently confirm every contact/load is OFF. The verifier also requires
   all six coil and output reads OFF before writing.
6. Stop if any channel is already ON, protocol checks fail, the physical state
   is uncertain, or the emergency stop is unavailable. Do not continue by
   automatically normalizing an unknown output state.
7. For the optional RGB/button case, confirm RGB and button application
   actions are enabled, the double-click binding is **identify** (action 6),
   manual RGB brightness is nonzero, no active fault can mask the LED, and all
   relay coils/outputs are OFF. Do not change persistent button bindings for
   this test. The physical button is not remotely injectable.

## 4. Test steps and acceptance criteria

| Case | Test | Expected / acceptance |
| --- | --- | --- |
| TCP-REL-01 | Connect to configured IPv4/port and unit; read identity, map, capabilities, transport status and configuration; exercise documented negative checks | All baseline checks pass and target identity/settings match the recorded device |
| TCP-REL-02 | Read all six FC01 coils and FC02 outputs before actuation | Every value is OFF; otherwise stop with no relay writes |
| TCP-REL-03 | For CH1 through CH6, perform three 1 s ON / 1 s OFF cycles | FC05 echoes; after every edge and interval, only the selected channel is ON or all channels are OFF as expected |
| TCP-REL-04 | Hold each selected channel ON for 30 seconds, then command OFF and hold OFF for 5 seconds | Selected coil/output remains ON during the ON hold; all six remain OFF during and after the OFF dwell |
| TCP-REL-05 | Verify all channels after each step | FC01 and FC02 match expected state; no non-selected channel changes |
| TCP-REL-06 | Read all six states after the final channel | All coils and outputs are OFF; independently observed physical contacts are OFF |
| TCP-REL-07 | Review evidence and discrepancies | Every case has an actual result and pass/fail; any communication, output, contact, safety, or restoration failure is recorded and the test is failed/aborted |
| TCP-IO-01 | Use FC16 mailbox opcode 1 (identify), then read command sequence/result and FC04 RGB/reason plus FC02 RGB-active state | Command is applied; RGB registers show the configured nonblack white output with manual-indicator reason; all relay states remain unchanged |
| TCP-IO-02 | Operator briefly double-clicks the physical BOOT button while the runner polls FC02 pressed state and FC04 gesture count/last gesture | Exactly one double-click is recorded, and its configured identify action produces the same five-second white RGB register output |
| TCP-IO-03 | Observer watches the actual LED during each identify indication | Record visibly observed white illumination separately; register values do not prove optical output |

Acceptance of a software read-back does not substitute for contact observation.
Record actual contact changes separately, including instrument identity and
the observed OFF/ON/OFF result for every channel.

The optional device-I/O case does not command any relay. The runner refuses to
start it unless all relay coils and applied outputs are OFF, and verifies they
remain unchanged. The Modbus identify opcode is sent as one FC16 write to
0x0200–0x0203; if the installer permission is absent, the test is denied and
the permission must be enabled in the device UI rather than bypassed. The
button check waits for one brief double-click and verifies gesture counter
increment and identify mapping. Never hold the BOOT button; reset-hold testing
is explicitly out of scope. Avoid single/triple-click testing because those
bindings are not part of this acceptance case.

## 5. Abort and recovery

Abort on an unexpected channel change, lost/contradictory read-back,
communication loss, unsafe load behavior, operator concern, or emergency
condition. The runner stops at the first failed channel and attempts to write
the channel under test OFF in a `finally` path. It reports restoration
failures explicitly. This best-effort command is not guaranteed if power,
network, firmware, or the output driver has failed.

On abort, use the independent emergency disconnect as required, verify
physical outputs without relying only on the host report, and recover using
the device's approved procedure. Do not resume from a partial run without
investigating the cause and recording a new test run.

## 6. Runner and evidence

The all-channel runner requires `--confirm-safe-loads`, performs the complete
read-only baseline first, and fails closed if a baseline check or all-OFF
preflight fails. It defaults to three 1 s ON / 1 s OFF toggle cycles, a 30 s ON
dwell, and a 5 s OFF dwell per channel. It turns on only one coil at a time,
checks all coils and outputs, and restores that channel OFF before continuing.
Durations must be greater than zero and at most 3600 seconds; cycle count must
be 1–100.

Example (from the repository root; use a new report filename for each run):

```powershell
py scripts/verify_modbus.py --transport tcp `
  --host 192.168.1.111 --port 502 --unit 1 --timeout 5 `
  --exercise-all-relays --toggle-cycles 3 `
  --toggle-on-seconds 1 --toggle-off-seconds 1 `
  --on-seconds 30 --off-seconds 5 --confirm-safe-loads `
  --output evidence/modbus-tcp-relay-fat-sat.json
```

Run the optional RGB/button acceptance after confirming the installer
indicator permission and that an observer can see the device LED:

```powershell
py scripts/verify_modbus.py --transport tcp `
  --host 192.168.71.48 --port 502 --unit 10 --timeout 5 `
  --exercise-device-io --confirm-indicator-observation `
  --button-timeout-seconds 60 `
  --output evidence/modbus-tcp-device-io-fat-sat.json
```

Keep the machine-readable JSON together with the completed physical-observation
record, firmware/build identity, UI configuration snapshot, network capture if
used, and tester/observer sign-off. Redact credentials and tokens. Retain
failed/incomplete reports; do not replace them with a later successful report.

The runner's `safeLoadsConfirmed` value records an operator declaration; it is
not an independent load sensor or interlock. Exit code `0` means the requested
protocol checks passed, `1` means a device check failed, and `2` means the
request/setup was invalid.
