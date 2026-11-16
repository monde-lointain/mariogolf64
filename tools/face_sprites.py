#!/usr/bin/env python3
"""Draw custom characters' face sprites, which the menus and scorecards show.

    face_sprites.py --assets ARCHIVE --output DIR PACKAGE...

A character's face sprites are ten small pictures: the character at rest,
four of it pleased, four of it beaten and a head for the menus. They share
four palettes, one for each of the character's outfits. face_sprites.json
holds, for each slot whose pictures it knows, the camera, pose, face and
light of each, fitted to the original's; a custom character is drawn the
same way, its head framed as the original's is.

The pictures go into a sheet: a column for each picture and a row for each
outfit. Clear pixels are the background; half-clear ones are the edge,
which the game blends into what's behind; opaque ones are the character. An
artist can paint over the sheet that this writes, DIR/SLOT_face_sprites.png,
and save it in the package as face_sprites.png; this then takes that in
place of drawing it.

The pictures' asset files go in DIR/asset_archive, for asset_archive.py to
pack in place of the originals in ARCHIVE, the directory that make splits
the archive into. DIR/face_sprites.txt, written last, lists the files.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import TypeVar, cast, final

import numpy as np
import numpy.typing as npt
from PIL import Image

import build_character as bc
import character_template as tmpl
import decode_asset

FloatArray = npt.NDArray[np.float64]
IntArray = npt.NDArray[np.int64]
ByteArray = npt.NDArray[np.uint8]
BoolArray = npt.NDArray[np.bool_]
ScalarT = TypeVar("ScalarT", bound=np.generic)
Box = tuple[float, float, float, float]
Vector = tuple[float, float, float]

POSES_FILE = Path(__file__).resolve().parent / "face_sprites.json"
SUMMARY = "Draw custom characters' face sprites, for the menus and scorecards."
SHEET_FILE = "face_sprites.png"
LIST_FILE = "face_sprites.txt"
# Where in the output the assets that replace the archive's go
ARCHIVE_DIRECTORY = "asset_archive"
WIDTH = 32
HEIGHT = 32
# The character's colors, then its three palettes'
OUTFIT_COUNT = 4
RGB = 3
RGBA = 4
HEAD_BONE = "head"
# Each pixel is the average of SUPERSAMPLE x SUPERSAMPLE samples
SUPERSAMPLE = 4
# The sheet's alpha for the edge's pixels. The game stores them clear, with
# their color darkened by how little of them the character covers, as if
# drawn over black, and its filtering blends them into the character's edge.
EDGE_ALPHA = 128
OPAQUE = 255
CHANNEL_MAX = 255
# Palette entries that every character's palettes keep: clear black and the
# background behind the pictures
CLEAR_INDEX = 0
BACKGROUND_INDEX = 4
KEPT_INDICES = (CLEAR_INDEX, BACKGROUND_INDEX)
PALETTE_SIZE = 256
PALETTE_ENTRY = struct.Struct(">H")
# An RGBA16 color: 5 bits of red, green and blue, then 1 of alpha
CHANNEL_LEVELS = 31
CHANNEL_SHIFTS = (11, 6, 1)
OPAQUE_BIT = 1
# The edge's colors get at least this many palette entries, and at most half
EDGE_ENTRIES_MIN = 16
CLUSTER_ITERATIONS = 30
# A triangle whose projected area is smaller than this, in samples, is edge-on
AREA_MIN = 1e-9
HEX = 16


class SpriteError(ValueError):
    """A package's face sprites can't be made, and why."""


# numpy's stubs name, in some overloads of these functions, types that the
# type checker's Python 3.10 lacks, which leaves them partly unknown to it.
# These give them the types that this module uses them with.


def _join(arrays: list[npt.NDArray[ScalarT]], axis: int = 0) -> npt.NDArray[ScalarT]:
    """Return ARRAYS joined along AXIS."""
    return np.concatenate(arrays, axis=axis)  # pyright: ignore[reportUnknownMemberType]


def _stack(arrays: list[npt.NDArray[ScalarT]]) -> npt.NDArray[ScalarT]:
    """Return ARRAYS stacked along a new last axis."""
    return np.stack(arrays, axis=-1)  # pyright: ignore[reportUnknownMemberType]


def _reshape(array: npt.NDArray[ScalarT], *shape: int) -> npt.NDArray[ScalarT]:
    """Return ARRAY reshaped."""
    return array.reshape(shape)  # pyright: ignore[reportUnknownMemberType]


def _transpose(matrix: FloatArray) -> FloatArray:
    """Return MATRIX transposed."""
    return matrix.T  # pyright: ignore[reportUnknownMemberType, reportUnknownVariableType]


def _clip(array: FloatArray, low: float, high: float) -> FloatArray:
    """Return ARRAY clipped to LOW and HIGH."""
    return np.clip(array, low, high)  # pyright: ignore[reportUnknownMemberType]


def _where(
    condition: BoolArray, chosen: npt.NDArray[ScalarT], other: npt.NDArray[ScalarT]
) -> npt.NDArray[ScalarT]:
    """Return CHOSEN where CONDITION holds, else OTHER."""
    return np.where(condition, chosen, other)  # pyright: ignore[reportUnknownMemberType]


def _distinct_rows(rows: FloatArray) -> tuple[FloatArray, IntArray, IntArray]:
    """Return the distinct ROWS, each row's index among them, and their counts."""
    distinct, inverse, counts = np.unique(  # pyright: ignore[reportUnknownMemberType]
        rows, axis=0, return_inverse=True, return_counts=True
    )
    return distinct, _reshape(inverse, -1), counts


