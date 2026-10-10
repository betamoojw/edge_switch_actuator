# Modbus RTU relay FAT/SAT test report

## Result

**PASS — RTU protocol and commanded-output sequence. Physical-contact acceptance is incomplete:** independent contact measurements were not captured. The device reports commanded/applied output state, not measured contact position.

## Run record

| Field | Result |
| --- | --- |
| Procedure | [Modbus RTU relay FAT/SAT plan](../docs/modbus-rtu-relay-fat-sat.md) |
| Standards alignment | IEC 62381 FAT/SAT approach; Modbus Serial Line Guide V1.02; Modbus Application Protocol V1.1b3 |
| Start (local, UTC+08:00) | 2026-10-10 08:07:03 |
| Completion (local, UTC+08:00) | 2026-10-10 08:12:08 |
| Elapsed | 304.763 seconds |
| Product / firmware | `EDGE-S3-6CH` / `0.6.2` |
| Protocol / serial profile | Modbus RTU running; unit 10; 19200 8E1 |
| Host adapter | CH343 USB-Enhanced-SERIAL on COM22 |
| Safe isolated test loads | Operator confirmed before run |
| Initial state | CH1–CH3, CH5–CH6 OFF; CH4 ON |
| Authorized preparation | CH4 written OFF; coil/output matched and all six then read OFF |
| Final state | All six coils and applied outputs OFF |
| Software checks | **113 passed, 0 failed** |
| Firmware hash, board serial/revision | Not recorded |
| Independent physical-contact evidence | Not captured |

## Per-channel results

Each channel completed three 1-second ON / 1-second OFF cycles, followed by a
30-second ON dwell and 5-second OFF dwell. At every transition and dwell
check, FC01 coil and FC02 output values matched the expected pattern: only the
selected channel ON during ON phases; all channels OFF during OFF phases.

| Channel | Toggle cycles | Long ON dwell | Final OFF dwell | Result |
| --- | --- | --- | --- | --- |
| CH1 | 3/3 pass | 30.446 s observed | 5.440 s observed | Pass |
| CH2 | 3/3 pass | 30.447 s observed | 5.447 s observed | Pass |
| CH3 | 3/3 pass | 30.448 s observed | 5.450 s observed | Pass |
| CH4 | 3/3 pass | 30.431 s observed | 5.465 s observed | Pass |
| CH5 | 3/3 pass | 30.465 s observed | 5.461 s observed | Pass |
| CH6 | 3/3 pass | 30.449 s observed | 5.450 s observed | Pass |

The 18 short ON holds measured 1.432–1.467 s and the 18 short OFF holds
measured 1.432–1.467 s. Measurements include host scheduling and response
read-back after the requested sleep; they are minimum dwell checks, not relay
contact transition-time measurements.

## Protocol evidence

- Identity, map version, effective protocol, RTU profile, RS-485 readiness,
  FC08 diagnostics echo, reserved-address exception 02, and invalid FC05
  exception 03 passed.
- The device profile matched the host: unit 10, baud enum 1 (19200), format 0
  (8E1); effective protocol was RTU.
- The RTU frame-error counter remained **0** (before and after). The request
  counter advanced from 35 to 288. The rejected-request counter advanced from
  4 to 6 due to the two expected negative checks.
- Final read-only verification and the device UI both showed all six relays
  OFF; the UI still showed Modbus RTU running.

See the machine-readable [JSON evidence](modbus-rtu-relay-fat-sat-20261010T080500.json)
for all 113 individual checks, elapsed times, profile, and counter values.

## Qualification limits

The host verified valid RTU replies, CRCs, protocol behavior, coil/output
read-backs, interval stability at poll time, and final commanded state. No
contact instrument readings were provided, so this does not establish that
physical contacts moved. Firmware hash, board serial/revision, fixture
instrument identity, and physical observations should be added before full
physical SAT sign-off. This is an acceptance procedure aligned with the cited
standards, not certification or relay endurance qualification.
