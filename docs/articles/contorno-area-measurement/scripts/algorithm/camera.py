"""Part II: from pixels to square millimetres, Eqs. (1) and (2), with the camera completed from the photograph (Section 4.2).

    camera = resolve(Configuration(...), photo_path)   # focal length, pitch and their sources
    camera.scale_mm_per_px                              # Eq. (1)
    area_mm2(mask, camera)                              # Eq. (2)
"""
from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np
from PIL import ExifTags, Image

FULL_FRAME_DIAGONAL_MM = 43.27


@dataclass
class Configuration:
    """The minimal configuration of Section 1.2; the sensor by its size, or else by its pixel pitch."""
    distance_mm: float
    focal_length_mm: float
    sensor_mm: tuple[float, float] | None = None
    pixel_um: float | None = None


@dataclass
class Camera:
    distance_mm: float
    focal_length_mm: float
    pixel_um: tuple[float, float]  # along the photo's columns and rows
    focal_source: str              # "photo" or "configuration"
    pixel_source: str              # "photo", "photo, 35 mm equivalent" or "configuration"

    @property
    def scale_mm_per_px(self) -> tuple[float, float]:
        """Eq. (1): s = d p / f, along columns and rows."""
        return tuple(self.distance_mm * p / 1000 / self.focal_length_mm for p in self.pixel_um)


def exif_values(photo_path: str) -> dict:
    """The EXIF values Section 4.2 uses, and the photo's size after its EXIF orientation."""
    with Image.open(photo_path) as image:
        exif = image.getexif()
        tags = {ExifTags.TAGS.get(k, k): v for k, v in list(exif.items()) + list(exif.get_ifd(0x8769).items())}
        width, height = image.size
    if tags.get("Orientation") in (5, 6, 7, 8):  # shown rotated by 90°: the sides swap
        width, height = height, width
    return {"tags": tags, "width": width, "height": height}


def resolve(config: Configuration, photo_path: str) -> Camera:
    """The camera of a photo: its EXIF values where valid, else the configuration; the distance always configured."""
    found = exif_values(photo_path)
    tags, w, h = found["tags"], found["width"], found["height"]
    long_px, short_px = max(w, h), min(w, h)
    focal, focal_source = config.focal_length_mm, "configuration"
    f_photo = float(tags["FocalLength"]) if tags.get("FocalLength") else None
    if f_photo is not None and 0.1 <= f_photo <= 2000:
        focal, focal_source = f_photo, "photo"
    pitch, pixel_source = None, "configuration"
    # 1. the focal-plane resolution, scaled when the photo was resized after capture
    unit_mm = {2: 25.4, 3: 10.0, 4: 1.0, 5: 0.001}.get(int(tags.get("FocalPlaneResolutionUnit", 2)))
    res_x, res_y = tags.get("FocalPlaneXResolution"), tags.get("FocalPlaneYResolution")
    if unit_mm and res_x and float(res_x) > 0:
        px = 1000 * unit_mm / float(res_x)
        py = 1000 * unit_mm / float(res_y) if res_y and float(res_y) > 0 else px
        dims = tags.get("ExifImageWidth"), tags.get("ExifImageHeight")
        if all(dims) and max(dims) != long_px:
            px, py = px * max(dims) / long_px, py * max(dims) / long_px
        if 0.1 <= px <= 100 and 0.1 <= py <= 100:
            pitch, pixel_source = (px, py), "photo"
    # 2. the 35 mm equivalent focal length, with the photo's own focal length
    f35 = tags.get("FocalLengthIn35mmFilm")
    if pitch is None and focal_source == "photo" and f35 and float(f35) > 0:
        diagonal = FULL_FRAME_DIAGONAL_MM / (float(f35) / focal)
        p = 1000 * diagonal * long_px / math.hypot(long_px, short_px) / long_px
        if 0.1 <= p <= 100:
            pitch, pixel_source = (p, p), "photo, 35 mm equivalent"
    # 3. the configuration: the sensor's long side over the photo's long side, or the pitch as given
    if pitch is None:
        p = 1000 * max(config.sensor_mm) / long_px if config.sensor_mm else config.pixel_um
        pitch = (p, p)
    return Camera(config.distance_mm, focal, pitch, focal_source, pixel_source)


def area_mm2(mask: np.ndarray, camera: Camera) -> float:
    """Eq. (2): A = N s_x s_y, counted on the pixels of the mask."""
    sx, sy = camera.scale_mm_per_px
    return np.count_nonzero(mask) * sx * sy