def _shape(array: npt.NDArray[np.generic]) -> tuple[int, ...]:
    """Return ARRAY's shape."""
    return cast("tuple[int, ...]", array.shape)


def _count(mask: BoolArray) -> int:
    """Return how many of MASK hold."""
    return int(mask.sum())


def _open_rgba(path: Path) -> ByteArray:
    """Return an image's RGBA pixels."""
    # Not a with statement: Pillow's __enter__ returns typing.Self, which
    # the type checker's Python 3.10 lacks
    opened = Image.open(path)
    try:
        image = opened.convert("RGBA")
    finally:
        opened.close()
    return np.asarray(image, np.uint8)


def turn_about(axis: int, degrees: float) -> FloatArray:
    """Return the matrix turning about the X, Y or Z AXIS by DEGREES."""
    cos, sin = math.cos(math.radians(degrees)), math.sin(math.radians(degrees))
    first, second = [i for i in range(3) if i != axis]
    matrix = np.eye(3)
    matrix[first, first] = matrix[second, second] = cos
    # About Y, Z turns toward X; about X and Z, the second axis toward the first
    sign = -1.0 if axis == 1 else 1.0
    matrix[first, second] = -sin * sign
    matrix[second, first] = sin * sign
    return matrix


def rotation(degrees: Vector) -> FloatArray:
    """Return the matrix turning about X, then Y, then Z, by DEGREES."""
    return (
        turn_about(2, degrees[2])
        @ turn_about(1, degrees[1])
        @ turn_about(0, degrees[0])
    )


@dataclass
class Camera:
    """An orthographic camera, as fit to an original picture.

    yaw turns it about Y toward the character's left, pitch raises it and
    roll turns the picture, in degrees. The target point lands at the pixel
    position center, scale pixels a model unit.
    """

    yaw: float
    pitch: float
    roll: float
    scale: float
    center: tuple[float, float]
    target: Vector

    def view(self) -> FloatArray:
        """Return the rotation from model axes to the camera's: x right, y up, z out."""
        return (
            turn_about(2, self.roll)
            @ turn_about(0, self.pitch)
            @ turn_about(1, -self.yaw)
        )

    def project(self, points: FloatArray) -> FloatArray:
        """Return POINTS' pixel x, y and depth (larger is nearer) in the last axis."""
        seen = (points - np.array(self.target)) @ _transpose(self.view())
        x = seen[..., 0] * self.scale + self.center[0]
        y = -seen[..., 1] * self.scale + self.center[1]
        return _stack([x, y, seen[..., 2]])

    def framed(self, zoom: float, shift: tuple[float, float]) -> Camera:
        """Return the camera zoomed about its center, then moved by SHIFT pixels."""
        return Camera(
            self.yaw,
            self.pitch,
            self.roll,
            self.scale * zoom,
            (self.center[0] + shift[0], self.center[1] + shift[1]),
            self.target,
        )

    def finer(self, factor: int) -> Camera:
        """Return the camera for a picture FACTOR times as many pixels across."""
        return Camera(
            self.yaw,
            self.pitch,
            self.roll,
            self.scale * factor,
            (self.center[0] * factor, self.center[1] * factor),
            self.target,
        )


@dataclass
class Picture:
    """One of the original's pictures, and how to draw a character as it."""

    asset: int
    name: str
    height: int
    camera: Camera
    pose: dict[str, Vector]
    # The face image each face group shows: a model kind and texture index
    faces: dict[str, tuple[str, int]]
    # Only these bones' parts are drawn, if any are named
    bones: list[str]
    # The picture whose head fixes this one's framing
    framing: str


