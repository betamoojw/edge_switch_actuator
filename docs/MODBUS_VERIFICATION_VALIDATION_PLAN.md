# Modbus RTU and TCP verification and validation plan

Status: proposed release qualification plan  
Baseline reviewed: `dev` at `9669340` on 2026-10-09  
Product: Edge S3 Relay 6CH, `waveshare-relay-6ch` environment

## 1. Purpose and scope

This plan verifies the implemented Modbus application contract and validates both transports with representative masters, the production board, and physical relay outputs. It covers Modbus RTU slave operation and Modbus TCP server operation.

KNX has already been verified against its design and is outside this campaign. KNX is used only as a known-good mode when checking protocol exclusivity and transitions; its package, commissioning, and group communication tests do not need to be repeated.

This plan does not claim formal Modbus conformance certification or electrical safety certification.

## 2. Release criteria

A release candidate passes when:

1. All automated gates pass from a clean checkout of the candidate commit.
2. Every `P0` case passes on the release firmware and production board revision for both RTU and TCP.
3. At least one independent third-party master reproduces the core read/write results on each transport.
4. No unresolved defect can energize a relay unexpectedly, bypass an access policy, partially apply a rejected request, corrupt durable configuration, or prevent management recovery.
5. Evidence identifies the firmware hash, board serial/revision, configuration, tools, topology, tester, and expected/actual result.
6. Any accepted `P1` exception has an owner, risk assessment, documented limitation, and target release.

`P0` is release blocking. `P1` is required unless explicitly risk accepted. `P2` is extended compatibility or endurance coverage.

## 3. Implementation baseline and risks

Both transports use the portable PDU engine in `src/protocols/ModbusPdu.h` and the version 1 object model in [the Modbus map](actuator-modbus-map.md). Implemented functions are FC01, FC02, FC03, FC04, FC05, FC06, FC15, FC16, FC43/14, and RTU-only FC08 subfunction 0.

`src/protocols/Modbus.cpp` owns TCP MBAP/session handling and UART frame handling. Protocol selection, network readiness, transition, rollback, and persistence are owned by `src/device/Actuator.cpp`; REST authorization and the temporary configuration-window command are in `src/device/ActuatorApi.cpp`.

Existing native tests cover the shared PDU parser, response bounds, exceptions, malformed/truncated PDUs, broadcast restrictions, FC08, and FC43/14. Simulator/browser tests cover configuration UI, protocol lifecycle, permissions, rollback, and decoded Modbus effects. They do not drive the real TCP socket adapter, UART timing, RS485 transceiver, or relay contacts.

Primary residual risks are:

- RS485 automatic direction turnaround, parity, baud-rate tolerance, termination, and frame timing.
- Strict RTU 1.5-character internal-gap rejection is not implemented and requires an explicit release decision.
- TCP stream fragmentation, concatenated ADUs, malformed MBAP handling, client limits, session-bound configuration windows, allowlists, and listener rebinding.
- Application-map atomicity, authorization, deduplication, persistence, and physical output agreement under real transport traffic.

## 4. Requirements traceability

| ID | Requirement | Primary evidence |
| --- | --- | --- |
| GEN-01 | Only the selected protocol runs; failed transitions restore the previous usable configuration | REST snapshots, network/serial capture, device log |
| GEN-02 | Boot, restart, protocol transition, malformed traffic, and network loss cause no uncommanded relay pulse | Contact or logic-analyzer trace |
| MB-01 | FC01/02/03/04/05/06/15/16 and FC43/14 conform to map version 1 | Probe JSON and independent-master transcript |
| MB-02 | Invalid function, address, value, permission, lock, and busy conditions return the specified result without partial mutation | Negative-test transcript and before/after snapshot |
| MB-03 | Coil state, applied discrete input, REST state, and physical contact agree | Master transcript and contact trace |
| MB-04 | Mailbox commands are atomic and deduplicated by peer/session, sequence, and payload | Two-client transcript and counters |
| MB-05 | Configuration staging is isolated to its owner and commits atomically against the base revision | Two-client transcript, REST snapshots, restart check |
| RTU-01 | Unit addressing, CRC, frame limits, broadcast silence, timing, and automatic turnaround work at every released serial profile | Serial capture, waveform, and master report |
| TCP-01 | MBAP validation, fragmentation, concatenation, client limit, allowlist, session ownership, and idle timeout match the contract | Packet capture and raw-socket report |
| SEC-01 | Web roles, channel masks, Modbus permissions, source allowlist, and configuration-window boundaries cannot be bypassed | Authorization matrix |
| DUR-01 | Committed settings survive restart/power loss; uncommitted staging does not | Pre/post snapshots and interruption log |
| REC-01 | Loss of master, malformed traffic, failed transition, and restart leave the device recoverable through web management | Recovery log |

