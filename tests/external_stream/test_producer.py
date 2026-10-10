"""CPU-only regression tests for demo startup failures (Python 3.7+)."""
import json
from pathlib import Path
import socket
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "examples/external_stream"))
from producer import check_launch_port, load_native_dll


class ProducerStartupTests(unittest.TestCase):
    def test_occupied_port_reports_action_and_probe_releases_free_port(self):
        with tempfile.TemporaryDirectory() as directory:
            window = Path(directory) / "window.json"
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as listener:
                if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
                    listener.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
                listener.bind(("127.0.0.1", 0))
                port = listener.getsockname()[1]
                window.write_text(json.dumps({"commands": {"udp": {
                    "enabled": True, "address": "127.0.0.1", "port": port}}}), encoding="utf-8")
                with self.assertRaises(RuntimeError) as failure:
                    check_launch_port(window)
                self.assertIn(str(port), str(failure.exception))
                self.assertIn("--instance", str(failure.exception))
                self.assertNotIn("architecture", str(failure.exception))
            check_launch_port(window)
            check_launch_port(window)

    def test_architecture_hint_is_specific_to_invalid_image(self):
        failure = OSError("Invalid image")
        failure.winerror = 193
        with patch("producer.ctypes.CDLL", side_effect=failure):
            with self.assertRaisesRegex(RuntimeError, "same architecture"):
                load_native_dll(Path("adapter.dll"))

    def test_missing_dependency_keeps_original_error(self):
        failure = OSError("Missing dependency")
        failure.winerror = 126
        with patch("producer.ctypes.CDLL", side_effect=failure):
            with self.assertRaises(OSError) as reported:
                load_native_dll(Path("adapter.dll"))
        self.assertIs(reported.exception, failure)


if __name__ == "__main__":
    unittest.main()
