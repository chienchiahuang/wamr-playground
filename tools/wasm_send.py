#!/usr/bin/env python3
"""Send a .wasm binary to a Zephyr device over serial.

Usage:
    python3 wasm_send.py -p /dev/tty.usbmodem* -s 0 sensor.wasm
    python3 wasm_send.py -p /dev/tty.usbmodem* -s 2 alert.wasm

Slots: 0=sensor, 1=actuator, 2=alert

Protocol:
    MAGIC(4B "WASM") + CMD(1B) + SLOT(1B) + SIZE(4B LE) + '\\n' + PAYLOAD + CRC32(4B LE)
"""

import argparse
import struct
import sys
import time
import zlib

try:
    import serial
except ImportError:
    print("Error: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

MAGIC = b"WASM"
CMD_UPLOAD = 0x01
CMD_STATUS = 0x02

SLOT_NAMES = {0: "sensor", 1: "actuator", 2: "alert"}


def send_wasm(port, baudrate, wasm_path, slot):
    with open(wasm_path, "rb") as f:
        payload = f.read()

    if payload[:4] != b"\x00asm":
        print(f"Warning: {wasm_path} does not look like a valid .wasm file")

    crc = zlib.crc32(payload) & 0xFFFFFFFF
    slot_name = SLOT_NAMES.get(slot, f"slot{slot}")
    header = MAGIC + struct.pack("<BBI", CMD_UPLOAD, slot, len(payload)) + b"\n"
    trailer = struct.pack("<I", crc)

    print(f"Sending {wasm_path} to slot {slot} ({slot_name})")
    print(f"  {len(payload)} bytes, CRC32=0x{crc:08x}")

    ser = serial.Serial(port, baudrate, timeout=5)
    time.sleep(0.1)

    ser.reset_input_buffer()
    ser.write(header)
    ser.write(payload)
    ser.write(trailer)
    ser.flush()

    print("Waiting for response...")
    deadline = time.time() + 5
    while time.time() < deadline:
        line = ser.readline().decode("ascii", errors="replace").strip()
        if line:
            print(f"  < {line}")
            if line.startswith("OK") or line.startswith("ERR"):
                break

    ser.close()


def query_status(port, baudrate):
    header = MAGIC + struct.pack("<BBI", CMD_STATUS, 0, 0) + b"\n"

    ser = serial.Serial(port, baudrate, timeout=3)
    time.sleep(0.1)
    ser.reset_input_buffer()
    ser.write(header)
    ser.flush()

    deadline = time.time() + 3
    while time.time() < deadline:
        line = ser.readline().decode("ascii", errors="replace").strip()
        if line:
            print(f"  < {line}")
            break

    ser.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Send .wasm to Zephyr device")
    parser.add_argument("wasm_file", nargs="?", help="Path to .wasm file")
    parser.add_argument("-p", "--port", required=True, help="Serial port")
    parser.add_argument("-b", "--baud", type=int, default=115200)
    parser.add_argument("-s", "--slot", type=int, default=0,
                        help="Target slot: 0=sensor, 1=actuator, 2=alert")
    parser.add_argument("--status", action="store_true", help="Query device status")
    args = parser.parse_args()

    if args.status:
        query_status(args.port, args.baud)
    elif args.wasm_file:
        send_wasm(args.port, args.baud, args.wasm_file, args.slot)
    else:
        parser.error("Provide a .wasm file or use --status")
