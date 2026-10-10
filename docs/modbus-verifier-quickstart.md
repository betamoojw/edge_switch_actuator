# Modbus verifier quick start

Use `scripts/verify_modbus.py` to perform repeatable smoke checks against the actuator over Modbus TCP or Modbus RTU. The default run does not request a relay state change. See the [full qualification plan](MODBUS_VERIFICATION_VALIDATION_PLAN.md) for transport timing, malformed-frame, concurrency, endurance, and physical-output testing.

## 1. Start safely

1. Use isolated low-voltage loads or disconnect mains loads.
2. Confirm all relay contacts are in the expected state.
3. Open the device web interface and select either **Modbus TCP** or **Modbus RTU**.
4. Record the unit ID and transport settings shown by the device.
5. For RTU, enable RS485 and connect A/B, common reference if required, and bus termination before running the tool.
6. Run commands from the repository root:

```powershell
Set-Location C:\BetaMoojw\edge_switch_actuator
```

## 2. Check the local tool

No device is needed for this check:

```powershell
py scripts/verify_modbus.py --self-test
```

Expected result:

```text
Modbus verifier self-test passed
```

Display all command options with:

```powershell
py scripts/verify_modbus.py --help
```

## 3. Modbus TCP quick start

Confirm the device has an IPv4 address, Modbus TCP is running, the configured source allowlist permits the test PC, and the configured TCP port is reachable.

Run the safe smoke test, replacing the host, port, and unit when needed:

```powershell
py scripts/verify_modbus.py --transport tcp `
  --host 192.168.1.111 `
  --port 502 `
  --unit 1 `
  --output evidence/modbus-tcp-smoke.json
```

The default TCP values are port `502`, unit `1`, and a two-second timeout. A custom timeout can be supplied with `--timeout 5`.

Optional connectivity check before running the verifier:

```powershell
Test-NetConnection 192.168.1.111 -Port 502
```

## 4. Modbus RTU quick start

RTU requires `pyserial`:

```powershell
py -m pip install pyserial
```

List detected serial ports if the COM number is unknown:

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name
```

Close any serial monitor or Modbus program using the adapter, then run the smoke test. This example uses `COM4`, unit `1`, and the default actuator profile of 19200 8E1:

```powershell
py scripts/verify_modbus.py --transport rtu `
  --serial-port COM4 `
  --unit 1 `
  --baud 19200 `
  --parity E `
  --stop-bits 1 `
  --output evidence/modbus-rtu-smoke.json
```

The command-line serial settings must exactly match the device. Supported parity arguments are `E`, `O`, and `N`; stop bits are `1` or `2`.

## 5. Optional relay test

Only perform this step with an isolated test load and a known safe relay state. Add `--write-channel` followed by a channel from 1 through 6:

```powershell
py scripts/verify_modbus.py --transport tcp `
  --host 192.168.1.111 `
  --unit 1 `
  --write-channel 1 `
  --output evidence/modbus-tcp-channel-1.json
```

The tool reads the original coil state, requests the opposite state, checks the coil and applied discrete input, and requests restoration in a `finally` block. Confirm the physical contact changes and returns to its original state. Software restoration is not a substitute for electrical isolation or an emergency disconnect.

The selected channel must be enabled, unblocked, and allowed by the configured Modbus channel/write policy. A rejected write is expected when those conditions are not met.

### All-channel FAT/SAT sequence

Use the separate [Modbus TCP relay FAT/SAT plan](modbus-tcp-relay-fat-sat.md) for prerequisites, operator responsibilities, instrumented physical checks, acceptance criteria, and recovery. This sequence briefly energizes each channel and must only be run with isolated safe loads, independent contact observation as required, and an emergency disconnect.

```powershell
py scripts/verify_modbus.py --transport tcp `
  --host 192.168.1.111 --port 502 --unit 1 --timeout 5 `
  --exercise-all-relays --toggle-cycles 3 `
  --toggle-on-seconds 1 --toggle-off-seconds 1 `
  --on-seconds 30 --off-seconds 5 --confirm-safe-loads `
  --output evidence/modbus-tcp-relay-fat-sat.json
```

The runner defaults to three 1-second ON/OFF toggle cycles, followed by a
30-second ON dwell and 5-second OFF dwell per channel. It checks all six coil
and output states before and during each step, aborts at the first failure,
and attempts to restore the channel under test OFF. `--confirm-safe-loads` is
an operator declaration, not an electrical or firmware interlock. The JSON
report does not prove physical contact movement; capture that separately.

For RTU using COM22 and a device configured at unit 10, 19200 8E1, follow the
[Modbus RTU relay FAT/SAT plan](modbus-rtu-relay-fat-sat.md) and use:

```powershell
& .venv\Scripts\python.exe scripts\verify_modbus.py --transport rtu `
  --serial-port COM22 --unit 10 --baud 19200 --parity E --stop-bits 1 `
  --timeout 5 --exercise-all-relays --toggle-cycles 3 `
  --toggle-on-seconds 1 --toggle-off-seconds 1 `
  --on-seconds 30 --off-seconds 5 --confirm-safe-loads `
  --prepare-off-channel 4 `
  --output evidence\modbus-rtu-relay-fat-sat.json
```

