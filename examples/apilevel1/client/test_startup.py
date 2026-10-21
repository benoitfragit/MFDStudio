#!/usr/bin/env python3
"""Verify that the real example client sends startup datagrams at authored defaults."""

import argparse
import json
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile


def main() -> None:
    """Run the client against an isolated loopback receiver and temporary assets."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(("127.0.0.1", 0))
        receiver.settimeout(2.0)
        with tempfile.TemporaryDirectory(prefix="mfd_api1_startup_") as temporary:
            root = Path(temporary)
            shutil.copytree(args.assets, root / "assets")
            window_path = root / "assets/windows/containers_window.json"
            window = json.loads(window_path.read_text(encoding="utf-8"))
            window["commands"]["udp"]["port"] = receiver.getsockname()[1]
            window_path.write_text(json.dumps(window), encoding="utf-8")
            result = subprocess.run(
                [str(args.client.resolve())], cwd=root, capture_output=True,
                text=True, timeout=10, check=True,
            )
            # SendStartup must emit activation and view commands even when the
            # generated handles retain every authored value (an empty diff).
            for command_name in ("page activation", "page view"):
                try:
                    payload, sender = receiver.recvfrom(65535)
                except socket.timeout as error:
                    raise AssertionError(
                        f"No UDP {command_name} received. Client output: {result.stdout} {result.stderr}"
                    ) from error
                if not payload or sender[0] != "127.0.0.1":
                    raise AssertionError(f"Invalid startup datagram from {sender}")
            print("Client startup: two non-empty UDP datagrams received at authored defaults.")


if __name__ == "__main__":
    main()