## 5. Equipment and topology

Record model, serial number, software version, and calibration status where applicable.

- Production-representative Edge S3 Relay 6CH and release power supply.
- Six isolated low-voltage test loads or a contact/logic analyzer. Do not start with mains loads.
- USB serial console at 115200 baud for firmware logs.
- Isolated USB-to-RS485 adapter supporting the configured parity and stop bits.
- 120-ohm termination at both RS485 bus ends and a common reference if required by the adapters.
- Host with Python, PlatformIO, Node 24, Wireshark, and an independent Modbus master such as QModMaster or `modpoll`.
- Optional programmable serial sender for timing faults, managed switch/capture host, controllable DC supply, and oscilloscope/logic analyzer.

Use a dedicated test network. Back up the device configuration before destructive tests. Keep an authenticated administrator session available for recovery. Start with all relays physically confirmed OFF.

## 6. Automated verification gates

Run from the repository root and retain complete logs:

```powershell
py -m platformio run -e waveshare-relay-6ch
py scripts/test_native.py
py scripts/verify_modbus.py --self-test
Set-Location interface
npm ci
npm run check
npm run build
npm run test:bundle
npm run test:sim
npx playwright test --project=chromium
```

Acceptance:

- Firmware and frontend builds complete without errors.
- Native PDU, network, durable-store, simulator, and browser tests pass.
- The Modbus verifier codec self-test passes.
- Failures caused by occupied frontend/simulator ports are rerun with isolated ports; they are not recorded as product passes or failures.

## 7. Device preparation

1. Flash the candidate and record commit, firmware SHA-256, PlatformIO environment, partition layout, board revision, and device serial number.
2. Set every startup state OFF and verify all contacts are OFF before and after boot.
3. Configure one enabled/permitted test channel, one disabled channel, and one enabled but blocked or policy-denied channel.
4. Record RTU unit, baud, serial format, TCP port, TCP unit, client limit, idle timeout, allowlist, channel mask, and write permissions.
5. Synchronize host clocks and start firmware, master, packet/serial, and physical-output capture before selecting Modbus mode.
6. Execute runtime writes only with isolated loads and an observer or instrument confirming the final safe state.

Every result records requirement ID, case ID, exact configuration/steps, expected result, actual result, pass/fail, defect, and evidence filenames.

## 8. Host verification tool

The repository includes `scripts/verify_modbus.py`. TCP uses only the Python standard library. RTU additionally requires `pyserial`. For setup, command examples, output interpretation, and troubleshooting, use the [Modbus verifier quick start](modbus-verifier-quickstart.md).

TCP read and negative checks:

```powershell
py scripts/verify_modbus.py --transport tcp --host 192.168.1.111 --unit 1 `
  --output evidence/modbus-tcp-smoke.json
```

RTU read and negative checks:

```powershell
py -m pip install pyserial
py scripts/verify_modbus.py --transport rtu --serial-port COM4 --unit 1 `
  --baud 19200 --parity E --stop-bits 1 `
  --output evidence/modbus-rtu-smoke.json
