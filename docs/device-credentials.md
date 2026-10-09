# Device credentials and QR labels — quick start

`scripts/get_device_credentials.py` reads the setup-password line printed by this project's actuator firmware over native ESP32-S3 USB. It optionally verifies a web login and retrieves the saved AP Wi-Fi credentials through the authenticated API. It does not flash firmware, erase settings, operate relays, guess passwords or recover customized web passwords from their hashes.

## Quick start: USB device to printable label

Run these commands from the repository root. Python 3.10+ is required; close any serial monitor first.

**1. Install dependencies and find your board.**

```powershell
python -m pip install -r scripts/requirements-credentials.txt
python scripts/get_device_credentials.py --list-ports
```

Copy the device's `port` and `usb_serial` from the JSON output. The following example uses COM21 and MAC CC:BA:97:34:CA:20; replace both for a different board.

**2. Recover credentials and generate a label.**

```powershell
python scripts/get_device_credentials.py `
  --port COM21 `
  --expect-mac CC:BA:97:34:CA:20 `
  --reset `
  --label-png .pio/labels/ccba9734ca20-setup.png `
  --label-dpi 300
```

This restarts the selected board, reads its factory setup password, and creates a **100 × 70 mm PNG**. Restart interrupts device control and applies its startup policy. The label contains the password, while terminal JSON stays masked. Add `--show-secrets` only when you also want the password displayed in the terminal. Existing output files are refused: choose a new filename for each export.

Successful output includes `label.path`, `label.dpi: 300`, `label.contains_secrets: true`, and exit code **0**. The modern monochrome layout has a device header, MAC identity, numbered connection/login steps, a separate password box and a high-contrast QR code.

**3. Print and connect.**

- Open the generated PNG and print on **100 × 70 mm** media at **100% / actual size**. Disable fit-to-page.
- Match `--label-dpi` to the printer: **203**, **300**, or **600**. Generating a PNG does not send it to a printer.
- With the device AP active, scan the QR to join Wi-Fi. Open **http://192.168.4.1**, then sign in as **admin** using the printed shared Wi-Fi/web password.
- Keep the label private. These are factory defaults; changed AP settings or web credentials can differ.

For this workspace's existing Python, replace `python` in any command with `& .pio/network-platformio/penv/Scripts/python.exe`.

### Common commands

| Task | Arguments after `python scripts/get_device_credentials.py` |
| --- | --- |
| Find USB devices | `--list-ports` |
| Show factory credentials | `--port COM21 --expect-mac CC:BA:97:34:CA:20 --reset --show-secrets` |
| Generate a thermal-printer label | `--port COM21 --expect-mac CC:BA:97:34:CA:20 --reset --label-png .pio/labels/setup-203.png --label-dpi 203` |
| Wait for a manual restart | `--port COM21 --expect-mac CC:BA:97:34:CA:20 --timeout 120 --show-secrets` |
| Verify live AP/login settings | `--port COM21 --expect-mac CC:BA:97:34:CA:20 --reset --device-url http://192.168.1.102 --show-secrets` |
| Verify a known customized login | Add `--username admin --ask-password` to the live-verification command |
| View all options | `--help` |

### Troubleshooting

| Message or symptom | Action |
| --- | --- |
| Port unavailable or identity mismatch | Run `--list-ports` again; use the matching port and MAC. |
| No setup-password line | Close other serial tools; use `--reset` or restart manually while listening. Try `--timeout 120`. |
| PNG dependency missing | Reinstall `scripts/requirements-credentials.txt` with the same Python interpreter. |
| Label output already exists | Pick a new `.png` filename; the tool never overwrites labels. |
| Exit code 3 / current settings unavailable | Factory capture succeeded. Check the explicit URL, network access and login; an already generated label remains available. |
| QR scans but does not connect | Confirm the device AP is broadcasting and saved Wi-Fi settings still match the defaults. |
| Printed QR does not scan | Print at actual size and matching DPI; preserve white QR margins and verify printer/media quality. |

The remaining sections explain credential provenance, optional verification, output handling and validation in detail.

## Install and identify

Use Python 3.10 or newer:

```powershell
python -m pip install -r scripts/requirements-credentials.txt
python scripts/get_device_credentials.py --list-ports
```

For this repository's existing PlatformIO Python, replace `python` with:

```powershell
& .pio/network-platformio/penv/Scripts/python.exe
```

Both `--port` and `--expect-mac` are mandatory for capture. Native USB must report VID/PID `303A:1001` and the matching MAC serial number. USB-to-UART adapters without that identity are intentionally unsupported. Never select a different board merely because a COM number was reused.

## Read factory setup credentials

For the second board used during development:

```powershell
python scripts/get_device_credentials.py --port COM21 --expect-mac CC:BA:97:34:CA:20 --reset --show-secrets
```

`--reset` explicitly permits restarting that board. Restart interrupts control and applies its configured startup policy. The command uses esptool's read-only `flash-id` operation, followed by the normal application reset; no flash writes occur. Without `--reset`, the tool only opens the port and waits up to 60 seconds: manually restart the board while it is listening. DTR/RTS are deasserted before opening, but some OS/drivers can still pulse reset lines on serial open/close.

The firmware must print `Device setup password (admin and factory AP): …`. Capture is bounded, handles fragmented serial data, and never prints or saves raw serial logs. Close serial monitors before running. `--timeout 120` allows more time for startup. If USB re-enumerates to a different port, list ports again and rerun with its matching identity.

The JSON `factory_setup` section contains the password read from the board, the repository-default AP name, AP address and administrator username. These are **setup defaults**, not proof of the currently saved settings. The setup identity survives a factory reset. Previously customized AP settings and administrator credentials survive ordinary firmware uploads.