@dataclass
class SpriteSet:
    """How a slot's face sprites were drawn."""

    first_palette: int
    light: Vector
    ambient: float
    pictures: list[Picture]
    # Where the original's head lies in a picture: x0, y0, x1, y1 pixels
    head_boxes: dict[str, Box]


@dataclass
class Mesh:
    """Triangles to draw, posed."""

    positions: FloatArray  # (N, 3, 3)
    normals: FloatArray  # (N, 3, 3)
    uvs: FloatArray  # (N, 3, 2), V up
    colors: FloatArray  # (outfits, N, 3), 0 to 1
    images: list[FloatArray]  # RGBA, 0 to 1
    image: IntArray  # Each triangle's index in images, or -1
    double_sided: BoolArray


def _numbers(value: object, count: int) -> list[float]:
    values = cast("list[float]", value)
    if len(values) != count:
        msg = f"{POSES_FILE} has {values} where it should have {count} numbers"
        raise SpriteError(msg)
    return [float(v) for v in values]


def _vector(value: object) -> Vector:
    x, y, z = _numbers(value, 3)
    return (x, y, z)


def _read_picture(value: dict[str, object], target: Vector) -> Picture:
    camera = cast("dict[str, float | list[float]]", value["camera"])
    center = _numbers(camera["center"], 2)
    pose = cast("dict[str, list[float]]", value.get("pose", {}))
    faces = cast("dict[str, tuple[str, int]]", value.get("faces", {}))
    return Picture(
        int(cast("str", value["asset"]), HEX),
        cast("str", value["name"]),
        cast("int", value.get("height", HEIGHT)),
        Camera(
            float(cast("float", camera["yaw"])),
            float(cast("float", camera["pitch"])),
            float(cast("float", camera["roll"])),
            float(cast("float", camera["scale"])),
            (center[0], center[1]),
            target,
        ),
        {bone: _vector(angles) for bone, angles in pose.items()},
        {group: (str(face[0]), int(face[1])) for group, face in faces.items()},
        cast("list[str]", value.get("bones", [])),
        cast("str", value["framing"]),
    )


def _box(value: object) -> Box:
    x0, y0, x1, y1 = _numbers(value, 4)
    return (x0, y0, x1, y1)


def load_sprite_sets(path: Path = POSES_FILE) -> dict[str, SpriteSet]:
    """Read face_sprites.json: each slot's pictures, by slot."""
    document = cast("dict[str, dict[str, object]]", json.loads(path.read_text()))
    sets: dict[str, SpriteSet] = {}
    for slot, entry in document.items():
        light = cast("dict[str, object]", entry["light"])
        target = _vector(entry["target"])
        boxes = cast("dict[str, list[float]]", entry["head_boxes"])
        sets[slot] = SpriteSet(
            int(cast("str", entry["first_palette"]), HEX),
            _vector(light["direction"]),
            float(cast("float", light["ambient"])),
            [
                _read_picture(picture, target)
                for picture in cast("list[dict[str, object]]", entry["pictures"])
            ],
            {name: _box(box) for name, box in boxes.items()},
        )
    return sets


def bone_heads(
    template: tmpl.CharacterTemplate, skeleton: dict[str, tmpl.Vec3]
) -> dict[str, FloatArray]:
    """Return where each bone's joint is: SKELETON's, else moved with its parent's."""
    heads: dict[str, FloatArray] = {}
    moved: dict[int, FloatArray] = {}
    for bone in template.bones:  # Parents come first
        head = np.array(bone.head, np.float64)
        if bone.name in skeleton:
            moved[bone.joint] = np.array(skeleton[bone.name], np.float64) - head
        else:
            moved[bone.joint] = moved.get(bone.parent, np.zeros(3))
        heads[bone.name] = head + moved[bone.joint]
    return heads


def posed_bones(
    template: tmpl.CharacterTemplate,
    heads: dict[str, FloatArray],
    pose: dict[str, Vector],
) -> dict[str, tuple[FloatArray, FloatArray]]:
    """Return each bone's rotation and offset in POSE, turning it about its joint."""
    by_joint = {bone.joint: bone.name for bone in template.bones}
    world: dict[str, tuple[FloatArray, FloatArray]] = {}
    for bone in template.bones:
        turn = rotation(pose.get(bone.name, (0.0, 0.0, 0.0)))
        head = heads[bone.name]
        offset = head - turn @ head
        parent = by_joint.get(bone.parent)
        if parent is not None:
            parent_turn, parent_offset = world[parent]
            turn, offset = parent_turn @ turn, parent_turn @ offset + parent_offset
        world[bone.name] = (turn, offset)
    return world


