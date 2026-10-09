"""Credential-tool failure-path tests; no hardware, credentials or dependencies needed."""
import importlib.util
import argparse
import io
import json
from pathlib import Path
from types import SimpleNamespace
import unittest
import tempfile
from unittest.mock import patch
from urllib.error import HTTPError

SPEC = importlib.util.spec_from_file_location(
    "credentials", Path(__file__).resolve().parents[1] / "scripts/get_device_credentials.py")
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)
MAC = "001122334455"
SECRET = "0123456789abcdef01234567"  # synthetic fixture


class CredentialTests(unittest.TestCase):
    def test_fragmented_serial_and_unrelated_logs(self):
        parser = tool.SetupParser()
        line = f"Device setup password (admin and factory AP): {SECRET}\r\n".encode()
        self.assertIsNone(parser.feed(b"unrelated secret=do-not-print\n"))
        for byte in line[:-1]:
            self.assertIsNone(parser.feed(bytes([byte])))
        self.assertEqual(parser.feed(line[-1:]), SECRET)

    def test_malformed_secret_rejected_and_buffer_bounded(self):
        parser = tool.SetupParser()
        self.assertIsNone(parser.feed(b"x" * 50000))
        self.assertEqual(len(parser.buffer), 8192)
        self.assertIsNone(parser.feed(f"Device setup password (admin and factory AP): {SECRET}f\n".encode()))

    def test_exact_usb_selection(self):
        port = SimpleNamespace(device="COM21", serial_number="00:11:22:33:44:55", vid=0x303A, pid=0x1001)
        self.assertIs(tool.select_port([port], "com21", MAC), port)
        for name, identity in [("COM20", MAC), ("COM21", "ffeeddccbbaa")]:
            with self.assertRaises(tool.CredentialError):
                tool.select_port([port], name, identity)
        port.vid = 0
        with self.assertRaises(tool.CredentialError):
            tool.select_port([port], "COM21", MAC)

    def test_serial_cleanup_and_lines_configured_before_open(self):
        class Reader:
            def open(self):
                assert self.dtr is False and self.rts is False
            def read(self, count):
                raise OSError("sensitive driver diagnostic")
            def close(self):
                self.closed = True
        reader = Reader()
        serial = SimpleNamespace(Serial=lambda **kw: reader, SerialException=OSError)
        with self.assertRaises(tool.CredentialError) as raised:
            tool.recover_setup(serial, "COM21", 1)
        self.assertTrue(reader.closed)
        self.assertNotIn("sensitive", str(raised.exception))

    def test_serial_deadline(self):
        reader = SimpleNamespace(open=lambda: None, close=lambda: None)
        serial = SimpleNamespace(Serial=lambda **kw: reader, SerialException=OSError)
        with patch.object(tool.time, "monotonic", side_effect=[0, 2]):
            with self.assertRaisesRegex(tool.CredentialError, "No setup-password"):
                tool.recover_setup(serial, "COM21", 1)

    def test_url_validation(self):
        for url in ["file:///tmp/x", "http://a:pw@host", "http://host/path", "http://host?token=x", "http://host:0", "http://host\n"]:
            with self.subTest(url=url), self.assertRaises(argparse.ArgumentTypeError):
                tool.device_url(url)
        self.assertEqual(tool.device_url("https://device:443/"), "https://device:443")

    def test_timeout_rejects_nonfinite(self):
        for value in ["nan", "inf", "0", "301"]:
            with self.subTest(value=value), self.assertRaises(argparse.ArgumentTypeError):
                tool.positive_timeout(value)

    def test_redaction_does_not_modify_report(self):
        report = {"factory_setup": {"ap": {"password": SECRET}}, "current": {"web": {"password": SECRET}}}
        self.assertNotIn(SECRET, tool.render(report, False))
        self.assertIn(SECRET, tool.render(report, True))

    def test_current_settings_and_password_validation(self):
        class Api:
            origin = "http://device"
            def call(self, route, token=None, body=None):
                return {"/rest/signIn": {"access_token": "fixture-token"},
                        "/rest/wifiStatus": {"mac_address": MAC},
                        "/rest/apSettings": {"ssid": "custom-ap", "password": "custom-pass"}}[route]
        current = tool.verify_current(Api(), MAC, "owner", SECRET)
        self.assertEqual(current["ap"]["ssid"], "custom-ap")
        self.assertEqual(current["ap"]["password"], "custom-pass")
        self.assertTrue(current["web"]["login_verified"])
        self.assertEqual(current["identity"], "matched_station_mac")

    def test_wrong_network_board_never_reads_ap_settings(self):
        calls = []
        class Api:
            def call(self, route, token=None, body=None):
                calls.append(route)
                return {"access_token": "fixture-token"} if body else {"mac_address": "ffeeddccbbaa"}
        with self.assertRaises(tool.CredentialError):
            tool.verify_current(Api(), MAC, "admin", SECRET)
        self.assertNotIn("/rest/apSettings", calls)

    def test_http_error_body_not_disclosed(self):
        api = tool.DeviceApi("http://device", 1)
        response = io.BytesIO(SECRET.encode())
        with patch.object(api.opener, "open", side_effect=HTTPError("http://device", 401, SECRET, {}, response)):
            with self.assertRaises(tool.HttpFailure) as raised:
                api.call("/rest/signIn", body={"password": SECRET})
        self.assertEqual(raised.exception.status, 401)
        self.assertNotIn(SECRET, str(raised.exception))
        self.assertTrue(response.closed)

    def test_redirects_disabled(self):
        self.assertIsNone(tool.NoRedirect().redirect_request(None, None, 302, "", {}, "http://other"))

    def test_oversized_response(self):
        api = tool.DeviceApi("http://device", 1)
        with patch.object(api.opener, "open", return_value=io.BytesIO(b"x" * 65537)):
            with self.assertRaisesRegex(tool.CredentialError, "64 KiB"):
                api.call("/rest/apSettings")

    def test_invalid_json_and_token(self):
        api = tool.DeviceApi("http://device", 1)
        for raw in [b"not JSON", b"[]", b"\xff"]:
            with patch.object(api.opener, "open", return_value=io.BytesIO(raw)):
                with self.assertRaises(tool.CredentialError):
                    api.call("/rest/signIn")
        with self.assertRaises(tool.CredentialError):
            tool.verify_current(SimpleNamespace(call=lambda *a, **kw: {}), MAC, "admin", SECRET)

    def test_network_ready_retries_only_public_endpoint(self):
        api = tool.DeviceApi("http://device", 1)
        with patch.object(api, "call", side_effect=[tool.CredentialError("offline"), {"firmware_built_target": "waveshare-relay-6ch"}]) as call, \
             patch.object(tool.time, "sleep"):
            api.wait_ready(2)
            self.assertEqual([c.args for c in call.call_args_list], [("/rest/features",), ("/rest/features",)])
        self.assertEqual(api.timeout, 1)

    def test_wrong_firmware_rejected_before_login(self):
        api = tool.DeviceApi("http://device", 1)
        with patch.object(api, "call", return_value={"firmware_built_target": "other"}):
            with self.assertRaisesRegex(tool.CredentialError, "no credentials sent"):
                api.wait_ready(2)

    def test_default_run_never_resets(self):
        port = SimpleNamespace(device="COM21", serial_number=MAC, vid=0x303A, pid=0x1001)
        with patch.object(tool, "serial_modules", return_value=(None, SimpleNamespace(comports=lambda: [port]))), \
             patch.object(tool, "recover_setup", return_value=SECRET), \
             patch.object(tool, "reset_device") as reset, \
             patch("sys.stdout", new_callable=io.StringIO) as stdout, \
             patch("sys.stderr", new_callable=io.StringIO):
            self.assertEqual(tool.main(["--port", "COM21", "--expect-mac", MAC]), 0)
            reset.assert_not_called()
            report = json.loads(stdout.getvalue())
            self.assertIsNone(report["current"])
            self.assertNotIn(SECRET, stdout.getvalue())