Passwords are masked unless `--show-secrets` is supplied. With that flag, stdout contains credentials: do not capture it in shared CI logs or commit redirected output. The tool writes a credential-bearing image only when `--label-png` is explicitly supplied, and never prints authentication tokens. No credentials or real passwords are embedded in the script or tests.

## Print-ready PNG setup labels

```powershell
python scripts/get_device_credentials.py --port COM21 --expect-mac CC:BA:97:34:CA:20 --reset --label-png .pio/labels/ccba9734ca20.png
```

The image contains the actual factory setup password, even when stdout is masked. `--label-png` is explicit permission to save those credentials. Existing files are never overwritten, including symlinks. Parent directories are created as needed. POSIX creation uses mode 0600; Windows uses the destination directory's inherited ACL. Keep these commissioning labels with controlled installation materials rather than publicly accessible equipment surfaces. `.pio/` is already excluded from Git.

The black-and-white PNG is **100 × 70 mm**, default **300 DPI** (1181 × 827 pixels). Select `--label-dpi 203` or `--label-dpi 600` to match the printer. Print at **100% / actual size**, disable fit-to-page, and set label media to 100 × 70 mm. The script generates a PNG; it does not submit a job to a printer.

The label shows model, MAC, AP SSID, shared factory Wi-Fi/web password, administrator username and setup URL. It explicitly marks these as setup defaults, which may differ from saved settings. It never substitutes verified/current account credentials into the factory label. The QR encodes only the AP SSID and password using `WIFI:T:WPA;S:…;P:…;;`; scan it to join the AP, then open the printed setup URL and log in. AP broadcasting must be available for onboarding.

### Standards and print quality

There is no universal industrial IoT credential-label layout asserted by this tool. The implementation uses:

- [ZXing's Wi-Fi onboarding content convention](https://github.com/zxing/zxing/wiki/Barcode-Contents), with escaping for special characters.
- QR Code Model 2 generation through `qrcode`, Q error correction, square modules, black on white and an unobstructed **four-module quiet zone**, following [DENSO WAVE's QR code area guidance](https://www.qrcode.com/en/howto/code.html).
- Integer pixels per module with no image resampling; a minimum 0.33 mm module size and at least three pixels per module for the supported resolutions.

These engineering choices do not constitute ISO/IEC 15415 print-quality certification, GS1 identification, Matter certification or Wi-Fi Easy Connect/DPP support. [GS1 Digital Link](https://www.gs1.org/standards/gs1-digital-link) concerns GS1 identifiers and links; no GTIN or registered GS1 identity was supplied, so none is invented. The QR is a commissioning Wi-Fi code, not a product-traceability identifier. Qualify the actual printer, stock, contrast, durability and scanner combination before production rollout; PNG decode tests cannot establish physical print quality.

## Verify the current web login and AP settings

When the PC can reach the board, specify its origin explicitly:

```powershell
python scripts/get_device_credentials.py --port COM21 --expect-mac CC:BA:97:34:CA:20 --reset --device-url http://192.168.1.102 --show-secrets
```

The tool waits up to 30 seconds for `/rest/features` to report the Waveshare firmware target (`--network-wait` adjusts this). Only one login attempt is made, using the recovered setup password. After login, the tool compares the station MAC when available, then reads `/rest/apSettings`. A mismatch stops retrieval. If the station is offline (for example, AP-only provisioning), its API omits the MAC: `current.identity` explicitly reports that it could not verify network identity. The supplied URL must therefore belong to the intended device. Reported AP settings do not prove the AP is currently broadcasting.

Use HTTPS when supported. The firmware normally serves HTTP, which transmits the password and token unencrypted on the local network. HTTPS certificate verification remains enabled. HTTP redirects and environment-configured proxies are disabled; network addresses printed in serial output are never followed automatically. The tool does not change the PC's Wi-Fi network.

If the saved administrator password is different, it cannot be recovered from the stored PBKDF2 hash. To verify a password you already know and read the actual AP password, use a hidden prompt:

```powershell
python scripts/get_device_credentials.py --port COM21 --expect-mac CC:BA:97:34:CA:20 --reset --device-url http://192.168.4.1 --username admin --ask-password --show-secrets
```

The `current` section is populated only after successful login and AP retrieval. A 401 means the supplied login was rejected; a 403 can mean the account lacks administrator permission. On failure, the report preserves factory setup results and describes verification failure without raw server bodies. No reset-to-factory fallback is performed.

## Exit codes and validation

| Code | Meaning |
| --- | --- |
| 0 | Requested operation completed; without a URL this verifies only setup-password capture |
| 1 | USB/dependency/I/O/capture failure |
| 2 | Invalid command-line arguments |
| 3 | Setup credentials recovered, but requested API verification failed |
| 130 | User cancelled or password prompt received EOF |

Run the hardware-independent tests:

```powershell
python -m unittest discover -s tests -p test_device_credentials.py -v
```

Install `python -m pip install zxing-cpp==2.3.0` in addition to the runtime requirements for the independent QR decode tests. Tests cover fragmented/malformed serial input, memory bounds, device mismatch, cleanup/deadlines, reset opt-in, redaction, partial failure, network identity, malformed/oversized responses, redirect policy, PNG dimensions/DPI, independently decoded QR payloads at all three print resolutions, and overwrite prevention. No device reset or network access is required by the unit tests. Actual startup password recovery was performed manually on COM21 before creating the tool; the new script's reset-and-capture flow requires a separate hardware run before claiming end-to-end qualification.