def _material_image(
    material: bc.Material, faces: dict[str, tuple[str, int]]
) -> Path | None:
    """Return the image a material shows in a picture with FACES, if any."""
    if material.face is not None and material.face.group in faces:
        kind, index = faces[material.face.group]
        chosen = material.face.images.get(kind, {}).get(index)
        if chosen is not None:
            return chosen
    return material.texture


def package_mesh(
    package: bc.Package,
    world: dict[str, tuple[FloatArray, FloatArray]],
    picture: Picture,
) -> Mesh:
    """Return a package's parts posed as WORLD has its bones, for PICTURE."""
    positions: list[FloatArray] = []
    normals: list[FloatArray] = []
    uvs: list[FloatArray] = []
    colors: list[FloatArray] = []
    image: list[IntArray] = []
    sides: list[BoolArray] = []
    paths: list[Path] = []
    for part in package.parts:
        if picture.bones and part.bone not in picture.bones:
            continue
        turn, offset = world[part.bone]
        corners = np.array(part.triangles, np.int64)
        count = len(corners)
        positions.append(
            np.array(part.positions, np.float64)[corners] @ _transpose(turn) + offset
        )
        normals.append(np.array(part.normals, np.float64)[corners] @ _transpose(turn))
        uvs.append(np.array(part.uvs, np.float64)[corners])
        material = package.materials[part.material]
        outfits = np.array(
            [color[:RGB] for color in (material.color, *material.palettes)],
            np.float64,
        )
        colors.append(np.ones((1, count, 1)) * outfits[:, None] / CHANNEL_MAX)
        path = _material_image(material, picture.faces)
        if path is not None and path not in paths:
            paths.append(path)
        image.append(np.full(count, -1 if path is None else paths.index(path)))
        sides.append(np.full(count, material.double_sided))
    if not positions:
        msg = f"{package.name} has no parts on {', '.join(picture.bones)} to draw"
        raise SpriteError(msg)
    return Mesh(
        _join(positions),
        _join(normals),
        _join(uvs),
        _join(colors, axis=1),
        [_open_rgba(path) / CHANNEL_MAX for path in paths],
        _join(image),
        _join(sides),
    )


def head_box(points: FloatArray, camera: Camera) -> Box:
    """Return the pixel box of POINTS through CAMERA: x0, y0, x1, y1."""
    projected = camera.project(points)
    x, y = projected[..., 0], projected[..., 1]
    return (float(x.min()), float(y.min()), float(x.max()), float(y.max()))


def framing(
    want: Box, have: Box, center: tuple[float, float]
) -> tuple[float, tuple[float, float]]:
    """Return the zoom about CENTER and the shift that fit box HAVE into box WANT."""
    zoom = min(
        (want[2] - want[0]) / (have[2] - have[0]),
        (want[3] - want[1]) / (have[3] - have[1]),
    )
    zoomed = [(have[i] - center[i % 2]) * zoom + center[i % 2] for i in range(4)]
    shift = (
        (want[0] + want[2] - zoomed[0] - zoomed[2]) / 2,
        (want[1] + want[3] - zoomed[1] - zoomed[3]) / 2,
    )
    return zoom, shift


@dataclass
class Samples:
    """What each sample of a picture sees: its triangle and where on it."""

    triangle: IntArray  # (H, W), -1 where nothing
    weights: FloatArray  # (H, W, 3) barycentric


