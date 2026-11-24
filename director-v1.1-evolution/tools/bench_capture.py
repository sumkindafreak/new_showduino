#!/usr/bin/env python3
"""Read-only three-port capture. Python 3.9+, pyserial only for live capture."""
import argparse
import base64
import json
import time
from datetime import datetime, timezone
from pathlib import Path

ROLES = ("director", "comms", "p4")
CHUNK = 4096
MAX_LINE = 8192


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def write_record(out, record):
    out.write(json.dumps(record, ensure_ascii=True) + "\n")
    out.flush()


def capture(args):
    # Import lazily: offline reports work without serial drivers/dependencies.
    import serial
    ports = {}
    config = {role: getattr(args, role) for role in ROLES}
    if len({port.casefold() for port, _ in config.values()}) != len(ROLES):
        raise ValueError("Each module must have a distinct serial port.")
    start = time.monotonic_ns()
    with Path(args.output).open("x", encoding="utf-8") as out:
        write_record(out, {"kind": "start", "utc": utc_now(), "ports": config})
        try:
            for role, (port, baud) in config.items():
                device = serial.Serial(port=None, baudrate=baud, timeout=0,
                                       xonxoff=False, rtscts=False, dsrdtr=False)
                ports[role] = device
                device.dtr = False
                device.rts = False
                device.port = port
                device.open()
                write_record(out, {"kind": "open", "role": role,
                                   "ns": time.monotonic_ns() - start})
            print("Capturing. Ctrl+C to stop. No serial commands are sent.")
            while True:
                if args.seconds and time.monotonic_ns() - start >= args.seconds * 1_000_000_000:
                    break
                received = False
                for role, device in ports.items():
                    data = device.read(min(CHUNK, device.in_waiting))
                    if not data:
                        continue
                    received = True
                    write_record(out, {"kind": "rx", "role": role,
                                       "ns": time.monotonic_ns() - start,
                                       "data": base64.b64encode(data).decode("ascii")})
                if not received:
                    time.sleep(0.005)
        except KeyboardInterrupt:
            print("Capture stopped.")
        except Exception as exc:
            write_record(out, {"kind": "error", "message": str(exc),
                               "ns": time.monotonic_ns() - start})
            raise
        finally:
            for role, device in ports.items():
                try:
                    device.close()
                except Exception as exc:
                    write_record(out, {"kind": "error", "role": role,
                                       "message": str(exc)})
            write_record(out, {"kind": "end", "utc": utc_now(),
                               "ns": time.monotonic_ns() - start})


def iter_lines(records):
    """Reassemble fragmented lines separately; bound buffers on noisy streams."""
    buffers = {role: bytearray() for role in ROLES}
    for rec in records:
        if rec.get("kind") != "rx":
            continue
        role = rec["role"]
        if role not in buffers:
            raise ValueError("Unknown module: " + role)
        data = base64.b64decode(rec["data"], validate=True)
        buf = buffers[role]
        for value in data:
            if value == 10:
                yield rec["ns"], role, buf.rstrip(b"\r").decode("utf-8", "replace"), False
                buf.clear()
            else:
                buf.append(value)
                if len(buf) >= MAX_LINE:
                    yield rec["ns"], role, buf.decode("utf-8", "replace"), True
                    buf.clear()
    for role, buf in buffers.items():
        if buf:
            yield None, role, buf.decode("utf-8", "replace"), True


def records_from(path):
    with Path(path).open(encoding="utf-8") as stream:
        for number, line in enumerate(stream, 1):
            try:
                yield json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError("Invalid capture record at line " + str(number)) from exc


def report(args):
    # Receiver timestamps approximate arrival, not firmware execution time.
    # Count only candidate lines; do not interpret these as confirmed faults.
    markers = ("error", "fault", "panic", "watchdog", "emergency", "timeout", "reset")
    counts = dict.fromkeys(ROLES, 0)
    candidates = 0
    with Path(args.output).open("x", encoding="utf-8") as out:
        for ns, role, line, partial in iter_lines(records_from(args.input)):
            counts[role] += 1
            flagged = any(marker in line.lower() for marker in markers)
            candidates += int(flagged)
            stamp = "EOF" if ns is None else f"{ns / 1e9:12.6f}"
            out.write(f"{stamp} {role:8} {'?' if flagged else ' '} "
                      f"{'[partial] ' if partial else ''}{line}\n")
        out.write(f"\nLine counts: {counts}\nCandidate lines: {candidates}\n")
        errors = 0
        ended = False
        for rec in records_from(args.input):
            if rec.get("kind") == "error":
                errors += 1
                out.write(f"CAPTURE ERROR: {rec.get('message', '')}\n")
            if rec.get("kind") == "end":
                ended = True
        out.write(f"Capture errors: {errors}; end record present: {ended}\n")
    print("Report saved; candidate lines require human interpretation.")


def parse_port(value):
    port, sep, baud = value.rpartition(":")
    if not sep or not port or not baud.isdecimal() or not 1 <= int(baud) <= 4_000_000:
        raise argparse.ArgumentTypeError("Use PORT:BAUD, e.g. COM7:115200")
    return port, int(baud)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    live = sub.add_parser("capture")
    for role in ROLES:
        live.add_argument("--" + role, type=parse_port, required=True)
    live.add_argument("--output", required=True)
    live.add_argument("--seconds", type=int, default=0, help="0 = until Ctrl+C")
    offline = sub.add_parser("report")
    offline.add_argument("--input", required=True)
    offline.add_argument("--output", required=True)
    args = parser.parse_args()
    if args.mode == "capture":
        if args.seconds < 0:
            parser.error("seconds cannot be negative")
        capture(args)
    else:
        report(args)


if __name__ == "__main__":
    main()
