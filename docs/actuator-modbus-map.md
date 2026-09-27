# Modbus interface contract, version 1

Status: implemented by the shared portable PDU engine and board adapters; see [validation and transport limitations](actuator-implementation.md). Both transports use this exact object model. RTU is a slave/server on the board's RS485 port; TCP is a server over the selected Network uplink. Only one is active, and neither runs alongside KNX. See [production design](actuator-production-design.md).

## Functions and encoding

Implement FC01 Read Coils, FC02 Read Discrete Inputs, FC03 Read Holding Registers, FC04 Read Input Registers, FC05 Write Single Coil, FC06 Write Single Register, FC15 (0x0F) Write Multiple Coils, FC16 (0x10) Write Multiple Registers and FC43/14 (0x2B/0x0E) Read Device Identification. FC08 diagnostics is RTU-only, initially subfunction 0 Return Query Data. Unsupported functions/subfunctions return exception 01. FC22/23 are not required for version 1.

Addresses below are **zero-based PDU offsets**, in hexadecimal, in four independent address spaces. Human labels such as 00001/40001 are not wire addresses. Coils pack least-significant bit first. Registers use network byte order; 32-bit counters span two registers, high word first, with an atomic read snapshot. Booleans are 0/1. Unknown/reserved offsets return exception 02; do not silently alias them.

FC05 accepts only 0x0000 OFF and 0xFF00 ON. Do not adopt vendor-demo toggle value 0x5500. Multi-writes validate address, value, channel enable, lock and access for the entire request before modifying anything. Cross-hole requests fail. Map invalid value to 03, inaccessible/locked object to 02, internal failure to 04, transient queue/transition busy to 06. Bad CRC and frames for other unit IDs are silently discarded. These are the product's application access policies, not new standard function codes.

Read-only objects use DI/IR, not writable coils with ignored writes. Read Device Identification basic objects: 00 vendor name, 01 product code `EDGE-S3-6CH`, 02 firmware revision. Support basic stream read code 01 and appropriate object pagination; return exception 03 for unsupported read codes. Values must reflect the actual released product identity.