def rasterize(projected: FloatArray, width: int, height: int) -> Samples:
    """Return the nearest triangle at each sample's center, and where on it."""
    depth = np.full((height, width), -np.inf)
    triangle = np.full((height, width), -1, np.int64)
    weights = np.zeros((height, width, 3))
    corners = cast("list[list[list[float]]]", projected.tolist())
    for index, ((ax, ay, az), (bx, by, bz), (cx, cy, cz)) in enumerate(corners):
        x0, x1 = (
            max(math.floor(min(ax, bx, cx)), 0),
            min(math.ceil(max(ax, bx, cx)), width - 1),
        )
        y0, y1 = (
            max(math.floor(min(ay, by, cy)), 0),
            min(math.ceil(max(ay, by, cy)), height - 1),
        )
        area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)
        if x0 > x1 or y0 > y1 or abs(area) < AREA_MIN:
            continue
        # The samples' centers in the triangle's box
        px = np.arange(x0, x1 + 1)[None, :] + 0.5
        py = np.arange(y0, y1 + 1)[:, None] + 0.5
        w0: FloatArray = ((bx - px) * (cy - py) - (cx - px) * (by - py)) / area
        w1: FloatArray = ((cx - px) * (ay - py) - (ax - px) * (cy - py)) / area
        w2: FloatArray = 1 - w0 - w1
        z: FloatArray = w0 * az + w1 * bz + w2 * cz
        box = (slice(y0, y1 + 1), slice(x0, x1 + 1))
        nearer: BoolArray = (
            np.greater_equal(w0, 0)
            & np.greater_equal(w1, 0)
            & np.greater_equal(w2, 0)
            & np.greater(z, depth[box])
        )
        depth[box] = _where(nearer, z, depth[box])
        triangle[box] = _where(
            nearer, np.full_like(triangle[box], index), triangle[box]
        )
        weights[box] = _where(nearer[..., None], _stack([w0, w1, w2]), weights[box])
    return Samples(triangle, weights)


def _texels(image: FloatArray, uv: FloatArray) -> FloatArray:
    """Return the texels of IMAGE nearest each UV (V up)."""
    rows, columns = _shape(image)[:2]
    column = _clip(np.floor(uv[:, 0] * columns), 0, columns - 1).astype(np.int64)
    row = _clip(np.floor((1 - uv[:, 1]) * rows), 0, rows - 1).astype(np.int64)
    return image[row, column]


def shade(
    mesh: Mesh, samples: Samples, camera: Camera, sprites: SpriteSet
) -> FloatArray:
    """Return each outfit's lit color at each sample, black where nothing is.

    The light is fixed to the camera: a sample is lit by the ambient share,
    plus the rest as its normal faces the light.
    """
    covered = samples.triangle >= 0
    hit = samples.triangle[covered]
    weights = samples.weights[covered][:, :, None]
    view = _transpose(camera.view())
    normal = (mesh.normals[hit] * weights).sum(axis=1) @ view
    normal /= np.sqrt((normal * normal).sum(axis=-1, keepdims=True)) + AREA_MIN
    # A double-sided triangle seen from behind shows its other side
    corners = mesh.positions[hit]
    facing = np.cross(corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0])
    behind = ((facing @ view)[:, 2] < 0) & mesh.double_sided[hit]
    normal[behind] *= -1
    light = np.array(sprites.light) / math.hypot(*sprites.light)
    lit = sprites.ambient + (1 - sprites.ambient) * np.maximum(normal @ light, 0)
    albedo = mesh.colors[:, hit]  # (outfits, samples, 3)
    uv = (mesh.uvs[hit] * weights).sum(axis=1)
    for index, image in enumerate(mesh.images):
        uses: BoolArray = np.equal(mesh.image[hit], index)
        texels = _texels(image, uv[uses])
        alpha = texels[:, RGB:]
        albedo[:, uses] = texels[None, :, :RGB] * alpha + albedo[:, uses] * (1 - alpha)
    out = np.zeros((OUTFIT_COUNT, *covered.shape, RGB))
    out[:, covered] = _clip(albedo * lit[None, :, None], 0, 1)
    return out


def draw_picture(
    mesh: Mesh, camera: Camera, picture: Picture, sprites: SpriteSet
) -> ByteArray:
    """Return a picture as the sheet holds it: (outfits, height, WIDTH, RGBA)."""
    fine = camera.finer(SUPERSAMPLE)
    samples = rasterize(
        fine.project(mesh.positions), WIDTH * SUPERSAMPLE, picture.height * SUPERSAMPLE
    )
    colors = shade(mesh, samples, camera, sprites)
    coverage = _reshape(
        (samples.triangle >= 0).astype(np.float64),
        picture.height,
        SUPERSAMPLE,
        WIDTH,
        SUPERSAMPLE,
    ).mean(axis=(1, 3))
    color = _reshape(
        colors, OUTFIT_COUNT, picture.height, SUPERSAMPLE, WIDTH, SUPERSAMPLE, RGB
    ).mean(axis=(2, 4))
    rgba = np.zeros((OUTFIT_COUNT, picture.height, WIDTH, RGBA), np.uint8)
    rgba[..., :RGB] = np.round(color * CHANNEL_MAX).astype(np.uint8)
    rgba[..., RGB] = EDGE_ALPHA * np.greater(coverage, 0)
    rgba[..., RGB][:, np.greater_equal(coverage, 1)] = OPAQUE
    return rgba


