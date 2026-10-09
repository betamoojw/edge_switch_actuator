"""Recover actuator setup credentials over USB; optionally verify saved settings.

Python 3.10+. Requires pyserial; --reset also requires esptool 5.x.
Passwords are masked unless --show-secrets is supplied. --label-png explicitly
writes a secret-bearing factory setup label. Customized web passwords are hashed
and cannot be recovered by this tool.
"""

from __future__ import annotations

import argparse
import copy
import getpass
import http.client
import json
import math
import io
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from urllib import error, parse, request


class CredentialError(Exception):
    """An actionable error safe to print without raw device data."""


class HttpFailure(CredentialError):
    def __init__(self, status: int):
        self.status = status
        super().__init__(f"Device HTTP request returned status {status}.")


def mac(value: str) -> str:
    value = value.replace(":", "").replace("-", "").lower()
    if not re.fullmatch(r"[0-9a-f]{12}", value):
        raise ValueError("Expected a 12-digit hexadecimal MAC address.")
    return value


def positive_timeout(value: str) -> float:
    number = float(value)
    if not math.isfinite(number) or not 1 <= number <= 300:
        raise argparse.ArgumentTypeError("Timeout must be between 1 and 300 seconds.")
    return number


def device_url(value: str) -> str:
    try:
        url = parse.urlsplit(value)
        port = url.port
    except ValueError as exc:
        raise argparse.ArgumentTypeError("Invalid device URL.") from exc
    if (url.scheme not in ("http", "https") or not url.hostname
            or url.username is not None or url.password is not None
            or url.path not in ("", "/") or url.query or url.fragment
            or any(c.isspace() or ord(c) < 32 for c in value)
            or (port is not None and port == 0)):
        raise argparse.ArgumentTypeError("Use an http(s) device origin without credentials, path or query.")
    return value.rstrip("/")


def serial_modules():
    try:
        import serial
        from serial.tools import list_ports
        return serial, list_ports
    except ImportError as exc:
        raise CredentialError("Install dependencies: python -m pip install -r scripts/requirements-credentials.txt") from exc


def select_port(ports, name: str, expected_mac: str):
    selected = [p for p in ports if p.device.casefold() == name.casefold()]
    if len(selected) != 1:
        raise CredentialError("Selected port is unavailable. Run --list-ports.")
    port = selected[0]
    try:
        identity = mac(port.serial_number or "")
    except ValueError as exc:
        raise CredentialError("USB serial number does not contain a MAC; this tool requires native ESP32-S3 USB.") from exc
    if (port.vid, port.pid) != (0x303A, 0x1001) or identity != expected_mac:
        raise CredentialError("USB device identity mismatch; no port opened or reset requested.")
    return port


class SetupParser:
    """Bounded rolling parser; raw serial logs are never printed or retained."""

    pattern = re.compile(rb"Device setup password \(admin and factory AP\): ([0-9a-f]{24})\r?\n")

    def __init__(self):
        self.buffer = b""

    def feed(self, chunk: bytes) -> str | None:
        self.buffer = (self.buffer + chunk)[-8192:]
        found = self.pattern.search(self.buffer)
        return found[1].decode("ascii") if found else None