Protocol references: [Modbus Organization specifications](https://www.modbus.org/modbus-specifications), [serial-line guide](https://www.modbus.org/file/secure/modbusoverserial.pdf), [TCP implementation guide](https://www.modbus.org/file/secure/messagingimplementationguide.pdf). Check function limits and exception behavior against the application specification during implementation.

## Coils: FC01 / FC05 / FC15

| Offset | Meaning | Access/persistence |
| --- | --- | --- |
| 0x0000–0x0005 | CH1–CH6 requested steady ON/OFF; reads applied commanded state | Runtime write if channel enabled/unlocked and protocol channel mask permits |
| 0x0010–0x0015 | CH1–CH6 enabled | Staged configuration, commissioning window required |
| 0x0020 | RGB enabled | Staged configuration |
| 0x0021 | Buzzer enabled | Staged configuration |
| 0x0022 | Button application actions enabled | Staged configuration; reserved reset remains available |
| 0x0023 | RS485 enabled | Read status only through DI 0x0023; this coil address is reserved/unmapped |
| 0x0030 | RGB manual override request | Runtime; requires RGB enabled; higher-priority indications prevail |
| 0x0031 | Buzzer manual tone request | Runtime; requires buzzer enabled; time-bounded |

Manual controls ON start once on a rising edge; OFF cancels. Their coils clear when the timeout expires. Repeated ON does not extend the deadline. Ordinary relay writes cancel that channel's active pulse. No Modbus factory-reset or protocol-mode command is exposed. RS485 enable is web-only because self-disabling a bus during a transaction is ambiguous; its state and UART configuration remain readable.

## Discrete inputs: FC02

| Offset | Meaning |
| --- | --- |
| 0x0000–0x0005 | Applied relay output state, not measured contacts |
| 0x0010 | Debounced BOOT currently pressed |
| 0x0011 | Application button actions enabled |
| 0x0012 | RGB output currently nonblack |
| 0x0013 | Buzzer currently sounding |
| 0x0014 | Selected network uplink has usable IPv4 |
| 0x0015 | AP active |
| 0x0016 | Any active fault |
| 0x0017 | KNX programming active (always false while Modbus is active) |
| 0x0020 | RGB enabled |
| 0x0021 | Buzzer enabled |
| 0x0022 | Button service available |
| 0x0023 | RS485 enabled |
| 0x0024 | RS485 port initialized |
| 0x0025 | Active Modbus transport ready |
| 0x0026 | Temporary Modbus configuration window open |
| 0x0030–0x0035 | Channel blocked/locked |

## Holding registers: FC03 / FC06 / FC16

| Offset | Field | Range/default and behavior |
| --- | --- | --- |
| 0x0100 | Manual RGB red | 0–255, default 0 |
| 0x0101 | Manual RGB green | 0–255, default 0 |
| 0x0102 | Manual RGB blue | 0–255, default 0 |
| 0x0103 | Manual brightness | 0–100 percent, default 10 |
| 0x0104 | Manual RGB duration | 1–30 seconds, default 5 |
| 0x0110 | Manual buzzer frequency | 500–4000 Hz proposed qualification range, default 2000 |
| 0x0111 | Manual buzzer duration | 10–2000 ms, default 100 |
| 0x0112 | Manual buzzer duty | 1–50 percent, default 25 |
| 0x0120 | Single-click action | Action enum below; staged |
| 0x0121 | Single-click target | 0 none/all; 1–6 channel; staged |
| 0x0122–0x0123 | Double-click action/target | Same format; staged |
| 0x0124–0x0125 | Triple-click action/target | Same format; staged |
| 0x0130–0x0135 | CH1–CH6 startup state | 0 OFF / 1 ON; staged |
| 0x0140–0x0145 | CH1–CH6 pulse duration | 10–60000 ms; default 1000; staged |
| 0x0150–0x0155 | CH1–CH6 disconnect policy | 0 hold / 1 OFF; default hold; staged |
| 0x0160–0x0165 | CH1–CH6 disconnect timeout | 1–3600 seconds; default 30; staged |
| 0x0200 | Command opcode | 0 idle, 1 identify, 2 acknowledge alarm, 3 relay pulse, 4 apply staged config, 5 discard stage |
| 0x0201 | Command target | 0 device, 1–6 relay |
| 0x0202 | Request sequence | 1–65535; client increments per command |
| 0x0203 | Commit trigger | Write 0xA55A, read returns 0; no action for 0 |

Actions: 0 none, 1 relay ON, 2 relay OFF, 3 relay toggle, 4 relay pulse, 5 all OFF, 6 identify, 7 acknowledge, 8 KNX programming toggle (binding only executes in KNX mode). Validate action/target pairing at configuration apply. Manual RGB/buzzer values are RAM parameters, not flash writes. Changes take effect on the next trigger. Generic manual tests cannot suppress fault or programming indications.

The command mailbox must be submitted as one FC16 write covering 0x0200–0x0203; reject FC06 on these four registers. Latch all four words together, validate, execute once and expose result in IR. Deduplicate same request sequence and payload per TCP session or RTU bus session; an identical retry returns the previous result, and reused sequence with a different payload fails. Retain the last 16 results for 60 seconds; clients must not reuse a sequence within that interval. A restart clears this cache, so clients must check uptime before retrying a pulse. Only an authorized window can apply/discard configuration; ordinary runtime access is governed by the configured fieldbus policy.

## Input registers: FC04

| Offset | Meaning |
| --- | --- |
| 0x0000 | Register-map version = 1 |
| 0x0001 | Capability bits: bits 0–4 relay/RGB/button/buzzer/RS485, bits 5–7 RTU/TCP/KNX compiled |
| 0x0002 | Effective protocol: 0 OFF, 1 RTU, 2 TCP, 3 KNX |
| 0x0003 | Network: 0 offline, 1 AP only, 2 STA connecting, 3 uplink IPv4 ready, 4 uplink IPv4 ready + AP |
| 0x0004 | Primary fault code: 0 none; codes defined in release error catalog |
| 0x0005 | Last gesture: 0 none, 1 single, 2 double, 3 triple, 4 reset hold |
| 0x0006–0x0007 | Gesture sequence counter |
| 0x0008–0x0009 | Uptime seconds |
| 0x000A–0x000B | Applied configuration revision |
| 0x0010–0x0011 | Received valid request count |
| 0x0012–0x0013 | CRC/frame error count |
| 0x0014–0x0015 | Rejected write count |
| 0x0020 | RTU unit address |
| 0x0021 | Baud enum: 0=9600, 1=19200, 2=38400, 3=57600, 4=115200 |
| 0x0022 | Serial format: 0=8E1, 1=8O1, 2=8N2, 3=8N1 compatibility |
| 0x0023 | TCP port |
| 0x0024 | TCP client count |
| 0x0025 | Last command sequence |
| 0x0026 | Command result: 0 none, 1 pending, 2 applied, 3 rejected, 4 failed |
| 0x0027 | Last command detail: 0 none, 1 bad target, 2 disabled, 3 locked, 4 denied, 5 persistence failure, 6 stale revision |
| 0x0030 | Current RGB red, 0–255 |
| 0x0031 | Current RGB green, 0–255 |
| 0x0032 | Current RGB blue, 0–255 |
| 0x0033 | Current buzzer frequency Hz, 0 if silent |
| 0x0034 | Indicator reason: 0 idle, 1 network, 2 manual, 3 error, 4 OTA, 5 KNX programming, 6 reset |

No hardware gesture injection is provided: remote callers can invoke equivalent authorized actions but cannot falsify physical press state. Counters wrap modulo 2^32. Every multi-register read is internally consistent.

## Configuration access and transport profiles

By default fieldbus writes allow only enabled relay commands within the commissioned channel mask. Manual indicators, pulse/acknowledge commands and configuration writes have separate installer-set permissions. A web installer can open a 60-second configuration window (maximum 300 seconds) for a selected TCP peer/session, or the physically controlled RTU bus. RTU cannot authenticate which master sent a frame; the UI must state that all bus participants can use that window.

Staged writes are RAM-only and read back as staged values to the window owner; other clients see effective settings. Record the base configuration revision when the stage opens. Apply validates the entire candidate and that revision before durable commit; concurrent web changes yield a conflict and leave effective state unchanged. Closing/expiry discards uncommitted data. Active UART/network/profile settings, access policies and protocol family are web-only. This deliberately avoids losing the response connection during register configuration.

RTU profile: enabled RS485 required; unit 1–247 (default 1); baud default 19200, 8E1; selectable 8O1/8N2 and explicitly labeled 8N1 compatibility. Hardware UART idle events delimit frames; strict 1.5-character gap rejection inside buffered frames remains a qualification limitation. Maximum RTU ADU 256 bytes. Verify automatic hardware turnaround at every offered baud; remove unqualified baud choices from release. Address 0 broadcast accepts only validated FC05/15 writes entirely within the six relay coils, with no reply; configuration broadcasts and reads are ignored. No second master polling service runs on the port.

TCP profile: configurable port default 502, unit ID default 1, connection limit default 4, idle timeout default 60 seconds and optional exact source IPv4 allowlist. Bind to the selected IPv4 uplink; provisioning AP exposure defaults OFF. Validate MBAP protocol ID, transaction ID, length and configured unit ID; parse fragmented and concatenated ADUs and cap buffers (260-byte maximum ADU). Close malformed streams after a bounded error; invalid unit ID receives a documented exception 0x0B from this server profile. Unit 0 is not a TCP broadcast. No RTU-over-TCP framing and no CRC in TCP ADUs.

Both adapters use the same in-repository portable PDU engine (`src/protocols/ModbusPdu.h`), covered by native tests including FC43/14 and FC08. Real-master interoperability remains to be qualified.

## Required interoperability tests

Test FC01/02/03/04 boundaries, FC05 legal values, FC06 range checks, FC15/16 all-or-nothing rejection, reserved holes, FC43 identity, FC08 echo, broadcast silence, bad CRC, delayed characters, competing clients and timed-out commands. Repeat the same map tests on RTU and TCP. Verify denied bus writes cannot change disabled hardware, security settings, UART mode or factory reset. Confirm staged edits never write flash until Apply and recover correctly after interrupted commit.
