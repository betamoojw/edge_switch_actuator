"""Run read-only Modbus protocol checks against an actuator.

TCP uses the Python standard library. RTU additionally requires pyserial.
Relay actuation is disabled unless --write-channel is supplied; the original
coil state is restored before exit.
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

    def run(self, write_channel: int | None) -> None:
        self.check("device_identity", self.device_identity)
        self.check("map_version", lambda: self._expect_register(4, 0x0000, 1))
        self.check("capabilities", lambda: self.read_registers(4, 0x0001, 3))
        self.check("relay_coils", lambda: self.read_bits(1, 0x0000, 6))
        self.check("relay_outputs", lambda: self.read_bits(2, 0x0000, 6))
        self.check("transport_status", lambda: self.read_bits(2, 0x0023, 4))
        self.check("transport_configuration", lambda: self.read_registers(4, 0x0020, 5))
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

    def _expect_register(self, function: int, address: int, expected: int) -> int:
        actual = self.read_registers(function, address, 1)[0]
        if actual != expected:
            raise VerificationError(f"expected {expected}, got {actual}")
        return actual

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

    transport: Transport
    if args.transport == "tcp":
        transport = TcpTransport(args.host, args.port, args.unit, args.timeout)
    else:
        transport = RtuTransport(
            args.serial_port, args.unit, args.baud, args.parity, args.stop_bits, args.timeout
        )
    verifier = Verifier(transport)
    try:
        verifier.run(args.write_channel)
    finally:
        transport.close()
    report = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "transport": args.transport,
        "target": args.host if args.transport == "tcp" else args.serial_port,
        "unit": args.unit,
        "writeChannel": args.write_channel,
        "checks": [asdict(check) for check in verifier.checks],
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