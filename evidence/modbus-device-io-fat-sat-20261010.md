# Modbus RGB and physical-button FAT/SAT report

## Result

The optional device-I/O acceptance cases passed over Modbus RTU first and
Modbus TCP second. The operator also confirmed direct visual observation of
the white identify indication on both transports. This is an
IEC 62381-aligned project FAT/SAT record, not an IEC or Modbus certification.

| Transport | Profile | Result |
| --- | --- | --- |
| RTU | COM22, unit 10, 19200 baud, 8E1 | 16 checks passed, 0 failed |
| TCP | 192.168.71.48:502, unit 10 | 13 checks passed, 0 failed on the final run |
| TCP post-restoration | 192.168.71.48:502, unit 10 | 10 read-only baseline checks passed |

## Device-I/O acceptance

| Case | Result | Evidence |
| --- | --- | --- |
| RTU-IO-01 Modbus identify | PASS; mailbox sequence 1 applied (result 2/detail 0); RGB input registers read `[25, 25, 25]` with the manual-indicator reason | [RTU JSON](./modbus-rtu-device-io-fat-sat-20261010T083016.json) |
| RTU-IO-02 physical button | PASS; exactly one double-click event was observed, with the debounced-pressed input sampled; configured double-click action was identify (action 6) | [RTU JSON](./modbus-rtu-device-io-fat-sat-20261010T083016.json) |
| RTU-IO-03 visual indication | Operator confirmed the white indication was visible for both RTU identify triggers | Operator observation recorded for this report |
| TCP-IO-01 Modbus identify | PASS; mailbox sequence 3 applied (result 2/detail 0); RGB input registers read `[25, 25, 25]` with the manual-indicator reason | [TCP JSON](./modbus-tcp-device-io-retry-20261010T083425.json) |
| TCP-IO-02 physical button | PASS; exactly one double-click event was observed, with the debounced-pressed input sampled; configured double-click action was identify (action 6) | [TCP JSON](./modbus-tcp-device-io-retry-20261010T083425.json) |
| TCP-IO-03 visual indication | Operator confirmed the white indication was visible for both TCP identify triggers | Operator observation recorded for this report |

The Modbus checks verified RGB register output and indicator state, not emitted
light. The visual acceptance above is the operator's independent observation.
The RGB brightness register was 10%, which explains the white output values of
25 per color channel.

## Safety, restoration, and limitations

- All six relay coils and applied-output inputs were OFF at the device-I/O
  preflight. The runner verified relay states remained unchanged; this
  addendum did not exercise relay contacts.
- RTU frame-error count remained zero: counters before were
  `[0, 317, 0, 0, 0, 8]`; after were `[0, 378, 0, 0, 0, 10]`. The frame-error
  words (indexes 2–3) did not change.
- The installer permission **Allow manual indicators and diagnostic
  commands** was enabled for the tests with operator approval, then restored
  OFF. The device remains in Modbus TCP mode, running on port 502, unit 10.
- After restoration, the TCP baseline reconnected successfully and confirmed
  identity, active TCP mode, and all relays OFF:
  [post-restoration JSON](./modbus-tcp-postrestore-20261010T083624.json).
- No physical relay-contact measurements were part of these new RGB/button
  tests. Refer to the earlier [RTU relay report](./modbus-rtu-relay-fat-sat-20261010T080500.md)
  and [TCP relay report](./modbus-tcp-relay-fat-sat-20261010T074400.md) for
  the separate six-channel relay runs.

## Preserved unsuccessful TCP attempts

The first TCP probe used unit 1 and was rejected with exception `0x0B`; the
device UI showed that the configured unit address was 10. A subsequent
unit-10 run passed the Modbus identify test but did not observe the button
gesture before its 120-second timeout. Both records are retained; neither is
the accepted final run:

- [Wrong-unit attempt JSON](./modbus-tcp-device-io-fat-sat-20261010T083058.json)
- [Button-timeout attempt JSON](./modbus-tcp-device-io-fat-sat-20261010T083128.json)
- [Accepted TCP rerun JSON](./modbus-tcp-device-io-retry-20261010T083425.json)