def draw_sheet(
    package: bc.Package, template: tmpl.CharacterTemplate, sprites: SpriteSet
) -> ByteArray:
    """Return the package's face sprites: a row for each outfit, a column a picture."""
    heads = bone_heads(template, package.skeleton)
    on_head = [
        np.array(part.positions, np.float64)
        for part in package.parts
        if part.bone == HEAD_BONE
    ]
    if not on_head:
        msg = (
            f"{package.name} has no parts on the {HEAD_BONE} bone, which the face "
            "sprites are framed by"
        )
        raise SpriteError(msg)
    head_points = _join(on_head)
    by_name = {picture.name: picture for picture in sprites.pictures}
    zooms: dict[str, tuple[float, tuple[float, float]]] = {}
    sheet = np.zeros(
        (OUTFIT_COUNT * HEIGHT, len(sprites.pictures) * WIDTH, RGBA), np.uint8
    )
    for column, picture in enumerate(sprites.pictures):
        if picture.framing not in zooms:
            framer = by_name[picture.framing]
            turn, offset = posed_bones(template, heads, framer.pose)[HEAD_BONE]
            have = head_box(head_points @ _transpose(turn) + offset, framer.camera)
            want = sprites.head_boxes[picture.framing]
            zooms[picture.framing] = framing(want, have, framer.camera.center)
        zoom, shift = zooms[picture.framing]
        world = posed_bones(template, heads, picture.pose)
        drawn = draw_picture(
            package_mesh(package, world, picture),
            picture.camera.framed(zoom, shift),
            picture,
            sprites,
        )
        left = column * WIDTH
        for outfit in range(OUTFIT_COUNT):
            top = outfit * HEIGHT
            sheet[top : top + picture.height, left : left + WIDTH] = drawn[outfit]
    return sheet


def read_sheet(path: Path, sprites: SpriteSet) -> ByteArray:
    """Return an artist's sheet, checking that it's the size this draws."""
    sheet = _open_rgba(path)
    height, width = _shape(sheet)[:2]
    size = (len(sprites.pictures) * WIDTH, OUTFIT_COUNT * HEIGHT)
    if (width, height) != size:
        msg = (
            f"{path} is {width}x{height} pixels, but a face sprite sheet is "
            f"{size[0]}x{size[1]}: a {WIDTH}-pixel column for each picture and a "
            f"{HEIGHT}-pixel row for each outfit. Start from the sheet that the "
            "build writes, SLOT_face_sprites.png"
        )
        raise SpriteError(msg)
    return sheet


def _cluster(
    points: FloatArray, counts: FloatArray, size: int
) -> tuple[FloatArray, IntArray]:
    """Return SIZE centers of weighted POINTS (k-means), and each point's center."""
    if len(points) <= size:
        return points, np.arange(len(points))
    # Each first center is the point that most pixels are far from the others'
    middle = np.zeros((size, _shape(points)[1]))
    first = int(counts.argmax())
    middle[0:1] = points[first : first + 1]
    nearest = np.square(points - middle[0:1]).sum(axis=1)
    for center in range(1, size):
        chosen = int((nearest * counts).argmax())
        middle[center : center + 1] = points[chosen : chosen + 1]
        far = np.square(points - middle[center : center + 1]).sum(axis=1)
        nearest = np.minimum(nearest, far)
    labels = np.zeros(len(points), np.int64)
    for _ in range(CLUSTER_ITERATIONS):
        distance = (
            np.square(points).sum(axis=1)[:, None]
            - 2 * points @ _transpose(middle)
            + np.square(middle).sum(axis=1)[None]
        )
        labels = distance.argmin(axis=1)
        weight = np.bincount(labels, counts, size)
        used: BoolArray = np.greater(weight, 0)
        for axis in range(_shape(points)[1]):
            total = np.bincount(labels, points[:, axis] * counts, size)
            middle[used, axis] = total[used] / weight[used]
    return middle, labels


@dataclass
class FaceSprites:
    """Face sprites as the game stores them: four palettes and indexed pictures."""

    palettes: list[list[int]]  # RGBA16 colors, by outfit
    pictures: list[ByteArray]  # (height, WIDTH) palette indices


def to_rgba16(levels: list[float], *, opaque: bool) -> int:
    """Return an RGBA16 color with red, green and blue LEVELS from 0 to 31."""
    color = int(opaque)
    for level, shift in zip(levels, CHANNEL_SHIFTS, strict=True):
        color |= min(max(round(level), 0), CHANNEL_LEVELS) << shift
    return color


