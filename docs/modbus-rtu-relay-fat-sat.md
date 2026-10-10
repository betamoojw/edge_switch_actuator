# Modbus RTU relay FAT/SAT test plan

## 1. Purpose and standards basis

This procedure defines a repeatable factory/site acceptance test for all six
Edge S3 Relay 6CH relay outputs through the RS-485 Modbus RTU interface.

The acceptance workflow is aligned with IEC 62381, *Automation systems in the
process industry — Factory acceptance test (FAT), site acceptance test (SAT),
and site integration test (SIT)*. Wire framing, serial master/slave operation,
CRC, and RTU timing are checked against the Modbus Serial Line Protocol and
Implementation Guide V1.02; object functions/exceptions use the Modbus
Application Protocol Specification V1.1b3. The test values below are project
FAT/SAT criteria, not durations prescribed by those standards. This is not an
IEC/Modbus conformance certificate, relay endurance qualification, safety
assessment, or electrical installation approval.

References:

- IEC 62381, *Automation systems in the process industry — Factory acceptance test (FAT), site acceptance test (SAT), and site integration test (SIT)*.
- [Modbus Organization specifications](https://www.modbus.org/modbus-specifications)
- [Modbus Serial Line Protocol and Implementation Guide V1.02 (PDF)](https://www.modbus.org/file/secure/modbusoverserial.pdf)
- [Modbus Application Protocol Specification V1.1b3 (PDF)](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf)
- [Actuator Modbus map](actuator-modbus-map.md)

## 2. Scope and limits

The host test covers COM-port use, RTU CRC and unit response, product
identity/map/protocol/profile, RS-485 readiness, FC08 subfunction 0 echo,
expected invalid-address/value exceptions, initial safe state, serial relay
switching, sequential channel isolation, repeated toggle cycles, timed ON/OFF
intervals, recovery to OFF, final state, and the device's RTU frame-error
counter. The optional device-I/O case exercises the Modbus identify mailbox
(five-second white RGB indication) and observes one operator-generated
physical double-click through the button status and gesture counter.

The device's FC02 discrete inputs report applied commanded output state, not
measured electrical contact position. For physical-output acceptance,
independently observe each contact with a suitably rated meter, isolated
indicator, or data logger and record instrument identity/results. Software
read-back alone does not prove contact movement.

This is a functional acceptance run, not a high-cycle endurance test. The
chosen short toggle count is intentionally bounded to limit mechanical wear.
Electrical life, contact bounce, coil/contact temperature, switching capacity,
insulation, EMC, process safety, malformed-frame timing, broadcast silence,
and every available baud profile require separate qualification.

## 3. Roles, equipment, and preconditions

The operator must have authority to energize the test loads. Keep an observer
at the test fixture/emergency disconnect for the complete run.

Required:

- Production-representative Edge S3 Relay 6CH and identified firmware.
- Host USB-to-RS485 adapter on **COM22**; no other program may hold the port.
- Isolated RS-485 connection with correctly identified A/B, reference where
  required, and suitable bus termination at the physical ends.
- Six isolated low-voltage test loads/indicators within the relay ratings; do
  not use mains loads for this commissioning procedure.
- Accessible independent emergency disconnect and, when physical acceptance
  is required, contact-state measurement for each channel.
- For device-I/O testing, direct visual access to the board's RGB LED and an
  authenticated installer session to enable **Allow manual indicators and
  diagnostic commands** for the duration of the test. This permission is
  separate from relay access.
- `scripts/verify_modbus.py` and a unique writable JSON evidence path.

Device profile verified for this run:

| Setting | Required value |
| --- | --- |
| Protocol interface | Modbus RTU, running |
| RS-485 | Enabled and initialized |
| Unit address | 10 |
| Serial profile | 19200 baud, 8 data bits, even parity, 1 stop bit (8E1) |
| Host adapter | COM22 |
| Modbus relay write mask | 63 (all channels) |

Before actuation:

1. Record date/time with timezone, operator/observer, board model/serial/revision,
   firmware/version/hash, device profile, COM adapter identity, test-load and
   meter identities, topology, and report path.
2. Check wiring, isolation, protective measures, and emergency disconnect.
   Confirm test loads are safe for repeated switching and the longest ON dwell.
3. Use one master only. Disable unrelated polling/writes, close serial
   monitors, and confirm COM22 belongs to the attached USB-to-RS485 adapter.
4. Verify the device UI shows RTU running, RS-485 enabled, unit 10, 19200 8E1,
   all intended channels enabled, and a write mask covering those channels.
5. Independently confirm all contacts/loads are OFF. The host also performs
   an FC01/FC02 all-OFF preflight and refuses to start otherwise.
6. Stop on any uncertain wiring/state, unavailable emergency disconnect,
   unexpected device profile, CRC/timeout/exception, or other master present.
   Never normalize an unknown or mismatched coil/output state automatically.
   If a known, matching initial ON state must be made safe, obtain explicit
   operator authorization and specify every channel with
   `--prepare-off-channel`; the report records which channels were turned OFF
   before exercising them.
7. For the optional RGB/button case, confirm RGB and button application
   actions are enabled, the double-click binding is **identify** (action 6),
   manual RGB brightness is nonzero, no active fault can mask the LED, and all
   relay coils/outputs are OFF. Do not change persistent button bindings for
   this test. The physical button is not remotely injectable.

## 4. Test matrix and acceptance

| Case | Test | Expected / acceptance |
| --- | --- | --- |
| RTU-REL-01 | Open COM22 and poll the configured unit/profile | Device responds at unit 10 with CRC-valid RTU frames; device profile matches host settings |
| RTU-REL-02 | Read identity, map, capabilities, effective protocol, RS-485 state/readiness and counters; run FC08 echo and negative address/value checks | Product/map correct, RTU effective mode and UART match, FC08 echo matches, exceptions 02/03 are correct |
| RTU-REL-03 | Read all six FC01 coils and FC02 applied outputs | All six values OFF before first write, or abort with no relay write |
| RTU-REL-04 | For each channel, execute 3 short cycles of 1 s ON / 1 s OFF | FC05 echoes; after every edge and interval, only the selected channel is ON or all channels are OFF as expected |
| RTU-REL-05 | Per channel, hold ON for 30 s, then hold OFF for 5 s | Selected channel remains ON with all others OFF for at least 30 s; all six remain OFF for at least 5 s afterward |
| RTU-REL-06 | Read all six channels after each OFF dwell and after the final channel | All coils and applied outputs OFF before advancing and at completion |
| RTU-REL-07 | Compare RTU frame-error counter before/after and inspect report | No new frame errors; every test step has pass/fail evidence; failures abort the sequence |
| RTU-REL-08 | Observe physical contacts under safe isolated loads | Each channel independently shows OFF/ON/OFF at each respective command; record measurement evidence. Mark physical acceptance incomplete if unmeasured |
| RTU-IO-01 | Use FC16 mailbox opcode 1 (identify), then read command sequence/result and FC04 RGB/reason plus FC02 RGB-active state | Command is applied; RGB registers show the configured nonblack white output with manual-indicator reason; all relay states remain unchanged |
| RTU-IO-02 | Operator briefly double-clicks the physical BOOT button while the runner polls FC02 pressed state and FC04 gesture count/last gesture | Exactly one double-click is recorded, and its configured identify action produces the same five-second white RGB register output |
| RTU-IO-03 | Observer watches the actual LED during each identify indication | Record visibly observed white illumination separately; register values do not prove optical output |

The sequence is channel-serial and energizes no more than one requested relay
at a time. Expected minimum test dwell is 6 × (3 × (1 s ON + 1 s OFF) + 30 s ON
+ 5 s OFF) = 246 seconds, plus Modbus traffic and serial execution overhead.
Reported hold times use a monotonic host clock and are checked against the
requested minimum; these do not measure contact transition time.

The device-I/O test itself does not command any relay. The runner refuses to
start it unless all relay coils and applied outputs are OFF, and verifies they
remain unchanged. The Modbus identify opcode is sent as one FC16 write to
0x0200–0x0203; if the installer permission is absent, the test is denied and
the permission must be enabled in the device UI rather than bypassed. The
button check waits for one brief double-click and verifies gesture counter
increment and identify mapping. Never hold the BOOT button; reset-hold testing
is explicitly out of scope. Avoid single/triple-click testing because those
bindings are not part of this acceptance case.

## 5. Abort and recovery

Stop at the first failed transition/read-back, communication loss, newly
incremented RTU frame-error counter, unsafe load behavior, unexpected
channel state, operator concern, or emergency event. The runner attempts to
write the channel under test OFF in a `finally` path and reports a failed
restoration. That action is best-effort and cannot guarantee OFF after loss of
power, bus, firmware, or output-driver failure.

Use the independent emergency disconnect as needed. Physically verify safe
state and recover through the approved device procedure; do not continue a
partial sequence as if it were a complete acceptance run. Preserve incomplete
reports.

## 6. Runner and evidence

The all-relay runner requires `--confirm-safe-loads`, runs all read-only checks
before actuation, validates the configured RTU unit/baud/format and readiness,
then requires all coils/outputs OFF. If and only if one or more
`--prepare-off-channel` values are explicitly provided, safe loads are
confirmed, and the initial coil/output states match, the runner writes only
authorized initially-ON channels OFF and records each one. Unspecified
initially-ON channels cause the preflight to fail without writes. For each
channel it performs 3 × 1 s ON /
1 s OFF toggles, a 30 s ON dwell, an OFF command and 5 s OFF dwell. It verifies
all six coil and output states after each transition and dwell, restores OFF
on errors where the bus remains usable, stops after the first channel failure,
and saves per-step results and elapsed time to JSON.

```powershell
& .venv\Scripts\python.exe scripts\verify_modbus.py --transport rtu `
  --serial-port COM22 --unit 10 --baud 19200 --parity E --stop-bits 1 `
  --timeout 5 --exercise-all-relays --toggle-cycles 3 `
  --toggle-on-seconds 1 --toggle-off-seconds 1 `
  --on-seconds 30 --off-seconds 5 --confirm-safe-loads `
  --prepare-off-channel 4 `
  --output evidence\modbus-rtu-relay-fat-sat.json
```

Run the optional RGB/button acceptance after confirming the installer
indicator permission and that an observer can see the device LED:

```powershell
& .venv\Scripts\python.exe scripts\verify_modbus.py --transport rtu `
  --serial-port COM22 --unit 10 --baud 19200 --parity E --stop-bits 1 `
  --timeout 5 --exercise-device-io --confirm-indicator-observation `
  --button-timeout-seconds 60 `
  --output evidence\modbus-rtu-device-io-fat-sat.json
```

`--confirm-safe-loads` records the operator's declaration; it is not a sensor
or hardware interlock. The runner's FC02 response is not contact feedback.
Keep the JSON with physical observations, tester/observer sign-off, firmware
identity/hash, device profile, and fixture details. Redact credentials. An
exit code of 0 means the requested software checks passed, 1 means one or more
device checks failed, and 2 means setup/arguments were invalid.
