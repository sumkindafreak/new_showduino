import base64
import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[1] / "tools" / "bench_capture.py"
spec = importlib.util.spec_from_file_location("bench_capture", path)
bc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bc)


def rx(role, ns, data):
    return {"kind": "rx", "role": role, "ns": ns,
            "data": base64.b64encode(data).decode("ascii")}


class CaptureTests(unittest.TestCase):
    def test_fragmentation_and_port_isolation(self):
        records = [rx("director", 1, b"REA"), rx("p4", 2, b"BOOT\r\n"),
                   rx("director", 3, b"DY\nnext"), rx("comms", 4, b"OK\n")]
        self.assertEqual(list(bc.iter_lines(records)),
                         [(2, "p4", "BOOT", False), (3, "director", "READY", False),
                          (4, "comms", "OK", False), (None, "director", "next", True)])

    def test_bounded_line_and_binary(self):
        rows = list(bc.iter_lines([rx("p4", 1, b"x" * bc.MAX_LINE + b"\xff\n")]))
        self.assertEqual(len(rows[0][2]), bc.MAX_LINE)
        self.assertTrue(rows[0][3])
        self.assertEqual(rows[1][2], "\ufffd")

    def test_ports(self):
        self.assertEqual(bc.parse_port("COM7:115200"), ("COM7", 115200))
        for bad in ("COM7", ":115200", "COM7:0", "COM7:abc"):
            with self.assertRaises(Exception):
                bc.parse_port(bad)


if __name__ == "__main__":
    unittest.main()