def from_rgba16(color: int) -> tuple[int, int, int, bool]:
    """Return an RGBA16 color's red, green and blue from 0 to 255, and if opaque.

    Each channel widens as the RDP's does, its top bits repeated below.
    """
    red, green, blue = (
        decode_asset.widen_channel(color >> shift & CHANNEL_LEVELS)
        for shift in CHANNEL_SHIFTS
    )
    return red, green, blue, bool(color & OPAQUE_BIT)


def index_sheet(
    sheet: ByteArray, sprites: SpriteSet, originals: list[list[int]]
) -> FaceSprites:
    """Return a sheet's pictures in palettes that share their indices.

    Each palette keeps the original's clear black and background entries;
    the rest are the most telling colors of the opaque pixels and of the
    edge's, so that a pixel's index gives its color in every outfit.
    """
    # Each pixel's colors in all four outfits, side by side, picture by picture
    pixels = _join(
        [
            _reshape(
                _join(
                    [
                        sheet[
                            outfit * HEIGHT : outfit * HEIGHT + picture.height,
                            column * WIDTH : (column + 1) * WIDTH,
                        ]
                        for outfit in range(OUTFIT_COUNT)
                    ],
                    axis=-1,
                ),
                -1,
                RGBA * OUTFIT_COUNT,
            )
            for column, picture in enumerate(sprites.pictures)
        ]
    )
    alpha = pixels[:, RGB]
    colors = _join(
        [
            pixels[:, outfit * RGBA : outfit * RGBA + RGB].astype(np.float64)
            for outfit in range(OUTFIT_COUNT)
        ],
        axis=1,
    )
    levels = np.round(colors * CHANNEL_LEVELS / CHANNEL_MAX)
    free = [i for i in range(PALETTE_SIZE) if i not in KEPT_INDICES]
    solid: BoolArray = np.equal(alpha, OPAQUE)
    edge: BoolArray = np.greater(alpha, 0) & ~solid
    edge_entries = 0
    if edge.any():
        share = _count(edge) / _count(edge | solid)
        edge_entries = min(
            max(round(len(free) * share), EDGE_ENTRIES_MIN), len(free) // 2
        )
    palettes = [
        [originals[outfit][i] if i in KEPT_INDICES else 0 for i in range(PALETTE_SIZE)]
        for outfit in range(OUTFIT_COUNT)
    ]
    index = np.full(len(pixels), BACKGROUND_INDEX, np.int64)
    for chosen, entries, opaque in (
        (edge, free[:edge_entries], False),
        (solid, free[edge_entries:], True),
    ):
        if not chosen.any():
            continue
        # Each distinct color once, weighted by how many pixels have it
        distinct, which, counts = _distinct_rows(levels[chosen])
        centers, labels = _cluster(distinct, counts.astype(np.float64), len(entries))
        index[chosen] = np.array(entries, np.int64)[labels[which]]
        center_levels = cast("list[list[float]]", centers.tolist())
        for center, entry in zip(center_levels, entries, strict=False):
            for outfit in range(OUTFIT_COUNT):
                palettes[outfit][entry] = to_rgba16(
                    center[outfit * RGB : (outfit + 1) * RGB], opaque=opaque
                )
    pictures: list[ByteArray] = []
    start = 0
    for picture in sprites.pictures:
        count = picture.height * WIDTH
        pictures.append(
            _reshape(
                index[start : start + count].astype(np.uint8), picture.height, WIDTH
            )
        )
        start += count
    return FaceSprites(palettes, pictures)


def faces_sheet(sprites: SpriteSet, faces: FaceSprites) -> ByteArray:
    """Return FACES as a sheet, in the colors that the game shows."""
    sheet = np.zeros(
        (OUTFIT_COUNT * HEIGHT, len(sprites.pictures) * WIDTH, RGBA), np.uint8
    )
    for outfit, palette in enumerate(faces.palettes):
        colors = np.zeros((PALETTE_SIZE, RGBA), np.uint8)
        for entry, color in enumerate(palette):
            red, green, blue, opaque = from_rgba16(color)
            clear = entry in KEPT_INDICES
            alpha = 0 if clear else OPAQUE if opaque else EDGE_ALPHA
            colors[entry] = (red, green, blue, alpha)
        for column, indices in enumerate(faces.pictures):
            top, left = outfit * HEIGHT, column * WIDTH
            sheet[top : top + len(indices), left : left + WIDTH] = colors[indices]
    return sheet


