from __future__ import annotations

import ctypes
from dataclasses import dataclass
from pathlib import Path


class _CaptureArgs(ctypes.Structure):
    _fields_ = [
        ("ip", ctypes.c_char_p),
        ("port", ctypes.c_int),
        ("user", ctypes.c_char_p),
        ("password", ctypes.c_char_p),
        ("channel", ctypes.c_char_p),
    ]


class _CaptureResult(ctypes.Structure):
    _fields_ = [
        ("jpeg_pic", ctypes.POINTER(ctypes.c_ubyte)),
        ("jpeg_pic_len", ctypes.c_size_t),
        ("p2p_data", ctypes.POINTER(ctypes.c_ubyte)),
        ("p2p_data_len", ctypes.c_size_t),
        ("visible_pic", ctypes.POINTER(ctypes.c_ubyte)),
        ("visible_pic_len", ctypes.c_size_t),
    ]


@dataclass
class CaptureResult:
    jpeg_pic: bytes | None
    p2p_data: bytes | None
    visible_pic: bytes | None


def _load_lib() -> ctypes.CDLL:
    lib_path = Path(__file__).with_name("libhcnet_thermal_picture.so")
    return ctypes.CDLL(str(lib_path))


_lib = _load_lib()
_lib.hcnet_capture_picture.argtypes = [ctypes.POINTER(_CaptureArgs), ctypes.POINTER(_CaptureResult)]
_lib.hcnet_capture_picture.restype = ctypes.c_bool
_lib.hcnet_free_buffer.argtypes = [ctypes.POINTER(ctypes.c_ubyte)]
_lib.hcnet_free_buffer.restype = None


def capture_picture(ip: str = "192.168.1.64", port: int = 8000, user: str = "admin", password: str = "abcd1234", channel: str = "1") -> CaptureResult:
    args = _CaptureArgs(ip.encode(), port, user.encode(), password.encode(), channel.encode())
    result = _CaptureResult()
    ok = _lib.hcnet_capture_picture(ctypes.byref(args), ctypes.byref(result))
    if not ok:
        raise RuntimeError("capture failed")

    jpeg = bytes(ctypes.string_at(result.jpeg_pic, result.jpeg_pic_len)) if result.jpeg_pic and result.jpeg_pic_len else None
    p2p = bytes(ctypes.string_at(result.p2p_data, result.p2p_data_len)) if result.p2p_data and result.p2p_data_len else None
    visible = bytes(ctypes.string_at(result.visible_pic, result.visible_pic_len)) if result.visible_pic and result.visible_pic_len else None

    if result.jpeg_pic:
        _lib.hcnet_free_buffer(result.jpeg_pic)
    if result.p2p_data:
        _lib.hcnet_free_buffer(result.p2p_data)
    if result.visible_pic:
        _lib.hcnet_free_buffer(result.visible_pic)

    return CaptureResult(jpeg, p2p, visible)