def reset_device(port: str) -> None:
    # flash-id is read-only; esptool enters the bootloader and resets into the app.
    # Never use a shell or return raw subprocess output (it may contain secrets).
    try:
        result = subprocess.run(
            [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", port, "flash-id"],
            capture_output=True, timeout=30, check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise CredentialError("USB restart failed or timed out. Check the port and close serial monitors.") from exc
    if result.returncode:
        raise CredentialError("USB restart failed. Install esptool 5.x and close other serial monitors.")


def recover_setup(serial, port: str, timeout: float) -> str:
    reader = serial.Serial(port=None, baudrate=115200, timeout=0.25)
    try:
        # Configure before open to avoid deliberately asserting boot/reset lines.
        # Some OS/drivers still glitch modem lines on open/close.
        reader.dtr = False
        reader.rts = False
        reader.port = port
        reader.open()
        parser = SetupParser()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            password = parser.feed(reader.read(1024))
            if password:
                return password
    except (serial.SerialException, OSError) as exc:
        raise CredentialError("Could not read the USB port. Check connection and close serial monitors.") from exc
    finally:
        reader.close()
    raise CredentialError("No setup-password line received. Restart the board manually while listening, or use --reset. Firmware must print the setup line.")


class NoRedirect(request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


class DeviceApi:
    def __init__(self, origin: str, timeout: float):
        self.origin = origin
        self.timeout = timeout
        # Avoid ambient proxies and cross-host redirect credential forwarding.
        self.opener = request.build_opener(request.ProxyHandler({}), NoRedirect())

    def wait_ready(self, seconds: float) -> None:
        """Allow boot/network startup without repeating a credential-bearing login."""
        deadline = time.monotonic() + seconds
        original_timeout = self.timeout
        try:
            while time.monotonic() < deadline:
                self.timeout = min(original_timeout, max(0.1, deadline - time.monotonic()))
                try:
                    features = self.call("/rest/features")
                except HttpFailure:
                    raise
                except CredentialError:
                    time.sleep(min(0.5, max(0, deadline - time.monotonic())))
                    continue
                if features.get("firmware_built_target") != "waveshare-relay-6ch":
                    raise CredentialError("URL does not report the waveshare-relay-6ch firmware target; no credentials sent.")
                return
        finally:
            self.timeout = original_timeout
        raise CredentialError("Device did not become reachable before the network deadline.")

    def call(self, path: str, token: str | None = None, body: dict | None = None) -> dict:
        headers = {"Content-Type": "application/json"}
        if token:
            headers["Authorization"] = f"Bearer {token}"
        req = request.Request(self.origin + path, headers=headers,
                              data=None if body is None else json.dumps(body).encode())
        try:
            with self.opener.open(req, timeout=self.timeout) as response:
                raw = response.read(65537)
                if len(raw) > 65536:
                    raise CredentialError("Device response exceeded the 64 KiB limit.")
            result = json.loads(raw)
            if not isinstance(result, dict):
                raise CredentialError("Device returned an unexpected JSON structure.")
            return result
        except error.HTTPError as exc:
            exc.close()
            raise HttpFailure(exc.code) from None
        except (error.URLError, OSError, ValueError, http.client.HTTPException) as exc:
            raise CredentialError("Device unreachable, TLS verification failed, or response was invalid.") from exc


def verify_current(api, expected_mac: str, username: str, password: str) -> dict:
    auth = api.call("/rest/signIn", body={"username": username, "password": password})
    token = auth.get("access_token")
    if not isinstance(token, str) or not token or len(token) > 8192:
        raise CredentialError("Device login response contained no valid access token.")
    wifi = api.call("/rest/wifiStatus", token)
    identity = wifi.get("mac_address")
    if identity is not None:
        try:
            matches = mac(identity) == expected_mac
        except (ValueError, AttributeError):
            matches = False
        if not matches:
            raise CredentialError("Network device MAC differs from selected USB device; saved settings not read.")
    ap = api.call("/rest/apSettings", token)
    if not isinstance(ap.get("ssid"), str) or not isinstance(ap.get("password"), str):
        raise CredentialError("Device AP settings response is incomplete.")
    return {
        "source": "authenticated_device_api", "url": api.origin,
        "identity": "matched_station_mac" if identity else "unavailable_station_offline",
        "ap": {"ssid": ap["ssid"], "password": ap["password"],
               "ip": ap.get("local_ip"), "provision_mode": ap.get("provision_mode")},
        "web": {"username": username, "password": password, "login_verified": True},
    }


def render(report: dict, show_secrets: bool) -> str:
    result = copy.deepcopy(report)
    def redact(value):
        if isinstance(value, dict):
            for key in value:
                if key == "password":
                    value[key] = "[REDACTED]"
                else:
                    redact(value[key])
    if not show_secrets:
        redact(result)
    return json.dumps(result, indent=2, ensure_ascii=True)


def label_modules():
    try:
        import qrcode
        from PIL import Image, ImageDraw, ImageFont
        return qrcode, Image, ImageDraw, ImageFont
    except ImportError as exc:
        raise CredentialError("PNG labels require qrcode and Pillow; install scripts/requirements-credentials.txt.") from exc


def wifi_payload(ssid: str, password: str) -> str:
    """ZXing's de facto Wi-Fi onboarding syntax; WPA covers WPA/WPA2 PSK."""
    def escape(value):
        return re.sub(r'([\\;,:\"])', r'\\\1', value)
    return f"WIFI:T:WPA;S:{escape(ssid)};P:{escape(password)};;"


def check_label_path(destination: Path) -> None:
    if destination.suffix.lower() != ".png":
        raise CredentialError("Label output must have a .png extension.")
    if destination.exists() or destination.is_symlink():
        raise CredentialError("Label output already exists; choose a new filename.")


def generate_label(report: dict, destination: Path, dpi: int = 300) -> dict:
    """Render fixed 100 x 70 mm factory label, never verified/current credentials.

    Integer QR modules, Q error correction, and a four-module quiet zone.
    A PNG cannot certify ISO/IEC print quality or a specific printer/substrate.
    """
    if dpi not in (203, 300, 600):
        raise CredentialError("Label DPI must be 203, 300 or 600.")
    check_label_path(destination)
    qrcode, Image, ImageDraw, ImageFont = label_modules()
    try:
        identity = mac(report["device_mac"])
        factory = report["factory_setup"]
        ap, web = factory["ap"], factory["web"]
        ssid, password = ap["ssid"], ap["password"]
        username, web_password = web["username"], web["password"]
        # This is the repository's factory profile, not a general label editor.
        if (ssid != f"ESP32-SvelteKit-{identity}" or ap["ip"] != "192.168.4.1"
                or username != "admin" or not re.fullmatch(r"[0-9a-f]{24}", password)
                or web_password != password):
            raise ValueError("Not the factory profile")
    except (KeyError, TypeError, ValueError) as exc:
        raise CredentialError("Label requires an unmodified actuator factory-setup report.") from exc
    px = lambda mm: round(mm * dpi / 25.4)
    canvas = Image.new("L", (px(100), px(70)), 255)
    draw = ImageDraw.Draw(canvas)

    def text(x, y, value, points, width, fill=0, bold=False):
        font = ImageFont.load_default(size=round(points * dpi / 72))
        if draw.textlength(value, font=font) > px(width):
            raise CredentialError("Label text exceeds the fixed print area.")
        draw.text((px(x), px(y)), value, font=font, fill=fill, anchor="lt",
                  stroke_width=max(1, round(dpi / 300)) if bold else 0)

    # Monochrome hierarchy stays crisp on thermal printers as well as laser stock.
    draw.rounded_rectangle((px(4), px(4), px(96), px(17)), radius=px(2), fill=0)
    text(7, 6, "DEVICE SETUP", 12, 70, fill=255, bold=True)
    text(7, 12, "Waveshare ESP32-S3 / Relay 6CH", 7.5, 70, fill=255)
    draw.rounded_rectangle((px(82), px(7), px(93), px(14)), radius=px(1), outline=255,
                           width=max(1, px(.2)))
    text(84, 9, "6CH", 9, 8, fill=255)
    pretty_mac = ":".join(identity[i:i + 2] for i in range(0, 12, 2)).upper()
    text(4, 19, f"MAC  {pretty_mac}", 8, 57)
    text(74, 19, "FACTORY DEFAULTS", 6.5, 23)
    text(4, 27, "01  CONNECT TO WI-FI", 8, 54, bold=True)
    text(4, 32, ssid, 8, 54)
    draw.rounded_rectangle((px(4), px(39), px(59), px(51)), radius=px(1.5),
                           outline=0, width=max(1, px(.2)))
    text(6, 41, "WI-FI + WEB PASSWORD", 7, 51)
    text(6, 45, password, 9, 51)
    text(4, 55, f"02  WEB LOGIN / {username}", 8, 54, bold=True)
    text(4, 60, "http://192.168.4.1", 10, 54)

    qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_Q, border=4, box_size=1)
    qr.add_data(wifi_payload(ssid, password), optimize=0)
    qr.make(fit=True)
    matrix = qr.get_matrix()  # Includes the four-module quiet zone.
    module_pixels = px(33) // len(matrix)
    if module_pixels < 3 or module_pixels * 25.4 / dpi < .33:
        raise CredentialError("QR modules would be too small for the label.")
    size = len(matrix) * module_pixels
    qr_image = Image.new("L", (size, size), 255)
    qr_draw = ImageDraw.Draw(qr_image)
    for row, values in enumerate(matrix):
        for col, dark in enumerate(values):
            if dark:
                x, y = col * module_pixels, row * module_pixels
                qr_draw.rectangle((x, y, x + module_pixels - 1, y + module_pixels - 1), fill=0)
    text(66, 26, "SCAN TO CONNECT", 7.5, 30, bold=True)
    left = px(63) + (px(33) - size) // 2
    top = px(31)
    canvas.paste(qr_image, (left, top))
    text(4, 67, "PRIVATE SETUP LABEL  /  Saved settings may differ.", 6.5, 92)
    # Binary raster prevents gray QR edges or unintended dithering.
    raster = canvas.point(lambda value: 0 if value < 128 else 255, mode="1")
    encoded = io.BytesIO()
    raster.save(encoded, format="PNG", dpi=(dpi, dpi))
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        # Exclusive creation refuses races/overwrites; POSIX permissions are 0600.
        # On Windows the destination directory's ACL controls file access.
        descriptor = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError as exc:
        raise CredentialError("Label output already exists; choose a new filename.") from exc
    try:
        with os.fdopen(descriptor, "wb") as output:
            output.write(encoded.getvalue())
    except BaseException:
        destination.unlink(missing_ok=True)
        raise
    return {"path": str(destination.resolve()), "dpi": dpi, "width_mm": 100,
            "height_mm": 70, "contains_secrets": True, "source": "factory_setup",
            "qr_format": "ZXing WIFI", "error_correction": "Q", "quiet_zone_modules": 4}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list-ports", action="store_true")
    parser.add_argument("--port", help="Explicit USB port, e.g. COM21 or /dev/ttyACM0")
    parser.add_argument("--expect-mac", type=mac, help="Required USB MAC guard")
    parser.add_argument("--reset", action="store_true", help="Restart selected device; interrupts control and applies startup policy")
    parser.add_argument("--timeout", type=positive_timeout, default=60.0, help="Serial capture seconds (default: 60)")
    parser.add_argument("--device-url", type=device_url, help="Explicit origin for optional verification; HTTP sends credentials in cleartext")
    parser.add_argument("--http-timeout", type=positive_timeout, default=8.0)
    parser.add_argument("--network-wait", type=positive_timeout, default=30.0, help="Wait for network startup before one login attempt (seconds)")
    parser.add_argument("--username", default="admin", help="Username for optional API verification")
    parser.add_argument("--ask-password", action="store_true", help="Prompt for a known current password instead of testing the setup password")
    parser.add_argument("--show-secrets", action="store_true", help="Include passwords in stdout JSON; otherwise masked")
    parser.add_argument("--label-png", type=Path, help="Write a secret-bearing 100 x 70 mm factory setup label; refuses overwrite")
    parser.add_argument("--label-dpi", type=int, choices=(203, 300, 600), default=300)
    args = parser.parse_args(argv)
    if not args.list_ports and (not args.port or not args.expect_mac):
        parser.error("--port and --expect-mac are required (or use --list-ports)")
    if args.ask_password and not args.device_url:
        parser.error("--ask-password requires --device-url")
    if args.list_ports and args.label_png:
        parser.error("--label-png requires credential capture, not --list-ports")
    if args.label_png:
        check_label_path(args.label_png)
        label_modules()  # Dependency/path failures must precede a requested reset.
    serial, ports = serial_modules()
    if args.list_ports:
        print(json.dumps([{"port": p.device, "description": p.description,
                           "usb_serial": p.serial_number, "vid": p.vid, "pid": p.pid}
                          for p in ports.comports()], indent=2))
        return 0
    select_port(ports.comports(), args.port, args.expect_mac)
    if args.reset:
        print("Restarting selected device; existing control will be interrupted.", file=sys.stderr)
        reset_device(args.port)
    print("Listening for setup credentials; no raw serial log will be saved.", file=sys.stderr)
    password = recover_setup(serial, args.port, args.timeout)
    report = {
        "schema_version": 1, "port": args.port, "device_mac": args.expect_mac,
        "factory_setup": {
            "source": "device_startup_password_and_repository_defaults",
            "current_settings_verified": False,
            "ap": {"ssid": f"ESP32-SvelteKit-{args.expect_mac}", "password": password, "ip": "192.168.4.1"},
            "web": {"username": "admin", "password": password},
        },
        "current": None,
        "note": "Saved settings may differ. Customized web passwords cannot be recovered from their hashes.",
    }
    exit_code = 0
    if args.label_png:
        report["label"] = generate_label(report, args.label_png, args.label_dpi)
    if args.device_url:
        try:
            candidate = getpass.getpass("Current web password: ") if args.ask_password else password
            api = DeviceApi(args.device_url, args.http_timeout)
            api.wait_ready(args.network_wait)
            report["current"] = verify_current(api, args.expect_mac, args.username, candidate)
        except CredentialError as exc:
            report["verification_error"] = str(exc)
            exit_code = 3
    print(render(report, args.show_secrets))
    return exit_code


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except CredentialError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
    except OSError:
        print("ERROR: Operating-system I/O failed. Check USB access and output destination.", file=sys.stderr)
        raise SystemExit(1)
    except (KeyboardInterrupt, EOFError):
        print("Cancelled.", file=sys.stderr)
        raise SystemExit(130)