def read_palette(data: bytes) -> list[int]:
    """Return a palette file's RGBA16 colors."""
    entries = data[: PALETTE_SIZE * PALETTE_ENTRY.size]
    return [cast("int", entry[0]) for entry in PALETTE_ENTRY.iter_unpack(entries)]


def asset_files(sprites: SpriteSet, faces: FaceSprites) -> dict[int, bytes]:
    """Return the asset archive's files for FACES, by asset ID."""
    files: dict[int, bytes] = {}
    for outfit, palette in enumerate(faces.palettes):
        files[sprites.first_palette + outfit] = b"".join(
            PALETTE_ENTRY.pack(color) for color in palette
        )
    for picture, indices in zip(sprites.pictures, faces.pictures, strict=True):
        files[picture.asset] = decode_asset.encode_asset(indices.tobytes())
    return files


def asset_name(asset: int) -> str:
    """Return the name that asset_archive.py gives an asset's file."""
    return f"{asset:03X}.bin"


def sheet_for(
    package: bc.Package, template: tmpl.CharacterTemplate, sprites: SpriteSet
) -> ByteArray:
    """Return the package's own sheet if it has one, else its sprites drawn."""
    painted = package.directory / SHEET_FILE
    if painted.is_file():
        return read_sheet(painted, sprites)
    return draw_sheet(package, template, sprites)


def build_face_sprites(
    package: bc.Package, template: tmpl.CharacterTemplate, archive: Path
) -> tuple[SpriteSet, FaceSprites] | None:
    """Return a package's face sprites, or None if its slot's aren't known.

    ARCHIVE is the directory of the original's assets, whose palettes give
    the entries that every palette keeps.
    """
    sprites = load_sprite_sets().get(package.slot)
    if sprites is None:
        return None
    originals = [
        read_palette(
            (archive / asset_name(sprites.first_palette + outfit)).read_bytes()
        )
        for outfit in range(OUTFIT_COUNT)
    ]
    sheet = sheet_for(package, template, sprites)
    return sprites, index_sheet(sheet, sprites, originals)


def write_face_sprites(
    package: bc.Package, template: tmpl.CharacterTemplate, archive: Path, output: Path
) -> list[Path]:
    """Write a package's face sprites to OUTPUT, if its slot's are known.

    Their asset files go in OUTPUT/asset_archive, and a sheet of them, as the
    game shows them, is OUTPUT/SLOT_face_sprites.png. Return the files.
    """
    built = build_face_sprites(package, template, archive)
    if built is None:
        return []
    assets = output / ARCHIVE_DIRECTORY
    assets.mkdir(parents=True, exist_ok=True)
    written: list[Path] = []
    for asset, data in asset_files(*built).items():
        written.append(assets / asset_name(asset))
        _ = written[-1].write_bytes(data)
    written.append(output / f"{package.slot}_{SHEET_FILE}")
    Image.fromarray(faces_sheet(*built)).save(written[-1])
    return written


@final
class Arguments(argparse.Namespace):
    """The command line's arguments."""

    assets: Path = Path()
    output: Path = Path()
    repository: Path = Path()
    packages: list[Path] | None = None


def main() -> None:
    """Draw the face sprites of the packages that the arguments name."""
    parser = argparse.ArgumentParser(description=SUMMARY)
    _ = parser.add_argument(
        "--assets", type=Path, required=True, help="the asset archive's split assets"
    )
    _ = parser.add_argument(
        "--output", type=Path, required=True, help="where the sprites' files go"
    )
    _ = parser.add_argument(
        "--repository", type=Path, default=Path(), help="the decompilation's root"
    )
    _ = parser.add_argument(
        "packages", type=Path, nargs="+", help="character package directories"
    )
    arguments = parser.parse_args(namespace=Arguments())
    # A package no longer built mustn't leave its sprites behind
    for stale in (arguments.output / ARCHIVE_DIRECTORY).glob("*.bin"):
        stale.unlink()
    written: list[Path] = []
    try:
        for directory in arguments.packages or []:
            package = bc.read_package(directory)
            template = tmpl.load_template(arguments.repository, package.slot)
            written += write_face_sprites(
                package, template, arguments.assets, arguments.output
            )
    except (bc.PackageError, SpriteError, tmpl.TemplateError, OSError) as error:
        sys.exit(f"face_sprites.py: {error}")
    listed = "".join(f"{path}\n" for path in written)
    _ = (arguments.output / LIST_FILE).write_text(listed)


if __name__ == "__main__":
    main()
