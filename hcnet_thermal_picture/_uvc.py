from __future__ import annotations

from os import PathLike
from typing import Union


Device = Union[int, str, PathLike[str]]


def capture_uvc_picture(
    device: Device = 0,
    width: int = 640,
    height: int = 480,
) -> bytes:
    """Capture one frame from a UVC camera and return it as PNG bytes.

    ``device`` can be an OpenCV camera index (for example, ``0``) or a
    device path (for example, ``/dev/video0``).
    """
    if width <= 0 or height <= 0:
        raise ValueError("width and height must be positive")

    import cv2

    capture = cv2.VideoCapture(str(device) if isinstance(device, PathLike) else device)
    try:
        if not capture.isOpened():
            raise RuntimeError(f"unable to open UVC camera: {device}")

        capture.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        capture.set(cv2.CAP_PROP_FRAME_HEIGHT, height)

        ret, frame = capture.read()
        if not ret or frame is None:
            raise RuntimeError(f"unable to read a frame from UVC camera: {device}")

        encoded, image = cv2.imencode(".png", frame)
        if not encoded:
            raise RuntimeError("unable to encode UVC camera frame as PNG")

        return image.tobytes()
    finally:
        capture.release()