class LabelTests(unittest.TestCase):
    @staticmethod
    def report():
        return {"device_mac": MAC, "factory_setup": {
            "ap": {"ssid": f"ESP32-SvelteKit-{MAC}", "password": SECRET, "ip": "192.168.4.1"},
            "web": {"username": "admin", "password": SECRET}},
            "current": {"ap": {"password": "different-current-secret"}}}

    def test_wifi_escaping(self):
        self.assertEqual(tool.wifi_payload('a;b:c,d"e\\f', "p;\\"),
                         'WIFI:T:WPA;S:a\\;b\\:c\\,d\\"e\\\\f;P:p\\;\\\\;;')

    def test_png_decodes_at_all_print_resolutions(self):
        from PIL import Image
        import zxingcpp
        with tempfile.TemporaryDirectory() as directory:
            for dpi in (203, 300, 600):
                with self.subTest(dpi=dpi):
                    output = Path(directory) / f"label-{dpi}.png"
                    result = tool.generate_label(self.report(), output, dpi)
                    self.assertTrue(result["contains_secrets"])
                    with Image.open(output) as png:
                        self.assertEqual(png.size, (round(100 * dpi / 25.4), round(70 * dpi / 25.4)))
                        self.assertAlmostEqual(png.info["dpi"][0], dpi, delta=.02)
                        self.assertEqual(png.mode, "1")
                        decoded = zxingcpp.read_barcodes(png.convert("L"))
                        self.assertEqual(len(decoded), 1)
                        self.assertEqual(decoded[0].text, f"WIFI:T:WPA;S:ESP32-SvelteKit-{MAC};P:{SECRET};;")
                        # No credentials in PNG textual metadata.
                        self.assertNotIn("password", str(png.info))

    def test_refuses_existing_file_and_nonfactory_data(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "label.png"
            output.write_bytes(b"keep")
            with self.assertRaises(tool.CredentialError):
                tool.generate_label(self.report(), output)
            self.assertEqual(output.read_bytes(), b"keep")
            output.unlink()
            report = self.report()
            report["factory_setup"]["ap"]["password"] = "[REDACTED]"
            with self.assertRaises(tool.CredentialError):
                tool.generate_label(report, output)
            self.assertFalse(output.exists())

    def test_label_cli_keeps_stdout_masked(self):
        port = SimpleNamespace(device="COM21", serial_number=MAC, vid=0x303A, pid=0x1001)
        with tempfile.TemporaryDirectory() as directory, \
             patch.object(tool, "serial_modules", return_value=(None, SimpleNamespace(comports=lambda: [port]))), \
             patch.object(tool, "recover_setup", return_value=SECRET), \
             patch("sys.stdout", new_callable=io.StringIO) as stdout, \
             patch("sys.stderr", new_callable=io.StringIO):
            output = Path(directory) / "label.png"
            code = tool.main(["--port", "COM21", "--expect-mac", MAC, "--label-png", str(output)])
            self.assertEqual(code, 0)
            self.assertTrue(output.exists())
            self.assertNotIn(SECRET, stdout.getvalue())

    def test_existing_label_fails_before_reset(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(tool, "reset_device") as reset:
            output = Path(directory) / "label.png"
            output.touch()
            with self.assertRaises(tool.CredentialError):
                tool.main(["--port", "COM21", "--expect-mac", MAC, "--reset", "--label-png", str(output)])
            reset.assert_not_called()

    def test_reset_failure_suppresses_raw_diagnostics(self):
        with patch.object(tool.subprocess, "run", return_value=SimpleNamespace(returncode=1, stderr=SECRET)):
            with self.assertRaises(tool.CredentialError) as raised:
                tool.reset_device("COM21")
        self.assertNotIn(SECRET, str(raised.exception))

    def test_failed_login_preserves_factory_result_but_returns_partial_exit(self):
        port = SimpleNamespace(device="COM21", serial_number=MAC, vid=0x303A, pid=0x1001)
        with patch.object(tool, "serial_modules", return_value=(None, SimpleNamespace(comports=lambda: [port]))), \
             patch.object(tool, "recover_setup", return_value=SECRET), \
             patch.object(tool, "verify_current", side_effect=tool.HttpFailure(401)), \
             patch.object(tool.DeviceApi, "wait_ready"), \
             patch("sys.stdout", new_callable=io.StringIO) as stdout, \
             patch("sys.stderr", new_callable=io.StringIO):
            code = tool.main(["--port", "COM21", "--expect-mac", MAC, "--device-url", "http://device"])
            self.assertEqual(code, 3)
            report = json.loads(stdout.getvalue())
            self.assertIsNone(report["current"])
            self.assertFalse(report["factory_setup"]["current_settings_verified"])
            self.assertNotIn(SECRET, stdout.getvalue())


if __name__ == "__main__":
    unittest.main()
