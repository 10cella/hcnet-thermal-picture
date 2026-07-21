from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path

def capture_picture(ip: str = "192.168.1.64", port: int = 8000, user: str = "admin", password: str = "abcd1234", channel: str = "1") -> bytes:
    cli_path = Path(__file__).resolve().parent.parent / "hcnet_capture_cli"
    if not cli_path.is_file():
        raise RuntimeError(f"C++ capture executable not found: {cli_path}")

    with tempfile.TemporaryDirectory() as output_dir:
        completed = subprocess.run(
            [str(cli_path), "--ip", ip, "--port", str(port), "--user", user, "--password", password, "--channel", channel, "--out", output_dir],
            capture_output=True,
            text=True,
        )
        if completed.returncode:
            raise RuntimeError(f"capture failed: {completed.stderr.strip() or completed.stdout.strip()}")

        output_path = Path(output_dir) / "thermal.jpg"
        if not output_path.is_file():
            raise RuntimeError("capture failed: C++ process produced no JPEG")
        return output_path.read_bytes()
