"""Run Modbus protocol checks against an actuator.

TCP uses the Python standard library. RTU additionally requires pyserial.
Relay actuation is disabled unless an actuation option is supplied.
All-relay testing requires an explicit safe-load confirmation.
"""

from __future__ import annotations

import argparse
import json
import socket
import struct
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Protocol
from unittest import mock


class VerificationError(RuntimeError):
    pass


def crc16(data: bytes) -> int:
    value = 0xFFFF
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xA001 if value & 1 else 0)
    return value


def rtu_frame(unit: int, pdu: bytes) -> bytes:
    message = bytes((unit,)) + pdu
    return message + struct.pack("<H", crc16(message))


def parse_rtu_frame(frame: bytes, expected_unit: int) -> bytes:
    if len(frame) < 5:
        raise VerificationError(f"short RTU response ({len(frame)} bytes)")
    if crc16(frame[:-2]) != struct.unpack("<H", frame[-2:])[0]:
        raise VerificationError("RTU response CRC mismatch")
    if frame[0] != expected_unit:
        raise VerificationError(f"unexpected RTU unit {frame[0]}")
    return frame[1:-2]


class Transport(Protocol):
    name: str

    def request(self, pdu: bytes) -> bytes: ...

    def close(self) -> None: ...


class TcpTransport:
    name = "tcp"

    def __init__(self, host: str, port: int, unit: int, timeout: float):
        self.host = host
        self.port = port
        self.unit = unit
        self.timeout = timeout
        self.transaction = 0

    @staticmethod
    def _receive_exact(connection: socket.socket, size: int) -> bytes:
        chunks = bytearray()
        while len(chunks) < size:
            chunk = connection.recv(size - len(chunks))
            if not chunk:
                raise VerificationError("Modbus TCP connection closed mid-response")
            chunks.extend(chunk)
        return bytes(chunks)

    def request(self, pdu: bytes) -> bytes:
        self.transaction = (self.transaction + 1) & 0xFFFF
        header = struct.pack(">HHHB", self.transaction, 0, len(pdu) + 1, self.unit)
        with socket.create_connection((self.host, self.port), self.timeout) as connection:
            connection.settimeout(self.timeout)
            connection.sendall(header + pdu)
            response_header = self._receive_exact(connection, 7)
            transaction, protocol, length, unit = struct.unpack(">HHHB", response_header)
            if transaction != self.transaction or protocol != 0 or unit != self.unit:
                raise VerificationError("invalid MBAP response header")
            if length < 2 or length > 254:
                raise VerificationError(f"invalid MBAP response length {length}")
            return self._receive_exact(connection, length - 1)

    def close(self) -> None:
        pass


class RtuTransport:
    name = "rtu"

    def __init__(
        self, port: str, unit: int, baud: int, parity: str, stop_bits: int, timeout: float
    ):
        try:
            import serial
        except ImportError as error:
            raise VerificationError(
                "RTU verification requires pyserial (install with 'py -m pip install pyserial')"
            ) from error
        self.unit = unit
        self.timeout = timeout
        self.baud = baud
        self.parity = parity
        self.stop_bits = stop_bits
        parity_value = {"N": serial.PARITY_NONE, "E": serial.PARITY_EVEN, "O": serial.PARITY_ODD}
        self.connection = serial.Serial(
            port=port,
            baudrate=baud,
            bytesize=serial.EIGHTBITS,
            parity=parity_value[parity],
            stopbits=stop_bits,
            timeout=min(timeout, 0.1),
        )

    def request(self, pdu: bytes) -> bytes:
        self.connection.reset_input_buffer()
        self.connection.write(rtu_frame(self.unit, pdu))
        self.connection.flush()
        deadline = time.monotonic() + self.timeout
        response = bytearray()
        while time.monotonic() < deadline:
            chunk = self.connection.read(256 - len(response))
            if chunk:
                response.extend(chunk)
                continue
            if response:
                break
        if not response:
            raise VerificationError("RTU request timed out")
        return parse_rtu_frame(bytes(response), self.unit)

    def close(self) -> None:
        self.connection.close()


@dataclass
class Check:
    name: str
    passed: bool
    detail: str