RTU additionally checks FC08 diagnostics echo, selected unit/serial-profile
match and the frame-error counter before/after the sequence. Repeat
`--prepare-off-channel N` only for initially-ON channels the operator has
explicitly authorized to turn OFF; omit it when all six states already read
OFF. Mismatched or unreadable states always fail closed, as do other ON
channels that were not individually authorized.

### RGB indicator and physical-button test

Use the transport-specific FAT/SAT plan before running this optional test. It
requires RGB and button actions enabled, double-click mapped to identify,
nonzero manual RGB brightness, all relay coils/outputs OFF, an observer able to
see the LED, and the installer permission **Allow manual indicators and
diagnostic commands**. The runner invokes identify through the FC16 mailbox,
then waits for one brief physical double-click and verifies the gesture
counter, last-gesture value, and identify RGB output over Modbus. It never
changes button bindings or energizes relay coils. Do not hold the BOOT button:
reset-hold gestures are not tested.

RTU on the verified COM22/unit 10/19200 8E1 profile:

```powershell
& .venv\Scripts\python.exe scripts\verify_modbus.py --transport rtu `
  --serial-port COM22 --unit 10 --baud 19200 --parity E --stop-bits 1 `
  --timeout 5 --exercise-device-io --confirm-indicator-observation `
  --button-timeout-seconds 60 `
  --output evidence\modbus-rtu-device-io-fat-sat.json
```

TCP (substitute the configured address and unit):

```powershell
py scripts/verify_modbus.py --transport tcp `
  --host 192.168.1.111 --port 502 --unit 1 --timeout 5 `
  --exercise-device-io --confirm-indicator-observation `
  --button-timeout-seconds 60 `
  --output evidence/modbus-tcp-device-io-fat-sat.json
```

The confirmation flag records that an observer is present; it does not
measure emitted light. The operator must record whether white illumination was
actually visible. If the mailbox is denied, enable the installer permission
through the authenticated UI and rerun; do not bypass the device's access
policy.

## 6. Understand the report

The tool prints JSON and writes the same JSON to `--output` when supplied. Each item under `checks` contains:

- `name`: check identifier.
- `passed`: `true` or `false`.
- `detail`: returned values or the failure reason.

A normal run checks:

- FC43/14 vendor, product code, and firmware identity.
- Register-map version and capability/status registers.
- Six relay coils and six applied-output discrete inputs.
- Transport status and configured transport values.
- Exception 02 for an unmapped coil address.
- Exception 03 for an invalid FC05 value, without changing a relay.
- FC08 diagnostics echo when using RTU.
- Relay state change and restoration only when `--write-channel` is present.
- Modbus identify and physical double-click monitoring only when
  `--exercise-device-io` is present.

Interpret the process result in PowerShell with:

```powershell
$LASTEXITCODE
```

| Exit code | Meaning |
| --- | --- |
| `0` | All requested checks passed |
| `1` | One or more device checks failed, or an unexpected runtime error occurred |
| `2` | The verifier rejected its setup, such as a missing host/serial port or missing `pyserial` |

Keep the JSON report together with the candidate firmware hash, device configuration, board identity, and test date.

## 7. Common failures

### TCP connection refused or timed out

- Confirm the device status shows Modbus TCP running rather than waiting for network.
- Confirm the IP address and configured port.
- Check the exact IPv4 allowlist and selected network interface.
- Confirm the PC firewall/network permits the connection.
- Ensure another service is not using the configured device port.

### RTU request timed out

- Confirm Modbus RTU is selected and RS485 is enabled/initialized.
- Match unit, baud, parity, and stop bits exactly.
- Swap A/B if the adapter labels use the opposite convention.
- Check termination, reference conductor, adapter driver, and receive indicators.
- Close other applications holding the COM port.

### Modbus exception 02

The address is unmapped or the requested object is inaccessible, disabled, blocked, or denied by policy. Compare the request with [the Modbus map](actuator-modbus-map.md) and the device configuration.

### Modbus exception 03

The value, quantity, or encoding is invalid. The verifier intentionally expects exception 03 for its invalid FC05 negative test.

### Identity or map-version failure

Confirm the target is the intended Edge S3 Relay 6CH running the candidate firmware. Save the JSON report and device firmware/version information before changing anything.

## 8. Minimum revisit checklist

```text
[ ] Candidate firmware hash and board identity recorded
[ ] Correct Modbus mode selected and running
[ ] Unit and TCP/serial settings recorded
[ ] Verifier self-test passed
[ ] Safe smoke test exited 0 and JSON evidence was saved
[ ] Optional relay test used an isolated load and restored the original state
[ ] Core reads/writes repeated with an independent Modbus master
[ ] Remaining P0 cases completed from the full qualification plan
```