```

The default probe checks identity, map version, capabilities, relay/state reads, transport status/configuration, a reserved address, invalid FC05 value handling, and RTU diagnostics echo. It does not request a relay state change. The invalid FC05 value must return exception 03 before backend mutation.

For physical actuation on an isolated load, add `--write-channel 1`. The tool reads the original coil, changes it, verifies coil and discrete-input state, and restores the original value in a `finally` path. Independently verify the contact transition and final state. Do not rely on software restoration as the only safety control.

The probe is a smoke/evidence tool, not a complete transport conformance suite. Use an independent master for ordinary interoperability and a raw socket/programmed serial sender for malformed, timing, fragmentation, and concurrency cases.

## 9. Shared object-model tests

Run every `P0` case over RTU and TCP. Compare coil reads with applied discrete inputs, REST status, and physical contacts; matching software values alone do not prove relay operation.

| Priority | Case | Procedure | Expected result |
| --- | --- | --- | --- |
| P0 | MB-C01 | Read first/last valid and one invalid boundary for FC01, FC02, FC03, and FC04 | Valid values match device state; invalid/cross-hole ranges return 02 |
| P0 | MB-C02 | FC05 each enabled channel with `0000`/`FF00`; try `0001` and `5500` | Legal writes apply; illegal values return 03 and change nothing |
| P0 | MB-C03 | FC06 minimum/maximum and out-of-range settings; attempt FC06 on mailbox words | Valid values read back; invalid value returns 03; mailbox FC06 is rejected |
| P0 | MB-C04 | FC15/16 valid multi-writes, then include one invalid, locked, disabled, or hole member | Valid request applies completely; invalid request applies nothing |
| P0 | MB-C05 | Read FC43/14 from objects 00, 01, and 02 | Vendor, `EDGE-S3-6CH`, firmware identity, count, and pagination are correct |
| P0 | MB-C06 | Read all documented DI/IR status fields and 32-bit counters while generating traffic | Values match REST/runtime state; counter words are one snapshot and increment correctly |
| P0 | MB-C07 | Exercise disabled, blocked, channel-mask-denied, indicator-denied, and configuration-denied writes | Specified exception is returned; REST and contacts prove no mutation |
| P0 | MB-C08 | Submit mailbox FC16, retry same sequence/payload, then reuse sequence with changed payload | Command executes once; identical retry returns prior result; changed payload is rejected |
| P0 | MB-C09 | Open configuration window; stage/read as owner and non-owner; expire/discard; then apply | Isolation, expiry, discard, revision conflict, atomic commit, and restart persistence match the map |
| P0 | MB-C10 | Attempt unsupported functions, zero/excess counts, truncated PDUs, maximum PDU, and cross-address overflow | Correct exception/discard; no reset, stale mutation, or buffer overrun symptom |
| P1 | MB-C11 | Trigger RGB/buzzer rising edge twice, cancel, and wait past timeout | Repeated ON does not extend deadline; OFF cancels; coil clears at timeout |
| P1 | MB-C12 | Pulse all enabled channels while one is blocked | Entire command is rejected; no relay changes |

For all write failures, capture before/after reads in the same evidence record. For staged configuration, verify flash is unchanged until Apply by restarting before commit.

## 10. Modbus RTU validation

Begin at default 19200 8E1, then qualify every serial profile exposed by the release UI. Test both short requests and the largest supported request/response at the slowest and fastest baud.

| Priority | Case | Procedure | Expected result |
| --- | --- | --- | --- |
| P0 | RTU-01 | Run the shared matrix at 19200 8E1 using repository and independent masters | All shared cases pass; no unsolicited frames |
| P0 | RTU-02 | Repeat identity, read, single write, FC15, and FC16 at every baud/format | Every advertised profile works reliably; unqualified choices are removed |
| P0 | RTU-03 | Address configured unit, adjacent units, and unit 0 reads | Configured unit replies; other-unit and broadcast reads are silent |
| P0 | RTU-04 | Corrupt CRC and send truncated, oversized, malformed, and unsupported-function frames | CRC/frame errors are silent where specified; unsupported valid function returns 01; service recovers |
| P0 | RTU-05 | Broadcast valid FC05/15 relay writes, then invalid/configuration broadcasts | Valid relay broadcast applies without reply; all other broadcasts are silent/non-mutating |
| P0 | RTU-06 | Measure response delay and driver-enable release for minimum/maximum frames at every profile | No clipped byte, echo corruption, collision, or stuck transmitter |
| P0 | RTU-07 | Poll while opening, using, expiring, and manually closing the RTU configuration window | Any physical bus participant can use it only while open; staging is discarded on expiry/close |
| P1 | RTU-08 | Inject gaps below/above 1.5 and 3.5 character times | Actual behavior is recorded against the serial-line specification and known implementation gap |
| P1 | RTU-09 | Disconnect/reconnect A/B, remove termination, inject noise, and restart during polling | No unintended relay operation; counters/faults update; valid traffic recovers |
| P2 | RTU-10 | Run mixed reads/writes for 24 hours at highest qualified baud | No resets, leaks, counter anomalies, stuck bus, or unintended output |

RTU-08 cannot be waived as “works as implemented.” The release report must either show strict timing compliance, constrain supported masters/topology, or explicitly accept and document the interoperability risk.

## 11. Modbus TCP validation

Capture traffic with Wireshark. Execute tests on the selected uplink, then verify exposure from the provisioning AP and any alternate interface.

| Priority | Case | Procedure | Expected result |
| --- | --- | --- | --- |
| P0 | TCP-01 | Run the shared matrix on configured port/unit using repository and independent masters | Correct MBAP transaction IDs, lengths, unit handling, and no CRC bytes |
| P0 | TCP-02 | Split one ADU at each MBAP/PDU boundary, including one-byte fragments | Server buffers it and returns exactly one correct response |
| P0 | TCP-03 | Send two and then several complete ADUs in one TCP write | Responses are complete, ordered, and retain matching transaction IDs |
| P0 | TCP-04 | Vary protocol ID, length 0/1/oversize, unit, and truncated body | Documented exception/connection close occurs within a bound; no parser desynchronization |
| P0 | TCP-05 | Open configured client limit, attempt one extra, close/reconnect, and let one idle | Limit/idle timeout are enforced and slots are reclaimed |
| P0 | TCP-06 | Test allowed and denied exact source IPv4 addresses and provisioning AP exposure | Allowlist and interface policy are enforced before application access |
| P0 | TCP-07 | Open a window for one IP; connect twice from it and once from another source | Window binds to the first requesting TCP session, not merely the IP address |
| P0 | TCP-08 | Remove selected-uplink IPv4 during active sessions, restore it, and change interface | Listener closes/rebinds; no stale authorized session or partial command survives |
| P0 | TCP-09 | Send unit 0 and wrong-unit requests | Unit 0 is not broadcast; invalid unit receives documented 0B behavior and no mutation |
| P1 | TCP-10 | Four clients concurrently poll/write and contend on mailbox sequences | Responses reach the correct clients; deduplication and state remain coherent |
| P1 | TCP-11 | Repeated malformed streams and slow partial frames while valid clients poll | Bounded resources; malformed clients close; valid service remains responsive |
| P2 | TCP-12 | Four mixed-load clients run for 24 hours with periodic reconnects | No reset, leak, stuck slot, transaction mismatch, or unintended output |

High-level Modbus libraries often hide TCP segmentation and reject malformed MBAP before transmission. Use a raw-socket harness or packet generator for TCP-02 through TCP-04 and save its source/version with the evidence.

## 12. Security, transition, durability, and recovery

1. Attempt reads/writes as each web role and with every protocol permission disabled/enabled. Verify web roles and Modbus interface policies remain independent.
2. Verify Modbus cannot alter accounts, security settings, active network/UART profile, protocol family, RS485 enable, OTA, or factory reset.
3. Transition `off -> RTU -> TCP -> KNX -> TCP -> RTU -> off` with relays OFF, ON, pulsing, blocked, and during network loss. KNX is only a known-good transition target in this test.
4. Confirm only the selected stack emits/listens: no stale TCP listener, UART response, or KNX traffic remains after transition.
5. Force RTU and TCP initialization failures. Verify atomic rollback restores the previous durable mode and management path.
6. Restart during staged edits and after committed changes. Uncommitted state disappears; committed state survives with the correct revision.
7. Interrupt power during configuration commit using the controlled lab method. Verify one complete generation recovers and no partial settings become effective.
8. Flood bounded malformed traffic while using web management. Verify responsiveness, rate/counter behavior, and recovery after traffic stops.
9. Remove and restore the active master/network. Verify configured disconnect policy and timeout for every channel without affecting disabled channels.

## 13. Evidence package

Store evidence outside tracked source unless the release process explicitly requires it. Suggested structure:

```text
evidence/<date>-modbus-qualification/
  manifest.json
  results.csv
  automated/
  modbus-common/
  modbus-rtu/
  modbus-tcp/
  physical/
  recovery/
```

`manifest.json` records commit, firmware hash, board identity/revision, tool versions, topology, configuration, tester, and start/end times. Preserve probe JSON, independent-master exports, packet/serial captures, waveforms, REST snapshots, firmware logs, power-interruption timing, and annotated setup photographs.

Redact passwords, tokens, Wi-Fi credentials, and signing material. Hash collected evidence after the run. A screenshot without candidate identity and a corresponding machine-readable transcript is supporting evidence, not the sole result.

## 14. Exit report

Publish:

- Candidate commit and firmware hash.
- Hardware, serial-profile, client, and tool matrix.
- Pass/fail/blocked totals by `P0`, `P1`, and `P2`.
- Requirements without evidence.
- Defects and accepted limitations, especially RTU internal-gap behavior.
- Explicit recommendation: pass, conditional pass, or fail.

The release result is **fail** if either transport lacks real-master and physical-output evidence, any `P0` case fails, protocol exclusivity/rollback fails, or an unauthorized/rejected request causes a partial or physical state change.