class Verifier:
    def __init__(self, transport: Transport):
        self.transport = transport
        self.checks: list[Check] = []
        self._rtu_frame_errors_before: int | None = None
        self.prepared_relays: list[int] = []

    def check(self, name: str, action) -> object | None:
        try:
            value = action()
            self.checks.append(Check(name, True, str(value)))
            return value
        except Exception as error:
            self.checks.append(Check(name, False, str(error)))
            return None

    def exchange(self, function: int, payload: bytes, expected_exception: int | None = None) -> bytes:
        response = self.transport.request(bytes((function,)) + payload)
        if not response:
            raise VerificationError("empty PDU")
        if response[0] == (function | 0x80):
            if len(response) != 2:
                raise VerificationError("malformed exception response")
            if expected_exception == response[1]:
                return response
            raise VerificationError(f"Modbus exception 0x{response[1]:02X}")
        if expected_exception is not None:
            raise VerificationError(f"expected exception 0x{expected_exception:02X}")
        if response[0] != function:
            raise VerificationError(f"unexpected function 0x{response[0]:02X}")
        return response

    def read_bits(self, function: int, address: int, count: int) -> list[bool]:
        response = self.exchange(function, struct.pack(">HH", address, count))
        byte_count = (count + 7) // 8
        if len(response) != byte_count + 2 or response[1] != byte_count:
            raise VerificationError("invalid bit-read byte count")
        return [bool(response[2 + index // 8] & (1 << (index % 8))) for index in range(count)]

    def read_registers(self, function: int, address: int, count: int) -> list[int]:
        response = self.exchange(function, struct.pack(">HH", address, count))
        if len(response) != 2 + count * 2 or response[1] != count * 2:
            raise VerificationError("invalid register-read byte count")
        return list(struct.unpack(f">{count}H", response[2:]))

    def write_coil(self, address: int, value: bool) -> None:
        encoded = 0xFF00 if value else 0
        payload = struct.pack(">HH", address, encoded)
        response = self.exchange(5, payload)
        if response[1:] != payload:
            raise VerificationError("write-single-coil echo mismatch")

    def write_registers(self, address: int, values: list[int]) -> None:
        if not values or len(values) > 123 or any(not 0 <= value <= 0xFFFF for value in values):
            raise VerificationError("invalid FC16 register values")
        payload = (
            struct.pack(">HHB", address, len(values), len(values) * 2)
            + struct.pack(f">{len(values)}H", *values)
        )
        response = self.exchange(16, payload)
        if response[1:] != struct.pack(">HH", address, len(values)):
            raise VerificationError("write-multiple-registers echo mismatch")

    def device_identity(self) -> dict[str, str]:
        response = self.exchange(0x2B, bytes((0x0E, 1, 0)))
        if len(response) < 7 or response[1] != 0x0E:
            raise VerificationError("malformed device identity response")
        count = response[6]
        offset = 7
        objects = {}
        for _ in range(count):
            if offset + 2 > len(response):
                raise VerificationError("truncated device identity object")
            object_id, length = response[offset : offset + 2]
            offset += 2
            if offset + length > len(response):
                raise VerificationError("truncated device identity value")
            objects[f"0x{object_id:02X}"] = response[offset : offset + length].decode(
                "ascii", errors="replace"
            )
            offset += length
        if objects.get("0x01") != "EDGE-S3-6CH":
            raise VerificationError(f"unexpected product code {objects.get('0x01')!r}")
        return objects

    def run(
        self,
        write_channel: int | None,
        exercise_all_relays_seconds: float | None = None,
        toggle_cycles: int = 3,
        toggle_on_seconds: float = 1,
        toggle_off_seconds: float = 1,
        off_seconds: float = 5,
        prepare_off_channels: list[int] | None = None,
        exercise_device_io: bool = False,
        button_timeout_seconds: float = 60,
    ) -> None:
        self.check("device_identity", self.device_identity)
        self.check("map_version", lambda: self._expect_register(4, 0x0000, 1))
        self.check("capabilities", lambda: self.read_registers(4, 0x0001, 3))
        self.check(
            "effective_protocol",
            lambda: self._expect_register(4, 0x0002, 1 if self.transport.name == "rtu" else 2),
        )
        self.check("relay_coils", lambda: self.read_bits(1, 0x0000, 6))
        self.check("relay_outputs", lambda: self.read_bits(2, 0x0000, 6))
        self.check("transport_status", self._verify_transport_status)
        self.check("transport_configuration", self._verify_transport_configuration)
        if self.transport.name == "rtu":
            self.check("rtu_counters_before", self._read_rtu_counters_before)
        self.check(
            "reserved_coil_exception",
            lambda: self.exchange(1, struct.pack(">HH", 0x0006, 1), expected_exception=2).hex(),
        )
        self.check(
            "invalid_coil_value_exception",
            lambda: self.exchange(5, struct.pack(">HH", 0, 1), expected_exception=3).hex(),
        )
        if self.transport.name == "rtu":
            echo = b"\x12\x34"
            self.check(
                "rtu_diagnostics_echo",
                lambda: self._expect_payload(8, b"\x00\x00" + echo, b"\x00\x00" + echo),
            )
        if write_channel is not None:
            self.check("relay_write_restore", lambda: self._exercise_relay(write_channel - 1))
        if exercise_all_relays_seconds is not None:
            if any(not check.passed for check in self.checks):
                self.checks.append(
                    Check(
                        "relay_sequence_preflight",
                        False,
                        "Skipped because a Modbus preflight check failed",
                    )
                )
            else:
                self.check(
                    "relay_sequence",
                    lambda: self._exercise_all_relays(
                        exercise_all_relays_seconds,
                        toggle_cycles,
                        toggle_on_seconds,
                        toggle_off_seconds,
                        off_seconds,
                        prepare_off_channels,
                    ),
                )
        if exercise_device_io:
            if any(not check.passed for check in self.checks):
                self.checks.append(
                    Check(
                        "device_io_preflight",
                        False,
                        "Skipped because a Modbus preflight or relay check failed",
                    )
                )
            else:
                context = self.check("device_io_preflight", self._device_io_preflight)
                if context is not None:
                    self.check(
                        "modbus_rgb_identify",
                        lambda: self._exercise_modbus_identify(context),
                    )
                    self.check(
                        "physical_button_double_click",
                        lambda: self._monitor_button_double_click(
                            button_timeout_seconds, context
                        ),
                    )
        if self.transport.name == "rtu" and self._rtu_frame_errors_before is not None:
            self.check("rtu_counters_after", self._verify_rtu_counters_after)

    def _expect_register(self, function: int, address: int, expected: int) -> int:
        actual = self.read_registers(function, address, 1)[0]
        if actual != expected:
            raise VerificationError(f"expected {expected}, got {actual}")
        return actual

    def _verify_transport_status(self) -> list[bool]:
        status = self.read_bits(2, 0x0023, 4)
        if not status[2]:
            raise VerificationError(f"active Modbus transport is not ready: {status}")
        if self.transport.name == "rtu" and status[:2] != [True, True]:
            raise VerificationError(f"RS485 is disabled or not initialized: {status}")
        return status

    def _verify_transport_configuration(self) -> list[int]:
        registers = self.read_registers(4, 0x0020, 5)
        if self.transport.name == "rtu":
            expected_formats = {("E", 1): 0, ("O", 1): 1, ("N", 2): 2, ("N", 1): 3}
            expected_format = expected_formats.get(
                (getattr(self.transport, "parity", ""), getattr(self.transport, "stop_bits", 0))
            )
            baud_indices = {9600: 0, 19200: 1, 38400: 2, 57600: 3, 115200: 4}
            expected_baud = baud_indices.get(getattr(self.transport, "baud", 0))
            expected = [getattr(self.transport, "unit", -1), expected_baud, expected_format]
            if registers[:3] != expected:
                raise VerificationError(
                    f"RTU settings mismatch: device unit/baud/format={registers[:3]}, "
                    f"host expects={expected}"
                )
        return registers

    def _read_rtu_counters_before(self) -> list[int]:
        counters = self.read_registers(4, 0x0010, 6)
        self._rtu_frame_errors_before = (counters[2] << 16) | counters[3]
        return counters

    def _verify_rtu_counters_after(self) -> list[int]:
        counters = self.read_registers(4, 0x0010, 6)
        frame_errors_after = (counters[2] << 16) | counters[3]
        if frame_errors_after != self._rtu_frame_errors_before:
            raise VerificationError(
                f"RTU frame-error counter changed from {self._rtu_frame_errors_before} "
                f"to {frame_errors_after} during the run"
            )
        return counters

    def _device_io_preflight(self) -> dict[str, object]:
        coils = self.read_bits(1, 0, 6)
        outputs = self.read_bits(2, 0, 6)
        if any(coils) or any(outputs):
            raise VerificationError(
                f"device-I/O test requires relays OFF; coils={coils}, outputs={outputs}"
            )
        if not self.read_bits(1, 0x0020, 1)[0]:
            raise VerificationError("RGB feature is disabled")
        button_status = self.read_bits(2, 0x0010, 3)
        if button_status[0]:
            raise VerificationError("button is already pressed; release it before testing")
        if not button_status[1]:
            raise VerificationError("button application actions are disabled")
        if not self.read_bits(2, 0x0022, 1)[0]:
            raise VerificationError("hardware button service is unavailable")
        if self.read_bits(2, 0x0016, 1)[0]:
            raise VerificationError("active fault indication may mask the RGB identify output")

        rgb_profile = self.read_registers(3, 0x0100, 5)
        brightness = rgb_profile[3]
        if not 1 <= brightness <= 100:
            raise VerificationError(
                f"manual RGB brightness must be 1..100 for a visible identify test; got {brightness}"
            )
        bindings = self.read_registers(3, 0x0120, 6)
        if bindings[2] != 6:
            raise VerificationError(
                f"double-click must be mapped to identify (action 6); got action {bindings[2]}"
            )
        gesture_state = self.read_registers(4, 0x0005, 3)
        return {
            "relayCoils": coils,
            "relayOutputs": outputs,
            "brightness": brightness,
            "doubleClickAction": bindings[2],
            "gestureState": gesture_state,
        }

    @staticmethod
    def _gesture_counter(registers: list[int]) -> int:
        return (registers[1] << 16) | registers[2]

    def _read_identify_output(self, expected_rgb: list[int]) -> list[int]:
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            rgb = self.read_registers(4, 0x0030, 3)
            reason = self.read_registers(4, 0x0034, 1)[0]
            led_on = self.read_bits(2, 0x0012, 1)[0]
            if rgb == expected_rgb and reason == 2 and led_on:
                return rgb
            time.sleep(0.05)
        raise VerificationError(
            "identify did not produce the expected nonblack manual RGB output "
            f"{expected_rgb}; current RGB/reason/LED={rgb}/{reason}/{led_on}"
        )

    def _assert_relays_unchanged(self, context: dict[str, object]) -> None:
        coils = self.read_bits(1, 0, 6)
        outputs = self.read_bits(2, 0, 6)
        if coils != context["relayCoils"] or outputs != context["relayOutputs"]:
            raise VerificationError(
                f"device-I/O test changed relay state: coils={coils}, outputs={outputs}"
            )

    def _exercise_modbus_identify(self, context: dict[str, object]) -> str:
        last_sequence = self.read_registers(4, 0x0025, 1)[0]
        sequence = (last_sequence % 0xFFFF) + 1
        try:
            self.write_registers(0x0200, [1, 0, sequence, 0xA55A])
        except VerificationError as error:
            if "Modbus exception 0x02" in str(error):
                raise VerificationError(
                    "identify command denied; enable the installer permission for "
                    "manual indicators and diagnostic commands"
                ) from error
            raise

        deadline = time.monotonic() + 2.0
        result = [0, 0, 0]
        while time.monotonic() < deadline:
            result = self.read_registers(4, 0x0025, 3)
            if result[0] == sequence and result[1] != 1:
                break
            time.sleep(0.05)
        if result != [sequence, 2, 0]:
            raise VerificationError(
                f"identify mailbox was not applied; sequence/result/detail={result}"
            )
        expected_level = int(context["brightness"]) * 255 // 100
        expected_rgb = [expected_level] * 3
        rgb = self._read_identify_output(expected_rgb)
        self._assert_relays_unchanged(context)
        return (
            f"Modbus identify sequence {sequence} applied; RGB register output={rgb}; "
            "visual LED output requires observer confirmation"
        )

    def _monitor_button_double_click(
        self, timeout_seconds: float, context: dict[str, object]
    ) -> str:
        initial = self.read_registers(4, 0x0005, 3)
        initial_count = self._gesture_counter(initial)
        print(
            "Button test ready: briefly double-click the physical BOOT button now; "
            "do not hold it. Identify should light the RGB LED white for 5 seconds.",
            flush=True,
        )
        deadline = time.monotonic() + timeout_seconds
        pressed_seen = False
        while time.monotonic() < deadline:
            pressed_seen = self.read_bits(2, 0x0010, 1)[0] or pressed_seen
            current = self.read_registers(4, 0x0005, 3)
            count = self._gesture_counter(current)
            if count != initial_count:
                increment = (count - initial_count) & 0xFFFFFFFF
                if increment != 1 or current[0] != 2:
                    raise VerificationError(
                        f"expected exactly one double-click gesture; "
                        f"last gesture={current[0]}, counter increment={increment}"
                    )
                expected_level = int(context["brightness"]) * 255 // 100
                rgb = self._read_identify_output([expected_level] * 3)
                self._assert_relays_unchanged(context)
                return (
                    f"physical double-click observed once; debounced-pressed sample="
                    f"{pressed_seen}; RGB register output={rgb}; visual LED output "
                    "requires observer confirmation"
                )
            time.sleep(0.05)
        raise VerificationError(
            f"no physical double-click gesture observed within {timeout_seconds:g} seconds"
        )

    def _expect_payload(self, function: int, payload: bytes, expected: bytes) -> str:
        response = self.exchange(function, payload)
        if response[1:] != expected:
            raise VerificationError("response payload mismatch")
        return response.hex()

    def _exercise_relay(self, address: int) -> str:
        original = self.read_bits(1, address, 1)[0]
        requested = not original
        try:
            self.write_coil(address, requested)
            if self.read_bits(1, address, 1)[0] != requested:
                raise VerificationError("coil read-back did not change")
            if self.read_bits(2, address, 1)[0] != requested:
                raise VerificationError("applied output did not change")
        finally:
            self.write_coil(address, original)
        return f"channel {address + 1} changed and restored to {original}"

    def _exercise_all_relays(
        self,
        on_seconds: float,
        toggle_cycles: int,
        toggle_on_seconds: float,
        toggle_off_seconds: float,
        off_seconds: float,
        prepare_off_channels: list[int] | None = None,
    ) -> str:
        durations = (on_seconds, toggle_on_seconds, toggle_off_seconds, off_seconds)
        if any(duration <= 0 or duration > 3600 for duration in durations):
            raise VerificationError("all relay hold durations must be greater than 0 and at most 3600 seconds")
        if not 1 <= toggle_cycles <= 100:
            raise VerificationError("toggle cycle count must be between 1 and 100")

        coils = self.read_bits(1, 0, 6)
        outputs = self.read_bits(2, 0, 6)
        initially_on = [index + 1 for index, is_on in enumerate(coils) if is_on]
        if any(coils) or any(outputs):
            if coils != outputs:
                raise VerificationError(
                    f"refusing to prepare mismatched coil/output states: coils={coils}, outputs={outputs}"
                )
            authorized_channels = prepare_off_channels or []
            if not set(initially_on).issubset(authorized_channels):
                raise VerificationError(
                    f"preflight requires all relays OFF; coils={coils}, outputs={outputs}; "
                    "authorize each initially-ON channel with --prepare-off-channel before testing"
                )
            prepared_state = list(coils)
            self.prepared_relays = initially_on
            for address in self.prepared_relays:
                self.write_coil(address - 1, False)
                prepared_state[address - 1] = False
                self._verify_all_relays(
                    prepared_state, f"preparation OFF for channel {address}"
                )
            self.checks.append(
                Check(
                    "relay_sequence_preparation",
                    True,
                    f"Explicitly prepared these initially ON channels OFF: {self.prepared_relays}",
                )
            )
        else:
            self.checks.append(
                Check("relay_sequence_preparation", True, "No relay preparation was required")
            )
        self._verify_all_relays([False] * 6, "all-OFF preflight")
        self.checks.append(Check("relay_sequence_preflight", True, "All six coils and outputs are OFF"))

        for address in range(6):
            channel = address + 1
            expected_on = [index == address for index in range(6)]
            expected_off = [False] * 6
            try:
                try:
                    for cycle in range(1, toggle_cycles + 1):
                        self.write_coil(address, True)
                        self._verify_all_relays(expected_on, f"channel {channel} cycle {cycle} ON")
                        self.checks.append(
                            Check(
                                f"relay_{channel}_cycle_{cycle}_on",
                                True,
                                "Only this channel read ON",
                            )
                        )
                        on_elapsed = self._hold_and_verify(
                            expected_on,
                            toggle_on_seconds,
                            f"channel {channel} cycle {cycle} ON",
                        )
                        self.checks.append(
                            Check(
                                f"relay_{channel}_cycle_{cycle}_on_hold",
                                True,
                                f"Only this channel remained ON; hold observed for {on_elapsed:.3f} seconds",
                            )
                        )
                        self.write_coil(address, False)
                        self._verify_all_relays(
                            expected_off, f"channel {channel} cycle {cycle} OFF"
                        )
                        self.checks.append(
                            Check(
                                f"relay_{channel}_cycle_{cycle}_off",
                                True,
                                "All six channels read OFF",
                            )
                        )
                        off_elapsed = self._hold_and_verify(
                            expected_off,
                            toggle_off_seconds,
                            f"channel {channel} cycle {cycle} OFF",
                        )
                        self.checks.append(
                            Check(
                                f"relay_{channel}_cycle_{cycle}_off_hold",
                                True,
                                f"All six channels remained OFF; hold observed for {off_elapsed:.3f} seconds",
                            )
                        )

                    self.write_coil(address, True)
                    self._verify_all_relays(expected_on, f"channel {channel} ON")
                    self.checks.append(
                        Check(f"relay_{channel}_dwell_on", True, "Only this channel read ON")
                    )
                    print(
                        f"Channel {channel} ON; holding for {on_seconds:g} seconds.",
                        flush=True,
                    )
                    on_elapsed = self._hold_and_verify(
                        expected_on, on_seconds, f"channel {channel} dwell ON"
                    )
                    self.checks.append(
                        Check(
                            f"relay_{channel}_dwell_on_hold",
                            True,
                            f"Only this channel remained ON; hold observed for {on_elapsed:.3f} seconds",
                        )
                    )
                except Exception as error:
                    self.checks.append(Check(f"relay_{channel}_sequence", False, str(error)))
                    raise
            finally:
                try:
                    self.write_coil(address, False)
                    self._verify_all_relays(expected_off, f"channel {channel} OFF")
                    self.checks.append(
                        Check(f"relay_{channel}_dwell_off", True, "All six channels read OFF")
                    )
                    print(
                        f"Channel {channel} OFF; holding for {off_seconds:g} seconds.",
                        flush=True,
                    )
                    off_elapsed = self._hold_and_verify(
                        expected_off, off_seconds, f"channel {channel} dwell OFF"
                    )
                    self.checks.append(
                        Check(
                            f"relay_{channel}_dwell_off_hold",
                            True,
                            f"All six channels remained OFF; hold observed for {off_elapsed:.3f} seconds",
                        )
                    )
                    print(f"Channel {channel} OFF; OFF dwell verified.", flush=True)
                except Exception as error:
                    self.checks.append(Check(f"relay_{channel}_dwell_off", False, str(error)))
                    raise VerificationError(
                        f"channel {channel} OFF restoration/read-back failed: {error}"
                    ) from error

        final_coils = self.read_bits(1, 0, 6)
        final_outputs = self.read_bits(2, 0, 6)
        if any(final_coils) or any(final_outputs):
            raise VerificationError(
                f"final state requires all relays OFF; coils={final_coils}, outputs={final_outputs}"
            )
        self.checks.append(
            Check("relay_sequence_final_state", True, "All six coils and outputs are OFF")
        )
        return (
            f"all six relays completed {toggle_cycles} cycles of "
            f"{toggle_on_seconds:g}s ON/{toggle_off_seconds:g}s OFF, "
            f"{on_seconds:g}s ON dwell, and {off_seconds:g}s OFF dwell"
        )

    def _hold_and_verify(
        self,
        expected: list[bool],
        seconds: float,
        phase: str,
    ) -> float:
        started = time.monotonic()
        time.sleep(seconds)
        self._verify_all_relays(expected, phase)
        elapsed = time.monotonic() - started
        if elapsed < seconds:
            raise VerificationError(
                f"{phase} elapsed {elapsed:.3f}s, shorter than requested {seconds:g}s"
            )
        return elapsed

    def _verify_all_relays(self, expected: list[bool], phase: str) -> None:
        coils = self.read_bits(1, 0, 6)
        outputs = self.read_bits(2, 0, 6)
        if coils != expected or outputs != expected:
            raise VerificationError(
                f"{phase} mismatch: coils={coils}, outputs={outputs}, expected={expected}"
            )


def self_test() -> None:
    if crc16(b"123456789") != 0x4B37:
        raise VerificationError("CRC-16/MODBUS vector failed")
    frame = rtu_frame(1, b"\x03\x00\x00\x00\x01")
    if parse_rtu_frame(frame, 1) != b"\x03\x00\x00\x00\x01":
        raise VerificationError("RTU round-trip failed")
    try:
        parse_rtu_frame(frame[:-1] + bytes((frame[-1] ^ 1,)), 1)
    except VerificationError:
        pass
    else:
        raise VerificationError("bad RTU CRC was accepted")

    class FakeTransport:
        name = "tcp"

        def __init__(self, fail_on_output_read: bool = False):
            self.coils = [False] * 6
            self.writes: list[tuple[int, bool]] = []
            self.fail_on_output_read = fail_on_output_read

        def request(self, pdu: bytes) -> bytes:
            function = pdu[0]
            address, quantity = struct.unpack(">HH", pdu[1:5])
            if function == 5:
                value = struct.unpack(">H", pdu[3:5])[0] == 0xFF00
                self.coils[address] = value
                self.writes.append((address, value))
                return pdu
            if function in (1, 2):
                if function == 2 and self.fail_on_output_read and self.coils[address]:
                    return bytes((function, 1, 0))
                bits = self.coils[address : address + quantity]
                packed = sum(int(bit) << index for index, bit in enumerate(bits))
                return bytes((function, 1, packed))
            raise AssertionError(f"unexpected function code {function}")

        def close(self) -> None:
            pass

    fake = FakeTransport()
    verifier = Verifier(fake)
    with mock.patch("time.sleep") as sleep, mock.patch("builtins.print"):
        detail = verifier._exercise_all_relays(0.000001, 2, 0.000001, 0.000001, 0.000001)
    expected_writes = [
        (address, value)
        for address in range(6)
        for value in (True, False) * 3
    ]
    if fake.writes != expected_writes:
        raise VerificationError("all-relay sequence did not complete all toggles and dwell writes")
    if fake.coils != [False] * 6 or sleep.call_count != 36:
        raise VerificationError("all-relay sequence did not restore OFF or verify all hold intervals")
    if "all six relays" not in detail:
        raise VerificationError("all-relay sequence report detail is missing")

    occupied = FakeTransport()
    occupied.coils[2] = True
    try:
        Verifier(occupied)._exercise_all_relays(1, 1, 0.000001, 0.000001, 0.000001)
    except VerificationError:
        pass
    else:
        raise VerificationError("all-relay sequence accepted a non-OFF initial state")
    if occupied.writes:
        raise VerificationError("all-relay sequence wrote before a passing OFF preflight")

    prepared = FakeTransport()
    prepared.coils[3] = True
    prepared_verifier = Verifier(prepared)
    with mock.patch("time.sleep"), mock.patch("builtins.print"):
        prepared_verifier._exercise_all_relays(
            0.000001, 1, 0.000001, 0.000001, 0.000001, [4]
        )
    if prepared_verifier.prepared_relays != [4] or prepared.coils != [False] * 6:
        raise VerificationError("explicit OFF preparation did not record and clear the initial relay")

    mismatched = FakeTransport(fail_on_output_read=True)
    try:
        Verifier(mismatched)._exercise_all_relays(
            1, 1, 0.000001, 0.000001, 0.000001
        )
    except VerificationError:
        pass
    else:
        raise VerificationError("all-relay sequence accepted an ON read-back mismatch")
    if mismatched.coils[0]:
        raise VerificationError("all-relay sequence did not restore OFF after an ON mismatch")

    class FakeDeviceIoTransport:
        name = "tcp"

        def __init__(self, identify_allowed: bool = True):
            self.identify_allowed = identify_allowed
            self.rgb = [0, 25, 0]
            self.indicator_reason = 1
            self.last_sequence = 10
            self.command_result = 0
            self.command_detail = 0
            self.last_gesture = 0
            self.gesture_count = 0
            self.gesture_reads = 0
            self.relay_writes = 0
            self.modbus_commands = 0

        def request(self, pdu: bytes) -> bytes:
            function = pdu[0]
            address, quantity = struct.unpack(">HH", pdu[1:5])
            if function in (1, 2):
                if function == 1 and address == 0 and quantity == 6:
                    bits = [False] * 6
                elif function == 1 and address == 0x20 and quantity == 1:
                    bits = [True]
                elif function == 2 and address == 0 and quantity == 6:
                    bits = [False] * 6
                elif function == 2 and address == 0x10 and quantity == 3:
                    bits = [False, True, self.indicator_reason == 2]
                elif function == 2 and address == 0x10 and quantity == 1:
                    bits = [False]
                elif function == 2 and address == 0x16 and quantity == 1:
                    bits = [False]
                elif function == 2 and address == 0x22 and quantity == 1:
                    bits = [True]
                elif function == 2 and address == 0x12 and quantity == 1:
                    bits = [any(self.rgb)]
                else:
                    raise AssertionError(f"unexpected bit read: function={function}, address={address}")
                packed = sum(int(bit) << index for index, bit in enumerate(bits))
                return bytes((function, 1, packed))
            if function == 3:
                if address == 0x100 and quantity == 5:
                    values = [0, 0, 0, 10, 5]
                elif address == 0x120 and quantity == 6:
                    values = [0, 0, 6, 0, 8, 0]
                else:
                    raise AssertionError(f"unexpected holding-register read at {address:#x}")
                return bytes((function, len(values) * 2)) + struct.pack(f">{len(values)}H", *values)
            if function == 4:
                if address == 0x05 and quantity == 3:
                    self.gesture_reads += 1
                    if self.gesture_reads == 3:
                        self.last_gesture = 2
                        self.gesture_count = 1
                    values = [self.last_gesture, 0, self.gesture_count]
                elif address == 0x25 and quantity == 1:
                    values = [self.last_sequence]
                elif address == 0x25 and quantity == 3:
                    values = [self.last_sequence, self.command_result, self.command_detail]
                elif address == 0x30 and quantity == 3:
                    values = self.rgb
                elif address == 0x34 and quantity == 1:
                    values = [self.indicator_reason]
                else:
                    raise AssertionError(f"unexpected input-register read at {address:#x}")
                return bytes((function, len(values) * 2)) + struct.pack(f">{len(values)}H", *values)
            if function == 16:
                if not self.identify_allowed:
                    return bytes((function | 0x80, 2))
                start, count, byte_count = struct.unpack(">HHB", pdu[1:6])
                values = list(struct.unpack(f">{count}H", pdu[6 : 6 + byte_count]))
                if start != 0x200 or count != 4 or values[0] != 1 or values[3] != 0xA55A:
                    raise AssertionError("invalid fake identify-mailbox request")
                self.last_sequence = values[2]
                self.command_result = 2
                self.command_detail = 0
                self.rgb = [25, 25, 25]
                self.indicator_reason = 2
                self.modbus_commands += 1
                return bytes((function,)) + struct.pack(">HH", start, count)
            if function == 5:
                self.relay_writes += 1
                raise AssertionError("device-I/O tests must not write relay coils")
            raise AssertionError(f"unexpected function code {function}")

        def close(self) -> None:
            pass

    io_transport = FakeDeviceIoTransport()
    io_verifier = Verifier(io_transport)
    with mock.patch("time.sleep"), mock.patch("builtins.print"):
        io_context = io_verifier._device_io_preflight()
        identify_detail = io_verifier._exercise_modbus_identify(io_context)
        button_detail = io_verifier._monitor_button_double_click(1, io_context)
    if io_transport.modbus_commands != 1 or io_transport.relay_writes:
        raise VerificationError("device-I/O test issued an unexpected Modbus command")
    if "RGB register output=[25, 25, 25]" not in identify_detail or "physical double-click" not in button_detail:
        raise VerificationError("device-I/O test did not verify identify and button behavior")

    denied_io = FakeDeviceIoTransport(identify_allowed=False)
    denied_verifier = Verifier(denied_io)
    try:
        denied_verifier._exercise_modbus_identify(denied_verifier._device_io_preflight())
    except VerificationError as error:
        if "enable the installer permission" not in str(error):
            raise
    else:
        raise VerificationError("device-I/O test accepted an unauthorized identify command")
    if denied_io.relay_writes:
        raise VerificationError("unauthorized device-I/O test wrote a relay coil")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--transport", choices=("tcp", "rtu"))
    parser.add_argument("--host", help="Modbus TCP device address")
    parser.add_argument("--port", type=int, default=502, help="TCP port or RTU COM port number is not accepted")
    parser.add_argument("--serial-port", help="RTU serial port, for example COM4")
    parser.add_argument("--unit", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--baud", type=int, default=19200)
    parser.add_argument("--parity", choices=("N", "E", "O"), default="E")
    parser.add_argument("--stop-bits", type=int, choices=(1, 2), default=1)
    parser.add_argument("--write-channel", type=int, choices=range(1, 7), metavar="1-6")
    parser.add_argument(
        "--exercise-all-relays",
        action="store_true",
        help="test each relay ON individually, then restore it OFF",
    )
    parser.add_argument(
        "--on-seconds",
        type=float,
        default=30,
        help="long ON dwell per relay when --exercise-all-relays is used (default: 30)",
    )
    parser.add_argument(
        "--toggle-cycles",
        type=int,
        default=3,
        help="short ON/OFF cycles per relay (default: 3)",
    )
    parser.add_argument(
        "--toggle-on-seconds",
        type=float,
        default=1,
        help="ON interval within each short toggle cycle (default: 1)",
    )
    parser.add_argument(
        "--toggle-off-seconds",
        type=float,
        default=1,
        help="OFF interval within each short toggle cycle (default: 1)",
    )
    parser.add_argument(
        "--off-seconds",
        type=float,
        default=5,
        help="final OFF dwell after each relay's long ON dwell (default: 5)",
    )
    parser.add_argument(
        "--confirm-safe-loads",
        action="store_true",
        help="confirm isolated safe test loads are connected before the relay sequence",
    )
    parser.add_argument(
        "--prepare-off-channel",
        type=int,
        choices=range(1, 7),
        action="append",
        default=[],
        metavar="1-6",
        help="explicitly authorize preparing this channel OFF before testing; repeat for multiple channels",
    )
    parser.add_argument(
        "--exercise-device-io",
        action="store_true",
        help="test Modbus RGB identify and monitor one physical double-click identify action",
    )
    parser.add_argument(
        "--button-timeout-seconds",
        type=float,
        default=60,
        help="wait time for the operator's physical double-click (default: 60)",
    )
    parser.add_argument(
        "--confirm-indicator-observation",
        action="store_true",
        help="confirm an operator can safely observe the RGB LED during the identify test",
    )
    parser.add_argument("--output", type=Path, help="write JSON evidence to this file")
    parser.add_argument("--self-test", action="store_true", help="test codecs without a device")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_test:
        self_test()
        print("Modbus verifier self-test passed")
        return 0
    if args.transport == "tcp" and not args.host:
        raise VerificationError("--host is required for TCP")
    if args.transport == "rtu" and not args.serial_port:
        raise VerificationError("--serial-port is required for RTU")
    if args.transport is None:
        raise VerificationError("--transport is required unless --self-test is used")
    if args.write_channel is not None and args.exercise_all_relays:
        raise VerificationError("--write-channel and --exercise-all-relays cannot be combined")
    if args.exercise_all_relays and not args.confirm_safe_loads:
        raise VerificationError(
            "--exercise-all-relays requires --confirm-safe-loads and isolated safe test loads"
        )
    if args.prepare_off_channel and not args.exercise_all_relays:
        raise VerificationError("--prepare-off-channel can only be used with --exercise-all-relays")
    if args.prepare_off_channel and not args.confirm_safe_loads:
        raise VerificationError("--prepare-off-channel requires --confirm-safe-loads")
    if args.exercise_device_io and not args.confirm_indicator_observation:
        raise VerificationError(
            "--exercise-device-io requires --confirm-indicator-observation"
        )
    if args.exercise_device_io and (
        args.button_timeout_seconds <= 0 or args.button_timeout_seconds > 600
    ):
        raise VerificationError("--button-timeout-seconds must be greater than 0 and at most 600")
    if args.exercise_all_relays:
        if not 1 <= args.toggle_cycles <= 100:
            raise VerificationError("--toggle-cycles must be between 1 and 100")
        for name, duration in (
            ("--on-seconds", args.on_seconds),
            ("--toggle-on-seconds", args.toggle_on_seconds),
            ("--toggle-off-seconds", args.toggle_off_seconds),
            ("--off-seconds", args.off_seconds),
        ):
            if duration <= 0 or duration > 3600:
                raise VerificationError(f"{name} must be greater than 0 and at most 3600")

    transport: Transport
    if args.transport == "tcp":
        transport = TcpTransport(args.host, args.port, args.unit, args.timeout)
    else:
        transport = RtuTransport(
            args.serial_port, args.unit, args.baud, args.parity, args.stop_bits, args.timeout
        )
    verifier = Verifier(transport)
    started_at = datetime.now(timezone.utc)
    started_monotonic = time.monotonic()
    try:
        verifier.run(
            args.write_channel,
            args.on_seconds if args.exercise_all_relays else None,
            args.toggle_cycles,
            args.toggle_on_seconds,
            args.toggle_off_seconds,
            args.off_seconds,
            args.prepare_off_channel,
            args.exercise_device_io,
            args.button_timeout_seconds,
        )
    finally:
        transport.close()
    elapsed_seconds = round(time.monotonic() - started_monotonic, 3)
    device_io_checks = {
        check.name: check
        for check in verifier.checks
        if check.name in ("modbus_rgb_identify", "physical_button_double_click")
    }
    report = {
        "startedAt": started_at.isoformat(),
        "completedAt": datetime.now(timezone.utc).isoformat(),
        "elapsedSeconds": elapsed_seconds,
        "transport": args.transport,
        "target": args.host if args.transport == "tcp" else args.serial_port,
        "unit": args.unit,
        "writeChannel": args.write_channel,
        "relaySequence": (
            {
                "enabled": True,
                "preparedInitiallyOnChannels": verifier.prepared_relays,
                "profile": {
                    "toggleCyclesPerChannel": args.toggle_cycles,
                    "toggleOnSeconds": args.toggle_on_seconds,
                    "toggleOffSeconds": args.toggle_off_seconds,
                    "longOnSeconds": args.on_seconds,
                    "finalOffSeconds": args.off_seconds,
                },
                "serialProfile": (
                    {
                        "port": args.serial_port,
                        "baud": args.baud,
                        "parity": args.parity,
                        "dataBits": 8,
                        "stopBits": args.stop_bits,
                    }
                    if args.transport == "rtu"
                    else None
                ),
                "safeLoadsConfirmed": args.confirm_safe_loads,
                "standardAlignment": (
                    "IEC 62381 FAT/SAT; Modbus Application Protocol Specification V1.1b3; "
                    "Modbus Serial Line Protocol and Implementation Guide V1.02"
                ),
                "certificationClaim": False,
            }
            if args.exercise_all_relays
            else None
        ),
        "deviceIoSequence": (
            {
                "enabled": True,
                "modbusIdentifyAttempted": "modbus_rgb_identify" in device_io_checks,
                "modbusIdentifyPassed": (
                    device_io_checks["modbus_rgb_identify"].passed
                    if "modbus_rgb_identify" in device_io_checks
                    else None
                ),
                "physicalGesture": "double-click",
                "physicalButtonAttempted": (
                    "physical_button_double_click" in device_io_checks
                ),
                "physicalButtonPassed": (
                    device_io_checks["physical_button_double_click"].passed
                    if "physical_button_double_click" in device_io_checks
                    else None
                ),
                "buttonTimeoutSeconds": args.button_timeout_seconds,
                "indicatorObserverPresent": args.confirm_indicator_observation,
                "visualLedMeasurement": "Not measured by Modbus; record operator observation separately",
                "relayStateMustRemainUnchanged": True,
            }
            if args.exercise_device_io
            else None
        ),
        "checks": [asdict(check) for check in verifier.checks],
        "passed": all(check.passed for check in verifier.checks),
    }
    rendered = json.dumps(report, indent=2)
    print(rendered)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered + "\n", encoding="utf-8")
    return 0 if all(check.passed for check in verifier.checks) else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except VerificationError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(2) from error