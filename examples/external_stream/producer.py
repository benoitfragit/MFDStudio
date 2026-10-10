"""Run with matching-bitness Python and mfd_stream_demo.dll; no external packages.

Pixels are generated in RAM then uploaded to consumer-owned shared GPU textures.
This measures the demonstration upload path, not a direct-GPU producer's speed.
"""
import argparse
import ctypes
import json
import math
import os
from pathlib import Path
import struct
import socket
import subprocess
import sys
import time

from pattern import Pattern


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll", type=Path, help="Path to mfd_stream_demo.dll")
    parser.add_argument("--instance", help="Instance printed by MFDStudio, e.g. 1234-1")
    parser.add_argument("--launch", action="store_true", help="Launch the shipped window automatically")
    parser.add_argument("--runtime", type=Path, help="mfd_window.exe (defaults next to DLL)")
    parser.add_argument("--stream", action="append", dest="streams", help="Repeat for several streams")
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=360)
    parser.add_argument("--format", choices=("rgba", "bgra"), default="rgba")
    parser.add_argument("--fps", type=float, default=50)
    parser.add_argument("--duration", type=float, default=0, help="Seconds; zero runs until Q/Ctrl+C")
    options = parser.parse_args()
    options.streams = options.streams or ["camera_a", "camera_b"]
    if not (1 <= len(options.streams) <= 4) or len(set(options.streams)) != len(options.streams):
        parser.error("Choose one to four distinct stream IDs")
    if not (math.isfinite(options.fps) and 0 < options.fps <= 500):
        parser.error("--fps must be finite and within (0, 500]")
    if not math.isfinite(options.duration) or options.duration < 0:
        parser.error("--duration must be finite and nonnegative")
    if not (1 <= options.width <= 1920 and 1 <= options.height <= 1080):
        parser.error("Maximum resolution is 1920 x 1080; dimensions must be positive")
    if not options.instance and not options.launch:
        parser.error("Pass --instance from the runtime log, or use --launch")
    return options


def load_native_dll(path):
    """Load the adapter and report bitness only for Windows' invalid-image error."""
    try:
        return ctypes.CDLL(str(path))
    except OSError as error:
        if getattr(error, "winerror", None) == 193:
            raise RuntimeError(
                "Cannot load {}: Python and the DLL must have the same architecture "
                "(32 or 64 bits).".format(path)) from error
        raise


def check_launch_port(window):
    """Detect an occupied command port without disturbing an existing runtime.

    The runtime still performs the authoritative bind after this probe closes.
    """
    with window.open(encoding="utf-8-sig") as source:
        udp = json.load(source).get("commands", {}).get("udp", {})
    if not udp.get("enabled", False):
        return
    address, port = udp["address"], udp["port"]
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
                probe.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            probe.bind((address, port))
    except OSError as error:
        raise RuntimeError(
            "Cannot launch MFDStudio: command UDP port {}:{} is unavailable ({}). "
            "Close the previous demo window and retry --launch, or connect to that "
            "window with --instance <pid>-1 without --launch.".format(address, port, error)) from error


class NativeAdapter:
    """Own ctypes declarations and the DLL search-directory lifetime."""

    def __init__(self, path):
        self.directory = os.add_dll_directory(str(path.parent)) if hasattr(os, "add_dll_directory") else None
        if self.directory is None:
            # Python 3.7 uses the legacy Windows dependent-DLL search path.
            os.environ["PATH"] = str(path.parent) + os.pathsep + os.environ.get("PATH", "")
        self.dll = load_native_dll(path)
        declarations = {
            "mfd_stream_create": ([ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32], ctypes.c_uint32),
            "mfd_stream_submit": ([ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint64], ctypes.c_int),
            "mfd_stream_poll": ([ctypes.c_uint32], ctypes.c_int),
            "mfd_stream_close": ([ctypes.c_uint32], None),
            "mfd_stream_error": ([ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32], None),
            "mfd_demo_control": ([ctypes.c_int], ctypes.c_int),
        }
        for name, (params, result) in declarations.items():
            function = getattr(self.dll, name)
            function.argtypes, function.restype = params, result


