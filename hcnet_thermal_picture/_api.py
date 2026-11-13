from __future__ import annotations

import ctypes
import os
import sys
from pathlib import Path
from typing import Optional

MAX_JPEG_SIZE = 8 * 1024 * 1024

QUALITY_BEST = 0
QUALITY_BETTER = 1
QUALITY_NORMAL = 2

STATUS_OK = 0
STATUS_INVALID_ARGUMENT = -1
STATUS_SDK_INIT = -2
STATUS_LOGIN = -3
STATUS_CAPTURE = -4
STATUS_BUFFER_TOO_SMALL = -5
STATUS_UNSUPPORTED_QUALITY = -6
STATUS_NOT_LOGGED_IN = -7
STATUS_OUT_OF_MEMORY = -8
STATUS_LOGOUT = -9

_library: Optional[ctypes.CDLL] = None
_library_path: Optional[Path] = None


class ThermalCameraError(RuntimeError):
    def __init__(self, message: str, *, status: Optional[int] = None, sdk_error: int = 0):
        super().__init__(message)
        self.status = status
        self.sdk_error = sdk_error


def _candidate_library_paths():
    configured = os.environ.get("HCNET_THERMAL_LIBRARY")
    if configured:
        yield Path(configured).expanduser()

    names = (
        "libhcnet_thermal.so",
        "libhcnet_thermal.dylib",
        "hcnet_thermal.dll",
    )
    package_dir = Path(__file__).resolve().parent
    project_dir = package_dir.parent
    roots = (
        package_dir,
        project_dir,
        project_dir / "build",
        project_dir / "build" / "Release",
        project_dir / "build" / "Debug",
    )
    for root in roots:
        for name in names:
            yield root / name


def _configure_library(library: ctypes.CDLL) -> None:
    library.hcnet_thermal_login.argtypes = [
        ctypes.c_char_p,
        ctypes.c_uint16,
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.c_int32,
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(ctypes.c_uint32),
    ]
    library.hcnet_thermal_login.restype = ctypes.c_int

    library.hcnet_thermal_capture_jpeg.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_ubyte),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
        ctypes.POINTER(ctypes.c_uint32),
    ]
    library.hcnet_thermal_capture_jpeg.restype = ctypes.c_int

    library.hcnet_thermal_logout.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_uint32),
    ]
    library.hcnet_thermal_logout.restype = ctypes.c_int

    library.hcnet_thermal_strerror.argtypes = [ctypes.c_int]
    library.hcnet_thermal_strerror.restype = ctypes.c_char_p


def _load_library() -> ctypes.CDLL:
    global _library, _library_path
    if _library is not None:
        return _library

    searched = []
    for path in _candidate_library_paths():
        searched.append(str(path))
        if not path.is_file():
            continue
        try:
            library = ctypes.CDLL(str(path))
        except OSError as exc:
            raise ThermalCameraError(f"unable to load {path}: {exc}") from exc
        _configure_library(library)
        _library = library
        _library_path = path
        return library

    searched_text = "\n  ".join(searched)
    raise ThermalCameraError(
        "hcnet_thermal shared library not found. "
        "Build it with CMake/Make or set HCNET_THERMAL_LIBRARY.\n"
        f"Searched:\n  {searched_text}"
    )


def _error_message(status: int, sdk_error: int) -> str:
    library = _load_library()
    raw = library.hcnet_thermal_strerror(status)
    message = raw.decode("utf-8", "replace") if raw else f"error {status}"
    if sdk_error:
        message += f" (SDK error {sdk_error})"
    return message


class ThermalCamera:
    """A logged-in Hikvision thermal camera session.

    Use it as a context manager, or call close() explicitly. A single handle
    serializes capture calls internally. Do not call close() concurrently with
    capture() on the same instance.
    """

    def __init__(
        self,
        ip: str = "192.168.1.64",
        port: int = 8000,
        user: str = "admin",
        password: str = "abcd1234",
        channel: int = 1,
    ):
        if not 0 < port <= 65535:
            raise ValueError("port must be in 1..65535")
        if channel < 0:
            raise ValueError("channel must be non-negative")

        self._library = _load_library()
        self._handle = ctypes.c_void_p()
        self._buffer = ctypes.create_string_buffer(MAX_JPEG_SIZE)
        sdk_error = ctypes.c_uint32(0)
        status = self._library.hcnet_thermal_login(
            ip.encode("utf-8"),
            ctypes.c_uint16(port),
            user.encode("utf-8"),
            password.encode("utf-8"),
            ctypes.c_int32(channel),
            ctypes.byref(self._handle),
            ctypes.byref(sdk_error),
        )
        if status != STATUS_OK:
            self._handle = ctypes.c_void_p()
            raise ThermalCameraError(
                _error_message(status, sdk_error.value),
                status=status,
                sdk_error=sdk_error.value,
            )

    def capture(self, quality: int = QUALITY_BEST) -> bytes:
        if quality not in (QUALITY_BEST, QUALITY_BETTER, QUALITY_NORMAL):
            raise ValueError("quality must be 0 (best), 1 (better), or 2 (normal)")
        if not self._handle.value:
            raise ThermalCameraError("camera is closed", status=STATUS_NOT_LOGGED_IN)

        out_size = ctypes.c_size_t(0)
        sdk_error = ctypes.c_uint32(0)
        status = self._library.hcnet_thermal_capture_jpeg(
            self._handle,
            ctypes.c_int(quality),
            ctypes.cast(self._buffer, ctypes.POINTER(ctypes.c_ubyte)),
            ctypes.c_size_t(MAX_JPEG_SIZE),
            ctypes.byref(out_size),
            ctypes.byref(sdk_error),
        )
        if status != STATUS_OK:
            raise ThermalCameraError(
                _error_message(status, sdk_error.value),
                status=status,
                sdk_error=sdk_error.value,
            )
        return self._buffer.raw[:out_size.value]

    def close(self) -> None:
        if not self._handle.value:
            return
        sdk_error = ctypes.c_uint32(0)
        status = self._library.hcnet_thermal_logout(self._handle, ctypes.byref(sdk_error))
        self._handle = ctypes.c_void_p()
        if status != STATUS_OK:
            raise ThermalCameraError(
                _error_message(status, sdk_error.value),
                status=status,
                sdk_error=sdk_error.value,
            )

    def __enter__(self) -> "ThermalCamera":
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        try:
            self.close()
        except ThermalCameraError:
            if exc_type is None:
                raise


def capture_picture(
    ip: str = "192.168.1.64",
    port: int = 8000,
    user: str = "admin",
    password: str = "abcd1234",
    channel: int = 1,
    quality: int = QUALITY_BEST,
) -> bytes:
    """Log in, capture one JPEG, log out, and return the JPEG bytes."""
    camera = ThermalCamera(
        ip=ip,
        port=port,
        user=user,
        password=password,
        channel=channel,
    )
    try:
        return camera.capture(quality)
    finally:
        try:
            camera.close()
        except ThermalCameraError:
            pass
