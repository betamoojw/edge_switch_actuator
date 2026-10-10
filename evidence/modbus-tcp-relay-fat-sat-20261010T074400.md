# Modbus TCP relay FAT/SAT test report

## Result

**PASS — Modbus TCP command/read-back sequence. Physical-contact acceptance is incomplete:** no independent contact instrument results were captured. The device reports commanded/applied output state and does not measure contact position.

## Run record

| Field | Result |
| --- | --- |
| Procedure | [Modbus TCP relay FAT/SAT plan](../docs/modbus-tcp-relay-fat-sat.md) |
| Standards alignment | IEC 62381 FAT/SAT approach; Modbus Application Protocol Specification V1.1b3 |
| Start (local, UTC+08:00) | 2026-10-10 07:44:36 |
| Completion (local, UTC+08:00) | 2026-10-10 07:47:37 |
| Elapsed | 181.128 seconds |
| Target / transport | `192.168.71.48`, Modbus TCP running |
| TCP port / unit | `502` / `1` |
| Product / firmware | `EDGE-S3-6CH` / `0.6.2` |
| Operator safe-load confirmation | Confirmed before actuation |
| Initial state | All six coils and output reads OFF |
| Final state | All six coils and output reads OFF |
| Overall software checks | **30 passed, 0 failed** |
| Firmware hash, board serial/revision | Not recorded |
| Independent physical-contact evidence | Not captured |

## Channel results

| Channel | FC05 ON / FC01+FC02 | 30-second hold | FC05 OFF / FC01+FC02 | Result |
| --- | --- | --- | --- | --- |
| CH1 | Pass; only CH1 read ON | Pass | Pass; all read OFF | Pass |
| CH2 | Pass; only CH2 read ON | Pass | Pass; all read OFF | Pass |
| CH3 | Pass; only CH3 read ON | Pass | Pass; all read OFF | Pass |
| CH4 | Pass; only CH4 read ON | Pass | Pass; all read OFF | Pass |
| CH5 | Pass; only CH5 read ON | Pass | Pass; all read OFF | Pass |
| CH6 | Pass; only CH6 read ON | Pass | Pass; all read OFF | Pass |

Modbus identity, map version, capabilities, transport status/configuration,
reserved-address exception, invalid-coil-value exception, all-OFF preflight,
and final-state checks passed. See the machine-readable
[JSON evidence](modbus-tcp-relay-fat-sat-20261010T074400.json) for the detailed
transcript and timestamps.

## Scope and qualification

The runner verified protocol writes and device read-backs. It cannot observe
electrical contacts; therefore this report does not claim that contact
movement, output wiring, or connected loads were independently verified.
Complete physical SAT sign-off only after adding instrument identity and
observed OFF/ON/OFF results per channel to the evidence package. This is an
acceptance procedure aligned with the cited standards, not a certification.