def find_dll(options):
    if options.dll:
        return options.dll.resolve(strict=True)
    local = Path(__file__).resolve().parent / "mfd_stream_demo.dll"
    if local.exists():
        return local
    root = Path(__file__).resolve().parents[2]
    architecture = "win32" if struct.calcsize("P") == 4 else "x64"
    candidates = sorted((root / "_Exec").glob("*/" + architecture + "/Debug/mfd_stream_demo.dll"))
    if not candidates:
        raise RuntimeError("Build mfd_stream_demo for this Python architecture, or pass --dll")
    return candidates[0]


def main():
    options = arguments()
    if sys.platform != "win32":
        raise RuntimeError("This demonstration requires Windows")
    import msvcrt
    dll_path = find_dll(options)
    native = NativeAdapter(dll_path)
    runtime = None
    ids = []
    try:
        if options.launch:
            executable = options.runtime or dll_path.parent / "mfd_window.exe"
            window = Path(__file__).resolve().parent / "assets/windows/external_stream_window.json"
            if not window.exists():
                window = dll_path.parent / "assets/windows/external_stream_window.json"
            check_launch_port(window)
            # Runtime logs remain visible. The window host owns its first registry (instance suffix 1).
            runtime = subprocess.Popen([str(executable), "--window", str(window.resolve())], cwd=str(executable.parent))
            options.instance = str(runtime.pid) + "-1"
        patterns = []
        for index, name in enumerate(options.streams):
            token = native.dll.mfd_stream_create(options.instance.encode("ascii"), name.encode("ascii"),
                                                  options.width, options.height, 1 if options.format == "rgba" else 2)
            if not token:
                raise RuntimeError("Invalid stream configuration: " + name)
            ids.append(token)
            patterns.append(Pattern(options.width, options.height, options.format, index * 107))
        print("Instance:", options.instance, "| Python:", struct.calcsize("P") * 8, "bits", flush=True)
        print("SPACE producer pause/resume | P reader play | A reader pause | S reader stop | V hide/show | Q quit", flush=True)
        started = time.monotonic()
        report = started
        deadline = started
        accepted = abandoned = errors = frame = 0
        paused, visible = False, True
        last_error = ""
        while not options.duration or time.monotonic() - started < options.duration:
            if runtime is not None and runtime.poll() is not None:
                if runtime.returncode:
                    raise RuntimeError("MFDStudio exited with code {}. See its error message above.".format(runtime.returncode))
                break
            if msvcrt.kbhit():
                key = msvcrt.getwch().lower()
                if key == "q":
                    break
                if key == " ":
                    paused = not paused
                if key in ("p", "a", "s"):
                    native.dll.mfd_demo_control({"p": 0, "a": 1, "s": 2}[key])
                if key == "v":
                    visible = not visible
                    native.dll.mfd_demo_control(4 if visible else 3)
            # Initial command also releases the existing runtime's startup splash.
            if accepted == 0:
                native.dll.mfd_demo_control(0)
            frame += 1
            for token, pattern in zip(ids, patterns):
                if paused:
                    native.dll.mfd_stream_poll(token)
                    continue
                pixels = pattern.update(frame)
                result = native.dll.mfd_stream_submit(token, pixels, len(pattern.buffer), frame)
                if result == 0:
                    accepted += 1
                elif result in (1, 2, 3):
                    abandoned += 1
                else:
                    errors += 1
                    diagnostic = ctypes.create_string_buffer(512)
                    native.dll.mfd_stream_error(token, diagnostic, len(diagnostic))
                    last_error = diagnostic.value.decode("utf-8", errors="replace") or "Result " + str(result)
            now = time.monotonic()
            if now - report >= 1:
                print("frame={} accepted={} abandoned={} errors={} {}".format(frame, accepted, abandoned, errors, last_error), flush=True)
                report = now
            deadline += 1 / options.fps
            if deadline < now:
                deadline = now
            time.sleep(max(0, deadline - time.monotonic()))
    finally:
        for token in ids:
            native.dll.mfd_stream_close(token)
        if runtime is not None and runtime.poll() is None:
            print("Producer closed; MFDStudio remains open to inspect the disconnected state.", flush=True)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except (OSError, RuntimeError, ValueError) as error:
        print("External stream demo:", error, file=sys.stderr)
        sys.exit(1)
