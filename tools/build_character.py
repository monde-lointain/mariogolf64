#!/usr/bin/env python3
"""Build a custom character's model files from its package.

    build_character.py [--repository BASE] --output DIR PACKAGE...
    build_character.py --check PACKAGE...
    build_character.py --list-files PACKAGE...

Each PACKAGE is a directory that the Blender add-on in
tools/blender/mario_golf_character exports: a character.json and its
textures. It names the character slot it replaces, such as "charlie". The
slot's own models, extracted by make setup into BASE/assets/character_models,
are its template: the custom character keeps their skeletons, animations,
clubs and points, and brings its own parts, colors and face. BASE is the
decompilation's folder, the current one unless given.

DIR gets the files that take the place of BASE's files at the same paths
within it. For each package, these are the slot's eight model files in
assets/character_models, compressed as the game loads them: SLOT.bin,
SLOT_reaction.bin and their palettes, which patch them. Then come
include/characters/character_names.h, with the packages' names, and,
written last, include/characters/model_layouts.h, with their model files'
layouts. --check only reads the packages and reports what's wrong with
them, and --list-files only names the model files that they replace, one
a line.

character.json holds, with positions in the game's model units, Y up and
the character facing +Z, in the template's bind pose:

    format          "mariogolf64-character", version 1
    name, slot      what the character is called, and whose place it takes
    materials       each with a name and an RGBA color; optionally three
                    palettes' colors, double_sided, a texture (a PNG) and
                    a face: the group of the template's face triangles
                    whose animation it takes, such as "eyes", with a PNG
                    for each of the template's textures that group shows,
                    by model kind and texture index. Face images are
                    drawn over the material's color (or its palette's)
    parts           each on a bone, in a material: positions, normals,
                    UVs and triangles
    points          optional: a point's id, bone and position, to move it
    portrait_camera optional: six numbers for each model kind

The character's name replaces the slot's in the game's menus, its text and
its tournaments. DIR also gets a copy of each text file that --text names
within BASE, such as assets/text/messages.txt, with the names changed.
tools/face_sprites.py draws the character's face sprites, which the menus
and scorecards show.
"""

import argparse
import json
import math
import re
import struct
import sys
from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path
from typing import TypeVar, cast, final

import numpy as np
import numpy.typing as npt
from PIL import Image

import character_model_format as fmt
import character_template as tmpl
import f3dex2 as gbi
import lz_codec

SUMMARY = "Build a custom character's model files from its package."

PACKAGE_FORMAT = "mariogolf64-character"
PACKAGE_VERSION = 1
PACKAGE_FILE = "character.json"
MODEL_KINDS = ("main", "reaction")
PALETTE_COUNT = 3
COLOR_CHANNELS = 4
CHANNEL_MAX = 255
PORTRAIT_CAMERA_VALUE_COUNT = fmt.PORTRAIT_CAMERA_VALUE_COUNT

# Each slot's character's name, as the game writes it
SLOT_NAMES = {
    "plum": "Plum",
    "charlie": "Charlie",
    "maple": "Maple",
    "baby_mario": "Baby Mario",
    "luigi": "Luigi",
    "peach": "Peach",
    "sonny": "Sonny",
    "wario": "Wario",
    "harry": "Harry",
    "mario": "Mario",
    "yoshi": "Yoshi",
    "donkey_kong": "DK",
    "bowser": "Bowser",
    "metal_mario": "Metal Mario",
    "kid": "Kid",
    "sherry": "Sherry",
    "azalea": "Azalea",
    "joe": "Joe",
}
# A name must fit the menus' IMPORTED_CHARACTER_NAME_LENGTH, with its end
NAME_LENGTH_MAX = 11
NAME_PUNCTUATION = " .-"
# The menus' font codes a lowercase letter as the letter plus this
LOWERCASE_CODE_OFFSET = 0x80
HEX_DIGITS = "0123456789ABCDEFabcdef"
# The headers that a build's copies replace, and where its model files go
NAMES_HEADER = Path("include/characters/character_names.h")
LAYOUTS_HEADER = Path("include/characters/model_layouts.h")
MODELS_DIRECTORY = Path("assets/character_models")
# A C macro's definition, with the lines that a backslash continues it onto
DEFINE_PATTERN = re.compile(r"^#define (\w+)\b(?:.*\\\n)*.*$", re.MULTILINE)
# How many segment addresses a line of a layout's macro holds
ADDRESSES_PER_LINE = 5
TOURNAMENT_SUFFIX = " Tournament"

# How much of a slot's model buffer a model can use, and how much of it the
# CPU sees through the TLB at segment 2: CHARACTER_BUFFER_SIZE, less what
# aligning the buffer to a page can take, and CHARACTER_MODEL_PAGE_COUNT
# pages. The file and its frame data's two copies, which follow it, must fit.
CHARACTER_BUFFER_SIZE = 0x23680
SMALLEST_PAGE_SIZE = 0x1000
USABLE_BUFFER_SIZE = CHARACTER_BUFFER_SIZE - SMALLEST_PAGE_SIZE
KIBIBYTE = 1024
MAPPED_MODEL_SIZE = 32 * SMALLEST_PAGE_SIZE
# The frame data: room for this many joint matrices, then a small texture
# that animated triangles show until their first is set, then each
# triangle's three vertices, then the list that loads its texture
FRAME_DATA_JOINT_COUNT_MAX = 30
FRAME_DATA_MATRICES_SIZE = FRAME_DATA_JOINT_COUNT_MAX * fmt.MATRIX_SIZE
FRAME_DATA_COPY_SIZE_MIN = 0x780
FRAME_DATA_COPY_COUNT = 3
PLACEHOLDER_TEXTURE_SIZE = 2
PLACEHOLDER_TEXTURE_BYTES = PLACEHOLDER_TEXTURE_SIZE * PLACEHOLDER_TEXTURE_SIZE * 2
# A placeholder texel: opaque black in RGBA5551
PLACEHOLDER_TEXEL = 0x0001
FRAME_DATA_ALIGNMENT = 0x40
# The list that loads an animated triangle's texture: gsSPTexture,
# gsDPSetTextureLUT, the 7 commands of gsDPLoadTextureBlock, gsSPVertex and
# gsSPEndDisplayList
TRIANGLE_LOAD_COMMAND_COUNT = 11
TRIANGLE_LOAD_SIZE = TRIANGLE_LOAD_COMMAND_COUNT * fmt.GFX_SIZE
ANIMATED_TEXTURE_MASK = 6
ANIMATED_TEXTURE_SIZE_MAX = 1 << ANIMATED_TEXTURE_MASK

# Textured parts are drawn with gsSPTexture at half scale, as the game's
# are, so a texel is 64 texture coordinate units
TEXTURE_SCALE_HALF = 0x8000
TEXEL_UNITS = 64
TMEM_SIZE = 0x1000
RGBA16_TEXEL_SIZE = 2
# A texture's line is loaded in 64-bit words
TEXTURE_WIDTH_MULTIPLE = 4
TEXTURE_ALIGNMENT = 8
ALPHA_OPAQUE = 255
NORMAL_SCALE = 127
S16_MIN = -0x8000
S16_MAX = 0x7FFF
VERTEX_FLAG = 0

# The runtime display lists that parts call: the culling segment's lists,
# which turn culling off or on (front or back, for a lefty or not), and the
# render mode segment's, opaque or textured with cut-out edges
CULLING_OFF = 0x05000000
CULLING_ON = 0x05000010
RENDER_MODE_SURFACE = 0x04000000
RENDER_MODE_TEXTURE_EDGE = 0x04000010

END_OF_LIST = 0xFFFFFFFF
END_DISPLAY_LIST = struct.pack(">II", fmt.G_ENDDL << fmt.SEGMENT_SHIFT, 0)
NULL = 0
TEXTURE_ANIMATIONS_END = struct.pack(">hh", -1, 0)
LAYOUT_SUFFIX = "_MODEL_LAYOUT"


class PackageError(ValueError):
    """A character package can't be built, and why."""


class BuilderBugError(RuntimeError):
    """Something that no package should cause went wrong in this tool."""

    def __init__(self, problem: str) -> None:
        """Say that PROBLEM is this tool's fault, and what to do about it."""
        message = (
            f"{problem}. This is a bug in build_character.py, not in the character; "
            "please report it with the package"
        )
        super().__init__(message)


@dataclass
class FaceImages:
    """The images that a face material shows for each of a template's textures."""

    group: str
    images: dict[str, dict[int, Path]]


@dataclass
class Material:
    """How a part's triangles are drawn."""

    name: str
    color: tuple[int, int, int, int]
    palettes: list[tuple[int, int, int, int]]
    double_sided: bool
    texture: Path | None
    face: FaceImages | None


@dataclass
class Part:
    """Triangles in one material, bound to one bone."""

    bone: str
    material: int
    positions: list[tmpl.Vec3]
    normals: list[tmpl.Vec3]
    uvs: list[tuple[float, float]]
    triangles: list[tuple[int, int, int]]


@dataclass
class PointMove:
    """A template point moved onto the custom character."""

    point_id: int
    bone: str
    position: tmpl.Vec3


@dataclass
class Package:
    """A custom character, as the Blender add-on exports it."""

    directory: Path
    name: str
    slot: str
    materials: list[Material]
    parts: list[Part]
    points: list[PointMove] = field(default_factory=list)
    portrait_cameras: dict[str, list[float]] = field(default_factory=dict)
    # Where the character's bones are in its bind pose, when it isn't built
    # to the template's proportions; a bone left out moves with its parent
    skeleton: dict[str, tmpl.Vec3] = field(default_factory=dict)


def _require(condition: bool, where: str, problem: str) -> None:  # noqa: FBT001 - as assert
    """Reject a malformed package, naming the faulty entry: "WHERE: PROBLEM"."""
    if not condition:
        msg = f"{where}: {problem}"
        raise PackageError(msg)


def _check(condition: bool, sentence: str) -> None:  # noqa: FBT001 - as assert
    """Reject a package that the game can't show, with a sentence for its artist."""
    if not condition:
        raise PackageError(sentence)


def _as_object(value: object, where: str, problem: str) -> dict[str, object]:
    """Return a JSON object, which the package must have at WHERE."""
    _require(isinstance(value, dict), where, problem)
    return cast("dict[str, object]", value)


def _as_list(value: object, where: str, problem: str) -> list[object]:
    """Return a JSON list, which the package must have at WHERE."""
    _require(isinstance(value, list), where, problem)
    return cast("list[object]", value)


def _as_str(value: object, where: str, problem: str) -> str:
    """Return a JSON string, which the package must have at WHERE."""
    _require(isinstance(value, str), where, problem)
    return cast("str", value)


def _is_number(value: object) -> bool:
    return isinstance(value, (int, float)) and math.isfinite(value)


def _color(value: object, where: str) -> tuple[int, int, int, int]:
    problem = "a color is four whole numbers from 0 to 255: red, green, blue and alpha"
    channels = _as_list(value, where, problem)
    _require(
        len(channels) == COLOR_CHANNELS
        and all(isinstance(c, int) and 0 <= c <= CHANNEL_MAX for c in channels),
        where,
        problem,
    )
    red, green, blue, alpha = cast("list[int]", channels)
    return (red, green, blue, alpha)


def _vectors(value: object, size: int, where: str) -> list[tuple[float, ...]]:
    problem = f"each entry should be {size} numbers"
    vectors: list[tuple[float, ...]] = []
    for item in _as_list(value, where, "should be a list"):
        numbers = _as_list(item, where, problem)
        _require(
            len(numbers) == size and all(_is_number(c) for c in numbers),
            where,
            problem,
        )
        vectors.append(tuple(float(c) for c in cast("list[float]", numbers)))
    return vectors


def _point(value: object, where: str) -> tmpl.Vec3:
    x, y, z = _vectors([value], 3, where)[0]
    return (x, y, z)


def _image_path(directory: Path, value: object, where: str) -> Path:
    name = _as_str(value, where, "should name a PNG in the package")
    path = (directory / name).resolve()
    _require(
        path.is_relative_to(directory.resolve()), where, "should be inside the package"
    )
    _require(path.is_file(), where, f"{name} isn't in the package")
    return path


def _read_face(directory: Path, value: object, where: str) -> FaceImages:
    face = _as_object(value, where, "should say its group and images")
    group = _as_str(face.get("group"), where, 'needs a "group", such as "eyes"')
    images: dict[str, dict[int, Path]] = {}
    for kind in MODEL_KINDS:
        kind_where = f"{where}.{kind}"
        kind_images = _as_object(
            face.get(kind, {}), kind_where, "should map indices to PNGs"
        )
        images[kind] = {}
        for index, path in kind_images.items():
            _require(index.isdigit(), kind_where, f"{index} isn't an index")
            images[kind][int(index)] = _image_path(
                directory, path, f"{kind_where}.{index}"
            )
    return FaceImages(group, images)


def _read_material(directory: Path, value: object, where: str) -> Material:
    material = _as_object(value, where, "should be an object")
    name = str(material.get("name", where))
    color = _color(material.get("color"), f"{where} ({name}).color")
    palettes_value = _as_list(
        material.get("palettes") or [list(color)] * PALETTE_COUNT,
        f"{where} ({name}).palettes",
        "should be a list of colors",
    )
    _require(
        len(palettes_value) == PALETTE_COUNT,
        f"{where} ({name}).palettes",
        f"should hold {PALETTE_COUNT} colors, one for each alternate palette",
    )
    palettes = [
        _color(palette, f"{where} ({name}).palettes[{index}]")
        for index, palette in enumerate(palettes_value)
    ]
    texture_value = material.get("texture")
    texture = (
        _image_path(directory, texture_value, f"{where} ({name}).texture")
        if texture_value
        else None
    )
    face_value = material.get("face")
    face = (
        _read_face(directory, face_value, f"{where} ({name}).face")
        if face_value
        else None
    )
    return Material(
        name, color, palettes, bool(material.get("double_sided")), texture, face
    )


def _read_part(value: object, material_count: int, where: str) -> Part:
    part = _as_object(value, where, "should be an object")
    bone = _as_str(part.get("bone"), where, "needs the name of the bone it moves with")
    material = part.get("material")
    _require(
        isinstance(material, int) and 0 <= material < material_count,
        where,
        f"its material should be an index below {material_count}",
    )
    positions = _vectors(part.get("positions"), 3, f"{where}.positions")
    normals = _vectors(part.get("normals"), 3, f"{where}.normals")
    uvs = _vectors(part.get("uvs"), 2, f"{where}.uvs")
    _require(
        len(normals) == len(positions) == len(uvs),
        where,
        "needs as many normals and UVs as positions",
    )
    triangles: list[tuple[int, int, int]] = []
    for triangle in _vectors(part.get("triangles"), 3, f"{where}.triangles"):
        _require(
            all(i.is_integer() and 0 <= i < len(positions) for i in triangle),
            f"{where}.triangles",
            f"each corner should index one of its {len(positions)} positions",
        )
        first, second, third = (int(i) for i in triangle)
        triangles.append((first, second, third))
    return Part(
        bone,
        cast("int", material),
        [(p[0], p[1], p[2]) for p in positions],
        [(n[0], n[1], n[2]) for n in normals],
        [(uv[0], uv[1]) for uv in uvs],
        triangles,
    )


def _read_point(value: object, where: str) -> PointMove:
    point = _as_object(value, where, "should be an object")
    point_id = point.get("id")
    _require(isinstance(point_id, int), where, "needs the id of the template's point")
    bone = _as_str(point.get("bone"), where, "needs the name of the bone it moves with")
    return PointMove(
        cast("int", point_id), bone, _point(point.get("position"), f"{where}.position")
    )


def _read_cameras(value: object, where: str) -> dict[str, list[float]]:
    problem = f"maps main or reaction to {PORTRAIT_CAMERA_VALUE_COUNT} numbers"
    cameras: dict[str, list[float]] = {}
    for kind, values in _as_object(value, where, problem).items():
        _require(kind in MODEL_KINDS, where, problem)
        cameras[kind] = list(_vectors([values], PORTRAIT_CAMERA_VALUE_COUNT, where)[0])
    return cameras


def read_package(directory: Path) -> Package:
    """Read and check the package in DIRECTORY."""
    path = directory / PACKAGE_FILE
    _require(
        path.is_file(), str(directory), f"has no {PACKAGE_FILE}; export it from Blender"
    )
    try:
        parsed = cast("object", json.loads(path.read_text()))
    except json.JSONDecodeError as error:
        msg = f"{path}: isn't JSON: {error}"
        raise PackageError(msg) from None
    where = str(path)
    document = _as_object(parsed, where, "should hold an object")
    _require(
        document.get("format") == PACKAGE_FORMAT,
        where,
        f'isn\'t a character package; its "format" should be "{PACKAGE_FORMAT}"',
    )
    _require(
        document.get("version") == PACKAGE_VERSION,
        where,
        (
            f"is version {document.get('version')}, but this build reads version "
            f"{PACKAGE_VERSION}"
        ),
    )
    slot = _as_str(document.get("slot"), where, 'needs a "slot", such as "charlie"')
    _require(slot.isidentifier(), where, 'needs a "slot", such as "charlie"')
    materials_value = _as_list(document.get("materials"), where, "has no materials")
    _require(bool(materials_value), where, "has no materials")
    materials = [
        _read_material(directory, material, f"{where}: materials[{index}]")
        for index, material in enumerate(materials_value)
    ]
    parts_value = _as_list(document.get("parts"), where, "has no parts")
    _require(bool(parts_value), where, "has no parts")
    parts = [
        _read_part(part, len(materials), f"{where}: parts[{index}]")
        for index, part in enumerate(parts_value)
    ]
    points = [
        _read_point(point, f"{where}: points[{index}]")
        for index, point in enumerate(
            _as_list(document.get("points", []), where, "points should be a list")
        )
    ]
    cameras = _read_cameras(
        document.get("portrait_camera", {}), f"{where}: portrait_camera"
    )
    skeleton = {
        name: _point(head, f"{where}: skeleton.{name}")
        for name, head in _as_object(
            document.get("skeleton", {}),
            f"{where}: skeleton",
            "maps bone names to heads",
        ).items()
    }
    name = _as_str(
        document.get("name", SLOT_NAMES.get(slot, slot)),
        where,
        '"name" should be the character\'s name, such as "Terry"',
    )
    _check_name(name, where)
    return Package(
        directory,
        name,
        slot,
        materials,
        parts,
        points,
        cameras,
        skeleton,
    )


def _check_name(name: str, where: str) -> None:
    """Reject a name that the game's menus can't show."""
    _require(
        0 < len(name) <= NAME_LENGTH_MAX,
        where,
        (
            f'the name "{name}" should be 1 to {NAME_LENGTH_MAX} letters long, '
            "to fit the menus"
        ),
    )
    _require(
        all((c.isascii() and c.isalnum()) or c in NAME_PUNCTUATION for c in name),
        where,
        (
            f'the name "{name}" can hold only the letters A to Z, digits, spaces, '
            "periods and hyphens, which the menus' font has"
        ),
    )


def menu_spelling(name: str) -> str:
    """Return NAME as a C string in the menus' font, as character_names.h has it."""
    spelled = '"'
    escaped = False
    for c in name:
        if c.islower():
            spelled += f"\\x{ord(c) + LOWERCASE_CODE_OFFSET:02X}"
            escaped = True
            continue
        if escaped and c in HEX_DIGITS:
            spelled += '" "'  # Else the escape would take the digit as its own
        spelled += c
        escaped = False
    return spelled + '"'


def name_defines(packages: list[Package]) -> dict[str, str]:
    """Return the definitions that give character_names.h the packages' names."""
    defines: dict[str, str] = {}
    for package in packages:
        macro = package.slot.upper()
        spellings = {
            "NAME": f'"{package.name}"',
            "MENU_NAME": menu_spelling(package.name),
            "CONTESTANT_NAME": menu_spelling(package.name.replace(" ", "_")),
        }
        for suffix, value in spellings.items():
            defines[f"{macro}_{suffix}"] = f"#define {macro}_{suffix} {value}"
    return defines


def replace_defines(header: str, defines: dict[str, str], where: Path) -> str:
    """Return HEADER with each macro that DEFINES names defined as it gives.

    A macro's definition takes the lines its backslashes continue it onto.
    WHERE names the header, for the error when it lacks a macro.
    """
    missing = set(defines) - set(cast("list[str]", DEFINE_PATTERN.findall(header)))
    if missing:
        msg = f"{where} doesn't define {', '.join(sorted(missing))}"
        raise BuilderBugError(msg)
    return DEFINE_PATTERN.sub(
        lambda match: defines.get(match.group(1), match.group(0)), header
    )


def rename_text(text: str, packages: list[Package]) -> str:
    """Return a text file's TEXT with each slot's name and tournament the packages'."""
    renames: dict[str, str] = {}
    for package in packages:
        old = SLOT_NAMES.get(package.slot, package.slot)
        renames[old] = package.name
        renames[old + TOURNAMENT_SUFFIX] = package.name + TOURNAMENT_SUFFIX
    lines: list[str] = []
    for line in text.splitlines(keepends=True):
        content = line.rstrip("\r\n")
        lines.append(renames.get(content, content) + line[len(content) :])
    return "".join(lines)


@dataclass
class Texture:
    """An RGBA16 texture, as the RDP loads it."""

    width: int
    height: int
    texels: bytes
    has_cutout: bool


# An image's RGBA pixels, by row and column
Pixels = npt.NDArray[np.uint16]
ScalarT = TypeVar("ScalarT", bound=np.generic)
# A texture as drawn: its image and, for a face's, the color under the image
TextureKey = tuple[Path, tuple[int, int, int] | None]


def read_texture(
    path: Path, *, animated: bool, backdrop: tuple[int, int, int] | None = None
) -> Texture:
    """Convert a PNG to RGBA16, checking that the game can load it.

    With a BACKDROP color, the image is drawn over it, opaque, as a face's
    images are drawn over the face's color, as Blender shows a material that
    mixes its image over a color by the image's alpha.
    """
    # Not a with statement: Pillow's __enter__ returns typing.Self, which
    # the type checker's Python 3.10 lacks
    opened = Image.open(path)
    try:
        image = opened.convert("RGBA")
    finally:
        opened.close()
    width, height = image.size
    where = f"The image {path.stem}"
    texel_limit = TMEM_SIZE // RGBA16_TEXEL_SIZE
    _check(
        width * height <= texel_limit,
        (
            f"{where} is {width}x{height}, {width * height} pixels, but the game's "
            f"texture memory holds {texel_limit} (such as 64x32 or 32x32). Scale it "
            "down"
        ),
    )
    _check(
        width % TEXTURE_WIDTH_MULTIPLE == 0,
        (
            f"{where} is {width} pixels wide, but the game loads textures in rows of "
            f"{TEXTURE_WIDTH_MULTIPLE} pixels. Make its width a multiple of "
            f"{TEXTURE_WIDTH_MULTIPLE}, such as 32"
        ),
    )
    if animated:
        _check(
            width <= ANIMATED_TEXTURE_SIZE_MAX and height <= ANIMATED_TEXTURE_SIZE_MAX,
            (
                f"{where} is {width}x{height}, but a face image can be at most "
                f"{ANIMATED_TEXTURE_SIZE_MAX}x{ANIMATED_TEXTURE_SIZE_MAX}. Scale it "
                "down"
            ),
        )
    pixels: Pixels = np.asarray(image, dtype=np.uint16)
    if backdrop is not None:
        pixels = draw_over(pixels, backdrop)
    is_opaque = pixels[:, :, 3] >= CHANNEL_MAX // 2
    has_cutout = not bool(is_opaque.all())
    if has_cutout:
        pixels = bleed_edges(pixels, is_opaque)
    red, green, blue = (pixels[:, :, channel] for channel in range(3))
    texels = (red >> 3) << 11 | (green >> 3) << 6 | (blue >> 3) << 1 | is_opaque
    return Texture(width, height, texels.astype(">u2").tobytes(), has_cutout)


def _s16_point(position: tmpl.Vec3, where: str) -> tuple[int, int, int]:
    """Return a point in the 16-bit whole numbers that the game's vertices hold."""
    x, y, z = (_s16(value, where) for value in position)
    return (x, y, z)


def draw_over(pixels: Pixels, backdrop: tuple[int, int, int]) -> Pixels:
    """Draw RGBA pixels over a color, keeping their partial alpha's soft edges."""
    alpha = pixels[:, :, 3:4].astype(np.float64) / CHANNEL_MAX
    colors = pixels[:, :, :3] * alpha + np.array(backdrop, dtype=np.float64) * (
        1 - alpha
    )
    drawn = np.full_like(pixels, CHANNEL_MAX)
    drawn[:, :, :3] = np.round(colors).astype(np.uint16)
    return drawn


def _wrap_shift(
    array: npt.NDArray[ScalarT], rows: int, columns: int
) -> npt.NDArray[ScalarT]:
    """Return ARRAY shifted down ROWS and right COLUMNS, wrapping around."""
    # One of np.roll's overloads names a type that Python 3.10 lacks
    return np.roll(array, (rows, columns), axis=(0, 1))  # pyright: ignore[reportUnknownMemberType]


def bleed_edges(pixels: Pixels, is_opaque: npt.NDArray[np.bool]) -> Pixels:
    """Give a cut-out texture's clear texels the colors of the opaque ones beside them.

    The game filters textures, blending each texel with its neighbors, so
    the colors under clear texels show at a cut-out's edges; painters
    usually leave them black, which draws a dark fringe. Each pass colors
    the clear texels next to colored ones with their neighbors' average.
    """
    colors = pixels[:, :, :3].astype(np.float64)
    # np.array copies: ndarray.copy returns typing.Self, which the type
    # checker's Python 3.10 lacks
    known = np.array(is_opaque)
    neighbors = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    while not known.all():
        total = np.zeros_like(colors)
        weight = np.zeros(known.shape)
        for dy, dx in neighbors:
            # Wrapping matches how the texture repeats
            shifted_known = _wrap_shift(known, dy, dx)
            total += _wrap_shift(colors, dy, dx) * shifted_known[:, :, None]
            weight += shifted_known
        growing = ~known & (weight > 0)
        if not growing.any():
            break  # No opaque texel at all
        colors[growing] = total[growing] / weight[growing][:, None]
        known |= growing
    bled = np.array(pixels)
    bled[:, :, :3] = np.round(colors).astype(np.uint16)
    return bled


def _mask(size: int) -> int:
    """Return a coordinate mask that wraps at SIZE; 0 unless it's a power of 2."""
    if size & (size - 1):
        return 0
    return size.bit_length() - 1


def _s16(value: float, where: str) -> int:
    rounded = round(value)
    _check(
        S16_MIN <= rounded <= S16_MAX,
        (
            f"{where} has a vertex {abs(value):.0f} units from its bone, but the game "
            f"stores positions from {S16_MIN} to {S16_MAX}. Is the character scaled to "
            "the template rig?"
        ),
    )
    return rounded


def _s8_normal(value: float) -> int:
    return max(-NORMAL_SCALE, min(NORMAL_SCALE, round(value * NORMAL_SCALE))) & 0xFF


def _vtx(
    position: tuple[int, int, int], st: tuple[int, int], normal: tmpl.Vec3
) -> bytes:
    return struct.pack(
        ">hhhHhhBBBB",
        *position,
        VERTEX_FLAG,
        *st,
        _s8_normal(normal[0]),
        _s8_normal(normal[1]),
        _s8_normal(normal[2]),
        ALPHA_OPAQUE,
    )


def _texture_st(uv: tuple[float, float], width: int, height: int) -> tuple[int, int]:
    """Return a UV's texture coordinates; the image's top row is T 0, where V is 1."""
    s = _s16(uv[0] * width * TEXEL_UNITS, "a UV")
    t = _s16((1.0 - uv[1]) * height * TEXEL_UNITS, "a UV")
    return (s, t)


class FileBuilder:
    """Lays a model file out, a piece at a time, at segment 2 addresses."""

    def __init__(self) -> None:
        """Start an empty file."""
        self.data: bytearray = bytearray()
        self.shared: dict[tuple[bytes, int], int] = {}

    def align(self, alignment: int) -> None:
        """Pad the file to a multiple of ALIGNMENT."""
        self.data += bytes(-len(self.data) % alignment)

    def add(self, content: bytes, alignment: int = 8) -> int:
        """Append CONTENT and return its segment address."""
        self.align(alignment)
        address = fmt.model_address(len(self.data))
        self.data += content
        return address

    def add_shared(self, content: bytes, alignment: int = 8) -> int:
        """Append CONTENT unless it was added this way already; return its address."""
        key = (content, alignment)
        if key not in self.shared:
            self.shared[key] = self.add(content, alignment)
        return self.shared[key]

    def next_address(self, alignment: int = 8) -> int:
        """Return the address that the next piece with ALIGNMENT gets."""
        return fmt.model_address(len(self.data) + (-len(self.data) % alignment))


@dataclass
class _AnimatedTriangleSource:
    """A custom face triangle, and the template triangle whose animation it takes."""

    corners: list[tuple[tuple[int, int, int], tmpl.Vec3, tuple[float, float]]]
    material: Material
    template_index: int


@dataclass
class _JointDrawing:
    """What a joint's part list draws, gathered by material."""

    by_material: dict[int, list[tuple[Part, tuple[int, int, int]]]] = field(
        default_factory=dict
    )
    face_triangles: list[_AnimatedTriangleSource] = field(default_factory=list)


def _bone_displacements(
    package: Package, template: tmpl.CharacterTemplate
) -> dict[int, tmpl.Vec3]:
    """How far each bone of the custom skeleton is from the template's."""
    given = {template.bone_index(name): head for name, head in package.skeleton.items()}
    displacements: dict[int, tmpl.Vec3] = {}
    for bone in template.bones:  # Parents come first
        if bone.joint in given:
            head = given[bone.joint]
            displacements[bone.joint] = (
                head[0] - bone.head[0],
                head[1] - bone.head[1],
                head[2] - bone.head[2],
            )
        else:
            displacements[bone.joint] = displacements.get(bone.parent, (0.0, 0.0, 0.0))
    return displacements


class ModelBuilder:
    """Builds one of a custom character's models over its template."""

    def __init__(
        self,
        package: Package,
        template: tmpl.CharacterTemplate,
        kind: str,
        palette: int,
    ) -> None:
        """Prepare to build PACKAGE's KIND model in PALETTE's colors, 0 for the base."""
        self.package: Package = package
        self.template: tmpl.CharacterTemplate = template
        self.kind: str = kind
        self.model_template: tmpl.TemplateModel = template.models()[kind]
        self.model: fmt.ModelFile = self.model_template.model
        self.palette: int = palette
        self.file: FileBuilder = FileBuilder()
        self.texture_cache: dict[TextureKey, Texture] = {}
        self.displacements: dict[int, tmpl.Vec3] = _bone_displacements(
            package, template
        )
        # Where each bone's joint is in the custom bind pose, moved with the
        # bone that the joint follows
        self.origins: dict[int, tmpl.Vec3] = {}
        for bone, origin in self.model_template.origins.items():
            joint = self.model_template.joints[bone]
            moved = self.displacements[self.model_template.joint_bones[joint]]
            self.origins[bone] = (
                origin[0] + moved[0],
                origin[1] + moved[1],
                origin[2] + moved[2],
            )

    def _local(self, bone: int, position: tmpl.Vec3) -> tmpl.Vec3:
        """Where a bind-pose point on BONE's part is in its joint's space."""
        origin = self.origins[bone]
        return (
            position[0] - origin[0],
            position[1] - origin[1],
            position[2] - origin[2],
        )

    def _texture(self, key: TextureKey, *, animated: bool) -> Texture:
        if key not in self.texture_cache:
            path, backdrop = key
            self.texture_cache[key] = read_texture(
                path, animated=animated, backdrop=backdrop
            )
        return self.texture_cache[key]

    def _texture_key(self, material: Material, path: Path) -> TextureKey:
        """Return how a material draws an image: a face's over its color."""
        if material.face is None:
            return (path, None)
        red, green, blue, _ = self._material_color(material)
        return (path, (red, green, blue))

    def _material_color(self, material: Material) -> tuple[int, int, int, int]:
        if self.palette == 0:
            return material.color
        return material.palettes[self.palette - 1]

    def _gather(self) -> dict[int, _JointDrawing]:
        """Sort the parts' triangles by the joint that draws them."""
        drawings: dict[int, _JointDrawing] = {}
        for part in self.package.parts:
            bone = self.template.bone_index(part.bone)
            joint = self.model_template.joints[bone]
            drawing = drawings.setdefault(joint, _JointDrawing())
            material = self.package.materials[part.material]
            for corner_indices in part.triangles:
                where = f"The part on the bone {part.bone}"
                local = [
                    _s16_point(self._local(bone, part.positions[index]), where)
                    for index in corner_indices
                ]
                if material.face is None or not self._animates(material):
                    drawing.by_material.setdefault(part.material, []).append(
                        (part, corner_indices)
                    )
                    continue
                corners = [
                    (local[n], part.normals[index], part.uvs[index])
                    for n, index in enumerate(corner_indices)
                ]
                x, y, z = (
                    sum(part.positions[index][axis] for index in corner_indices) / 3
                    for axis in range(3)
                )
                drawing.face_triangles.append(
                    _AnimatedTriangleSource(
                        corners,
                        material,
                        self._nearest_template_triangle(material.face.group, (x, y, z)),
                    )
                )
        return drawings

    def _animates(self, material: Material) -> bool:
        """Whether this model kind animates a face material's part.

        Where the template doesn't, such as the mouth in a main model, the
        material shows its still image instead.
        """
        if material.face is None:
            return False
        if material.face.group in self.model_template.face_groups():
            return True
        _check(
            material.texture is not None,
            (
                f"The face material {material.name} shows a {material.face.group}, "
                f"which {self.template.slot}'s {self.kind} model doesn't animate, so "
                "there it shows its still image; give it one (in Blender, an Image "
                "Texture feeding its Base Color)"
            ),
        )
        return False

    def _nearest_template_triangle(self, group: str, centroid: tmpl.Vec3) -> int:
        """Return the template's face triangle in GROUP nearest a custom one's side."""
        candidates = [t for t in self.model_template.face_triangles if t.group == group]
        if not candidates:
            msg = (
                f"the {self.kind} model has no {group} triangles, but _animates said so"
            )
            raise BuilderBugError(msg)

        def distance(triangle: tmpl.FaceTriangle) -> float:
            # A triangle on the other side of the face is never nearer, so that
            # a wink stays on its eye
            other_side = (triangle.centroid[0] < 0) != (centroid[0] < 0)
            return math.dist(triangle.centroid, centroid) + (1e9 if other_side else 0.0)

        return min(candidates, key=distance).index

    def _copy_template_lists(self) -> dict[int, int]:
        """Copy the clubs' lists and what they reference, and map their addresses."""
        roots = [
            *self.model.display_list_choices,
            self.model.entry.segment_addresses[fmt.SEGMENT_CHOSEN_DISPLAY_LIST],
        ]
        fixed_matrices: list[int] = []
        for node, _ in fmt.walk_skeleton(self.model.skeleton):
            if node.joint < 0:
                roots += node.display_lists
                fixed_matrices.append(node.matrix_address)
        blobs = fmt.referenced_blobs(self.model.data, roots)
        matrix_blobs = [
            fmt.Blob(start, start + fmt.MATRIX_SIZE, "matrix")
            for start in map(fmt.segment_offset, fixed_matrices)
        ]
        runs = fmt.merge_blobs([*blobs, *matrix_blobs, self._chosen_slot()])
        moved: dict[int, int] = {}
        for run in runs:
            address = self.file.add(self.model.data[run.start : run.end])
            moved[run.start] = fmt.segment_offset(address) - run.start

        def relocate(address: int) -> int:
            offset = fmt.segment_offset(address)
            for run in runs:
                if run.start <= offset < run.end:
                    return address + moved[run.start]
            msg = f"0x{address:08X} isn't in what was copied from the template"
            raise BuilderBugError(msg)

        for blob in blobs:
            if blob.kind == "display list":
                self._relocate_list(
                    blob, relocate(fmt.model_address(blob.start)), relocate
                )
        return {address: relocate(address) for address in {*roots, *fixed_matrices}}

    def _chosen_slot(self) -> fmt.Blob:
        """Return the slot that the game writes the chosen club's call into.

        The slot reads as a list that ends at once, but the game makes it a
        call, after which the list must still end.
        """
        chosen = fmt.segment_offset(
            self.model.entry.segment_addresses[fmt.SEGMENT_CHOSEN_DISPLAY_LIST]
        )
        slot = fmt.Blob(chosen, chosen + fmt.CHOSEN_DISPLAY_LIST_SIZE, "chosen list")
        if self.model.data[chosen + fmt.GFX_SIZE : slot.end] != END_DISPLAY_LIST:
            name = self.model.entry.rom_name
            msg = f"{name}'s chosen display list has no end after its slot"
            raise BuilderBugError(msg)
        return slot

    def _relocate_list(
        self, blob: fmt.Blob, new_address: int, relocate: Callable[[int], int]
    ) -> None:
        """Point a copied display list's model addresses at the copies."""
        new_start = fmt.segment_offset(new_address)
        for offset, word0, word1 in fmt.commands(
            self.model.data, fmt.model_address(blob.start)
        ):
            opcode = word0 >> fmt.SEGMENT_SHIFT
            if opcode in {fmt.G_DL, fmt.G_VTX, fmt.G_SETTIMG} and (
                fmt.segment_of(word1) == fmt.MODEL_SEGMENT
            ):
                position = new_start + (offset - blob.start) + 4
                struct.pack_into(">I", self.file.data, position, relocate(word1))

    def _add_textures(
        self, drawings: dict[int, _JointDrawing]
    ) -> dict[TextureKey, int]:
        """Add every texture the parts and the face show, and return their addresses."""
        keys: dict[TextureKey, bool] = {}
        for material in self.package.materials:
            if material.texture is not None:
                keys[self._texture_key(material, material.texture)] = False
        for drawing in drawings.values():
            for source in drawing.face_triangles:
                if source.material.face is not None:
                    for path in source.material.face.images[self.kind].values():
                        keys[self._texture_key(source.material, path)] = True
        return {
            key: self.file.add(
                self._texture(key, animated=animated).texels, TEXTURE_ALIGNMENT
            )
            for key, animated in keys.items()
        }

    def _material_commands(
        self,
        material: Material,
        textures: dict[TextureKey, int],
        state: dict[str, object],
    ) -> list[gbi.Command]:
        """Set up the RDP for a material, changing only what the last one left."""
        commands: list[gbi.Command] = [gbi.pipe_sync()]
        culling = CULLING_OFF if material.double_sided else CULLING_ON
        if state.get("culling") != culling:
            commands.append(gbi.call(culling))
            state["culling"] = culling
        key = (
            self._texture_key(material, material.texture) if material.texture else None
        )
        texture = self._texture(key, animated=False) if key else None
        render_mode = (
            RENDER_MODE_TEXTURE_EDGE
            if texture is not None and texture.has_cutout
            else RENDER_MODE_SURFACE
        )
        if state.get("render_mode") != render_mode:
            commands.append(gbi.call(render_mode))
            state["render_mode"] = render_mode
        if texture is None or key is None:
            if state.get("combine") != gbi.COMBINE_PRIMITIVE_SHADE:
                commands.append(gbi.COMBINE_PRIMITIVE_SHADE)
                state["combine"] = gbi.COMBINE_PRIMITIVE_SHADE
            commands.append(gbi.prim_color(*self._material_color(material)))
            return commands
        mask_s, mask_t = _mask(texture.width), _mask(texture.height)
        commands += [
            gbi.texture(
                TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, gbi.G_TX_RENDERTILE, on=True
            ),
            gbi.texture_lut(gbi.G_TT_NONE),
            *gbi.load_rgba16_block(
                textures[key],
                texture.width,
                texture.height,
                gbi.TileWrap(
                    gbi.G_TX_WRAP if mask_s else gbi.G_TX_CLAMP,
                    gbi.G_TX_WRAP if mask_t else gbi.G_TX_CLAMP,
                    mask_s,
                    mask_t,
                ),
            ),
            gbi.COMBINE_TEXEL_SHADE,
            gbi.prim_color(*self._material_color(material)),
        ]
        state["combine"] = gbi.COMBINE_TEXEL_SHADE
        return commands

    def _vertex_batches(
        self,
        triangles: list[tuple[Part, tuple[int, int, int]]],
        texture: Texture | None,
        bone_of: Callable[[Part], int],
    ) -> list[tuple[list[bytes], list[tuple[int, int, int]]]]:
        """Split triangles into loads of the vertex buffer, sharing equal vertices."""
        batches: list[tuple[list[bytes], list[tuple[int, int, int]]]] = []
        vertices: list[bytes] = []
        indices: dict[bytes, int] = {}
        batch_triangles: list[tuple[int, int, int]] = []
        for part, corners in triangles:
            bone = bone_of(part)
            encoded: list[bytes] = []
            for index in corners:
                local = self._local(bone, part.positions[index])
                position = (
                    _s16(local[0], part.bone),
                    _s16(local[1], part.bone),
                    _s16(local[2], part.bone),
                )
                st = (
                    _texture_st(part.uvs[index], texture.width, texture.height)
                    if texture is not None
                    else (0, 0)
                )
                encoded.append(_vtx(position, st, part.normals[index]))
            new_count = len({vertex for vertex in encoded if vertex not in indices})
            if len(vertices) + new_count > gbi.VERTEX_BUFFER_SIZE:
                batches.append((vertices, batch_triangles))
                vertices, indices, batch_triangles = [], {}, []
            for vertex in encoded:
                if vertex not in indices:
                    indices[vertex] = len(vertices)
                    vertices.append(vertex)
            batch_triangles.append(
                (indices[encoded[0]], indices[encoded[1]], indices[encoded[2]])
            )
        if batch_triangles:
            batches.append((vertices, batch_triangles))
        return batches

    def _triangle_commands(
        self, triangles: list[tuple[int, int, int]]
    ) -> list[gbi.Command]:
        commands = [
            gbi.triangles(triangles[index], triangles[index + 1])
            for index in range(0, len(triangles) - 1, 2)
        ]
        if len(triangles) % 2:
            commands.append(gbi.triangle(*triangles[-1]))
        return commands

    def _add_part_list(
        self,
        drawing: _JointDrawing,
        textures: dict[TextureKey, int],
        triangle_loads: dict[int, int],
    ) -> int:
        """Add a joint's vertices and part list, returning the list's address."""
        state: dict[str, object] = {
            "culling": CULLING_ON,
            "render_mode": RENDER_MODE_SURFACE,
        }
        commands: list[gbi.Command] = [
            gbi.pipe_sync(),
            gbi.cycle_type(gbi.G_CYC_1CYCLE),
            gbi.geometry_mode(
                gbi.G_SHADE
                | gbi.G_CULL_BOTH
                | gbi.G_FOG
                | gbi.G_LIGHTING
                | gbi.G_SHADING_SMOOTH,
                0,
            ),
            gbi.geometry_mode(0, gbi.G_ZBUFFER),
            gbi.call(CULLING_ON),
            gbi.call(RENDER_MODE_SURFACE),
            gbi.geometry_mode(0, gbi.G_SHADE | gbi.G_LIGHTING | gbi.G_SHADING_SMOOTH),
        ]

        def bone_of(part: Part) -> int:
            return self.template.bone_index(part.bone)

        for material_index, triangles in sorted(drawing.by_material.items()):
            material = self.package.materials[material_index]
            commands += self._material_commands(material, textures, state)
            texture = (
                self._texture(
                    self._texture_key(material, material.texture), animated=False
                )
                if material.texture
                else None
            )
            for vertices, batch_triangles in self._vertex_batches(
                triangles, texture, bone_of
            ):
                address = self.file.add(b"".join(vertices))
                commands.append(gbi.vertex(address, len(vertices), 0))
                commands += self._triangle_commands(batch_triangles)
        material = None
        for source in drawing.face_triangles:
            if source.material is not material:
                material = source.material
                commands += [
                    gbi.pipe_sync(),
                    gbi.call(CULLING_OFF if material.double_sided else CULLING_ON),
                    gbi.call(RENDER_MODE_SURFACE),
                    gbi.COMBINE_TEXEL_SHADE,
                ]
            commands += [
                gbi.pipe_sync(),
                gbi.call(triangle_loads[id(source)]),
                gbi.triangle(0, 1, 2),
            ]
        commands.append(gbi.end())
        return self.file.add(gbi.to_bytes(commands))

    def _add_main_list(self, part_lists: dict[int, int], copied: dict[int, int]) -> int:
        """Add the list that pushes each joint's matrix and draws its part."""
        commands: list[gbi.Command] = []

        def draw(node: fmt.SkeletonNode) -> None:
            if node.joint >= 0:
                address = fmt.frame_data_address(node.joint * fmt.MATRIX_SIZE)
            else:
                address = copied[node.matrix_address]
            commands.append(gbi.matrix(address, gbi.G_MTX_PUSH))
            if node.joint >= 0:
                if node.joint in part_lists:
                    commands.append(gbi.call(part_lists[node.joint]))
            else:
                commands.extend(gbi.call(copied[dl]) for dl in node.display_lists)
            for child in node.children:
                draw(child)
            commands.append(gbi.pop_matrix())

        for root in self.model.skeleton:
            draw(root)
        commands.append(gbi.end())
        return self.file.add(gbi.to_bytes(commands))

    def _copy_joints(self) -> int:
        """Copy the joints' keyframes, and return the joint table's address."""
        data = self.model.data
        table = fmt.segment_offset(
            self.model.entry.segment_addresses[fmt.SEGMENT_JOINTS]
        )

        def copy(address: int, size: int) -> int:
            start = fmt.segment_offset(address)
            return self.file.add_shared(data[start : start + size], 4)

        structs = bytearray()
        for index, joint in enumerate(self.model.joints):
            start = table + index * fmt.JOINT_ANIMATION_SIZE
            counts = data[start : start + 8]
            pointers = struct.unpack_from(">6I", data, start + 8)
            sizes = (
                len(joint.translation_frames) * 2,
                len(joint.rotation_frames) * 2,
                len(joint.scale_frames) * 2,
                len(joint.translations) * fmt.VEC3_SIZE,
                len(joint.rotations) * fmt.QUATERNION_SIZE,
                len(joint.scales) * fmt.VEC3_SIZE,
            )
            moved = [
                copy(pointer, size)
                for pointer, size in zip(pointers, sizes, strict=True)
            ]
            shift = self._joint_shift(index)
            if any(shift):
                # The joint sits where the custom skeleton puts it
                translations = [
                    value + shift[axis % 3]
                    for axis, value in enumerate(
                        v for key in joint.translations for v in key
                    )
                ]
                moved[3] = self.file.add_shared(
                    struct.pack(f">{len(translations)}f", *translations), 4
                )
            spans = data[start + 32 : start + fmt.JOINT_ANIMATION_SIZE]
            structs += counts + struct.pack(">6I", *moved) + spans
        end = table + len(self.model.joints) * fmt.JOINT_ANIMATION_SIZE
        structs += data[end : end + fmt.JOINT_ANIMATION_SIZE]
        return self.file.add(bytes(structs), 4)

    def _joint_shift(self, joint: int) -> tmpl.Vec3:
        """How far the custom skeleton moves a joint from its parent's place."""
        bones = self.model_template.joint_bones
        moved = self.displacements[bones[joint]]
        parent = self.model.joint_parents()[joint]
        if parent < 0:
            return moved
        parent_moved = self.displacements[bones[parent]]
        return (
            moved[0] - parent_moved[0],
            moved[1] - parent_moved[1],
            moved[2] - parent_moved[2],
        )

    def _copy_animations(self) -> int:
        data = self.model.data
        table = fmt.segment_offset(
            self.model.entry.segment_addresses[fmt.SEGMENT_ANIMATIONS]
        )
        structs = bytearray()
        for index, animation in enumerate(self.model.animations):
            start = table + index * fmt.CHARACTER_ANIMATION_SIZE
            events_start = fmt.segment_offset(struct.unpack_from(">I", data, start)[0])
            events_size = (len(animation.events) + 1) * fmt.ANIMATION_EVENT_SIZE
            events = self.file.add(data[events_start : events_start + events_size], 4)
            structs += (
                struct.pack(">I", events)
                + data[start + 4 : start + fmt.CHARACTER_ANIMATION_SIZE]
            )
        structs += bytes(fmt.CHARACTER_ANIMATION_SIZE)
        return self.file.add(bytes(structs), 4)

    def _joint_path(self, joint: int) -> list[int]:
        parents = self.model.joint_parents()
        path = [joint]
        while parents[path[-1]] >= 0:
            path.append(parents[path[-1]])
        return path[::-1]

    def _add_points(self) -> int:
        moves = {move.point_id: move for move in self.package.points}
        structs = bytearray()
        for point in self.model.points:
            path, offset = point.joint_path, point.offset
            move = moves.get(point.point_id)
            if move is not None:
                bone = self.template.bone_index(move.bone)
                path = self._joint_path(self.model_template.joints[bone])
                offset = self._local(bone, move.position)
            path_address = self.file.add(
                struct.pack(f">{len(path) + 1}h", *path, fmt.END_OF_JOINT_PATH), 4
            )
            structs += struct.pack(">I3fhxx", path_address, *offset, point.point_id)
        structs += bytes(fmt.CHARACTER_POINT_SIZE)
        return self.file.add(bytes(structs), 4)

    def _add_texture_animations(self) -> int:
        table = b"".join(
            struct.pack(">hh", animation.index, animation.frame_count)
            for animation in self.model.texture_animations
        )
        return self.file.add(table + TEXTURE_ANIMATIONS_END, 4)

    def _frame_layout(
        self, drawings: dict[int, _JointDrawing]
    ) -> tuple[list[_AnimatedTriangleSource], dict[int, int]]:
        """Place each face triangle's vertices and load list in the frame data."""
        sources = [
            source
            for joint in sorted(drawings)
            for source in drawings[joint].face_triangles
        ]
        vertices_start = FRAME_DATA_MATRICES_SIZE + PLACEHOLDER_TEXTURE_BYTES
        loads_start = (
            vertices_start + len(sources) * fmt.TRIANGLE_VERTEX_COUNT * fmt.VTX_SIZE
        )
        loads = {
            id(source): fmt.frame_data_address(loads_start + index * TRIANGLE_LOAD_SIZE)
            for index, source in enumerate(sources)
        }
        return sources, loads

    def _face_texture_table(
        self, sources: list[_AnimatedTriangleSource], textures: dict[TextureKey, int]
    ) -> tuple[list[TextureKey], int]:
        """Add the table of the textures that face triangles show."""
        shown: list[TextureKey] = []
        for source in sources:
            face = source.material.face
            if face is None:
                continue
            for path in face.images[self.kind].values():
                key = self._texture_key(source.material, path)
                if key not in shown:
                    shown.append(key)
        table = bytearray()
        for key in shown:
            texture = self._texture(key, animated=True)
            table += struct.pack(">IBBxx", textures[key], texture.width, texture.height)
        return shown, self.file.add(bytes(table) or bytes(fmt.ANIMATED_TEXTURE_SIZE), 4)

    def _face_image(
        self, source: _AnimatedTriangleSource, template_texture: int
    ) -> Path:
        face = source.material.face
        if face is None:
            msg = "a face triangle has no face images"
            raise BuilderBugError(msg)
        images = face.images[self.kind]
        needed = self.model_template.face_groups()[face.group]
        kind = self.kind.capitalize()
        _check(
            template_texture in images,
            (
                f"The face material {source.material.name} needs an image in place of "
                f"the template's {kind} {template_texture}; its {face.group} use "
                f"{', '.join(f'{kind} {index}' for index in needed)}"
            ),
        )
        return images[template_texture]

    def _add_animated_triangles(
        self, sources: list[_AnimatedTriangleSource], shown: list[TextureKey]
    ) -> tuple[int, bytes]:
        """Add the face triangles' keys, and build the frame data that they patch."""
        vertices_start = FRAME_DATA_MATRICES_SIZE + PLACEHOLDER_TEXTURE_BYTES
        loads_start = (
            vertices_start + len(sources) * fmt.TRIANGLE_VERTEX_COUNT * fmt.VTX_SIZE
        )
        frame_vertices = bytearray()
        frame_loads: list[gbi.Command] = []
        pointers: list[int] = []
        for index, source in enumerate(sources):
            template_triangle = self.model.animated_triangles[source.template_index]

            def coordinates(
                key: TextureKey, source: _AnimatedTriangleSource = source
            ) -> tuple[list[int], list[int]]:
                texture = self._texture(key, animated=True)
                st = [
                    _texture_st(uv, texture.width, texture.height)
                    for _, _, uv in source.corners
                ]
                return [s for s, _ in st], [t for _, t in st]

            key_sets = bytearray()
            for key_set in template_triangle.key_sets:
                keys = bytearray()
                for key in key_set.keys:
                    image = self._texture_key(
                        source.material, self._face_image(source, key.texture_index)
                    )
                    s, t = coordinates(image)
                    keys += struct.pack(
                        ">hh3h3h", key.frame, shown.index(image), *s, *t
                    )
                keys_address = self.file.add(bytes(keys), 4)
                key_sets += struct.pack(
                    ">hhI",
                    len(key_set.keys),
                    key_set.texture_animation_index,
                    keys_address,
                )
            key_sets_address = self.file.add(bytes(key_sets), 4)
            first = template_triangle.texture_index
            first_image = self._texture_key(
                source.material,
                self._face_image(
                    source,
                    first
                    if first >= 0
                    else template_triangle.key_sets[0].keys[0].texture_index,
                ),
            )
            s, t = coordinates(first_image)
            vertices_address = fmt.frame_data_address(
                vertices_start + index * 3 * fmt.VTX_SIZE
            )
            load_address = fmt.frame_data_address(
                loads_start + index * TRIANGLE_LOAD_SIZE
            )
            texture_index = shown.index(first_image) if first >= 0 else first
            pointers.append(
                self.file.add(
                    struct.pack(
                        ">IIIhh3h3h",
                        vertices_address,
                        load_address,
                        key_sets_address,
                        len(template_triangle.key_sets),
                        texture_index,
                        *s,
                        *t,
                    ),
                    4,
                )
            )
            for (position, normal, _), vertex_s, vertex_t in zip(
                source.corners, s, t, strict=True
            ):
                frame_vertices += _vtx(position, (vertex_s, vertex_t), normal)
            frame_loads += [
                gbi.texture(
                    TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, gbi.G_TX_RENDERTILE, on=True
                ),
                gbi.texture_lut(gbi.G_TT_NONE),
                *gbi.load_rgba16_block(
                    fmt.frame_data_address(FRAME_DATA_MATRICES_SIZE),
                    PLACEHOLDER_TEXTURE_SIZE,
                    PLACEHOLDER_TEXTURE_SIZE,
                    gbi.TileWrap(
                        gbi.G_TX_CLAMP,
                        gbi.G_TX_CLAMP,
                        ANIMATED_TEXTURE_MASK,
                        ANIMATED_TEXTURE_MASK,
                    ),
                ),
                gbi.vertex(vertices_address, fmt.TRIANGLE_VERTEX_COUNT, 0),
                gbi.end(),
            ]
        table = self.file.add(struct.pack(f">{len(pointers) + 1}I", *pointers, NULL), 4)
        # An Mtx holds the integer halves of its elements, then the fractions
        identity = struct.pack(
            ">16h32x", 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
        )
        frame = (
            identity * FRAME_DATA_JOINT_COUNT_MAX
            + struct.pack(">4H", *[PLACEHOLDER_TEXEL] * 4)
            + bytes(frame_vertices)
            + gbi.to_bytes(frame_loads)
        )
        return table, frame

    def build(self) -> tuple[bytes, tuple[int, ...], int, int]:
        """Lay the model file out; return it, its segments and frame data bounds."""
        _require(
            len(self.model.joints) <= FRAME_DATA_JOINT_COUNT_MAX,
            self.template.slot,
            (
                f"has {len(self.model.joints)} joints; the frame data holds "
                f"{FRAME_DATA_JOINT_COUNT_MAX}"
            ),
        )
        drawings = self._gather()
        copied = self._copy_template_lists()
        textures = self._add_textures(drawings)
        sources, triangle_loads = self._frame_layout(drawings)
        part_lists = {
            joint: self._add_part_list(drawing, textures, triangle_loads)
            for joint, drawing in sorted(drawings.items())
        }
        main_list = self._add_main_list(part_lists, copied)
        joints = self._copy_joints()
        animations = self._copy_animations()
        points = self._add_points()
        texture_animations = self._add_texture_animations()
        shown, texture_table = self._face_texture_table(sources, textures)
        triangle_table, frame = self._add_animated_triangles(sources, shown)
        choices = self.file.add(
            struct.pack(
                f">{len(self.model.display_list_choices) + 1}I",
                *(copied[choice] for choice in self.model.display_list_choices),
                END_OF_LIST,
            ),
            4,
        )
        camera = self.package.portrait_cameras.get(
            self.kind, self.model.portrait_camera
        )
        portrait = self.file.add(
            struct.pack(f">{PORTRAIT_CAMERA_VALUE_COUNT}f", *camera), 4
        )
        frame_start = self.file.add(frame, FRAME_DATA_ALIGNMENT)
        self.file.align(FRAME_DATA_ALIGNMENT)
        frame_end = self.file.next_address()
        segments = (
            main_list,
            copied[self.model.entry.segment_addresses[fmt.SEGMENT_CHOSEN_DISPLAY_LIST]],
            choices,
            animations,
            points,
            joints,
            texture_animations,
            triangle_table,
            texture_table,
            *(portrait + index * 4 for index in range(PORTRAIT_CAMERA_VALUE_COUNT)),
        )
        self._check_size(frame_start, frame_end)
        return bytes(self.file.data), segments, frame_start, frame_end

    def _check_size(self, frame_start: int, frame_end: int) -> None:
        start = fmt.segment_offset(frame_start)
        needed = buffer_use(frame_start, frame_end)
        where = f"{self.package.name}'s {self.kind} model"
        advice = "Use fewer triangles, fewer materials or smaller textures"
        _check(
            start <= MAPPED_MODEL_SIZE,
            (
                f"{where} is too big for the game: its parts and textures take "
                f"{start / KIBIBYTE:.1f} KiB, {start / MAPPED_MODEL_SIZE:.0%} of the "
                f"{MAPPED_MODEL_SIZE / KIBIBYTE:.0f} KiB the game maps. {advice}"
            ),
        )
        _check(
            needed <= USABLE_BUFFER_SIZE,
            (
                f"{where} is too big for the game: with its animation data it takes "
                f"{needed / KIBIBYTE:.1f} KiB, {needed / USABLE_BUFFER_SIZE:.0%} of "
                f"the {USABLE_BUFFER_SIZE / KIBIBYTE:.1f} KiB a character has. {advice}"
            ),
        )


def buffer_use(frame_start: int, frame_end: int) -> int:
    """Return how much of a slot's buffer a model takes: file, then two frame copies."""
    copy_size = max(frame_end - frame_start, FRAME_DATA_COPY_SIZE_MIN)
    return fmt.segment_offset(frame_start) + copy_size * FRAME_DATA_COPY_COUNT


@dataclass
class BuiltModel:
    """A model file, built, and its layout."""

    name: str
    data: bytes
    segments: tuple[int, ...]
    frame_start: int
    frame_end: int


def room_used(model: BuiltModel) -> float:
    """Return the fraction of a slot's buffer that a built model takes."""
    return buffer_use(model.frame_start, model.frame_end) / USABLE_BUFFER_SIZE


def model_file_names(slot: str) -> dict[str, list[str]]:
    """Each model kind's file names for a slot: its base file's, then its palettes'."""
    names: dict[str, list[str]] = {}
    for kind in MODEL_KINDS:
        name = slot + ("" if kind == "main" else tmpl.REACTION_SUFFIX)
        names[kind] = [
            name,
            *(f"{name}_palette_{palette}" for palette in range(1, PALETTE_COUNT + 1)),
        ]
    return names


def build_models(
    package: Package, template: tmpl.CharacterTemplate
) -> dict[str, list[BuiltModel]]:
    """Build each model kind's base file and palettes, decompressed."""
    models: dict[str, list[BuiltModel]] = {}
    for kind, file_names in model_file_names(template.slot).items():
        name = file_names[0]
        built: list[BuiltModel] = []
        for palette, file_name in enumerate(file_names):
            data, segments, start, end = ModelBuilder(
                package, template, kind, palette
            ).build()
            if built and (segments, start, end) != (
                built[0].segments,
                built[0].frame_start,
                built[0].frame_end,
            ):
                msg = (
                    f"{file_name}'s layout differs from {name}'s, so it can't patch it"
                )
                raise BuilderBugError(msg)
            built.append(BuiltModel(file_name, data, segments, start, end))
        models[kind] = built
    return models


def layout_define(name: str, base: BuiltModel, shadow_scale: int) -> str:
    """Return the definition that gives model_layouts.h a file's layout."""
    words = [f"0x{address:08X}" for address in base.segments]
    rows = [
        ", ".join(words[start : start + ADDRESSES_PER_LINE])
        for start in range(0, len(words), ADDRESSES_PER_LINE)
    ]
    segments = ", \\\n      ".join(rows)
    return (
        f"#define {name.upper()}{LAYOUT_SUFFIX} \\\n    {{ {segments} }}, \\\n    "
        f"{shadow_scale}, ModelRomBounds({base.name}), 0x{base.frame_start:08X}, "
        f"0x{base.frame_end:08X}"
    )


def write_models(
    package: Package, template: tmpl.CharacterTemplate, output: Path
) -> dict[str, str]:
    """Write a package's compressed model files, and return their layouts' defines."""
    models = output / MODELS_DIRECTORY
    models.mkdir(parents=True, exist_ok=True)
    defines: dict[str, str] = {}
    for kind, built in build_models(package, template).items():
        base = built[0]
        _ = (models / f"{base.name}.bin").write_bytes(lz_codec.encode(base.data))
        shadow_scale = template.models()[kind].model.entry.shadow_scale
        for palette in built[1:]:
            _ = (models / f"{palette.name}.bin").write_bytes(
                lz_codec.encode_patch(base.data, palette.data)
            )
        for model in built:
            define = layout_define(model.name, base, shadow_scale)
            defines[model.name.upper() + LAYOUT_SUFFIX] = define
    return defines


def _claim_slot(slots: dict[str, Path], slot: str, directory: Path) -> None:
    """Note that DIRECTORY's package replaces SLOT, unless another one does."""
    _check(
        slot not in slots,
        (
            f"{directory} and {slots.get(slot)} both replace {slot}, but a character "
            "can be replaced only once. Leave one of them out"
        ),
    )
    slots[slot] = directory


@final
class Arguments(argparse.Namespace):
    """The command line's arguments."""

    check: bool = False
    list_files: bool = False
    repository: Path = Path()
    output: Path | None = None
    header: Path | None = None
    text: list[Path] | None = None
    packages: list[Path] | None = None


def main() -> None:
    """Build or check the packages that the arguments name."""
    parser = argparse.ArgumentParser(description=SUMMARY)
    mode = parser.add_mutually_exclusive_group()
    _ = mode.add_argument(
        "--check", action="store_true", help="only check the packages"
    )
    _ = mode.add_argument(
        "--list-files", action="store_true", help="only name the model files replaced"
    )
    _ = parser.add_argument(
        "--repository", type=Path, default=Path(), help="the decompilation's root"
    )
    _ = parser.add_argument(
        "--output", type=Path, help="where the files that replace BASE's go"
    )
    _ = parser.add_argument(
        "--text",
        type=Path,
        action="append",
        help="a text file in BASE to copy to --output with the names changed",
    )
    _ = parser.add_argument(
        "packages", type=Path, nargs="+", help="character package directories"
    )
    arguments = parser.parse_args(namespace=Arguments())
    output = arguments.output
    building = not (arguments.check or arguments.list_files)
    if building and output is None:
        parser.error("give --output, --check or --list-files")
    layouts: dict[str, str] = {}
    slots: dict[str, Path] = {}
    packages: list[Package] = []
    try:
        for directory in arguments.packages or []:
            package = read_package(directory)
            _claim_slot(slots, package.slot, directory)
            packages.append(package)
            if arguments.list_files:
                for names in model_file_names(package.slot).values():
                    print("\n".join(names))
                continue
            template = tmpl.load_template(arguments.repository, package.slot)
            if arguments.check:
                print(check_report(package, build_models(package, template)))
            elif output is not None:
                layouts |= write_models(package, template, output)
        if output is not None and building:
            write_names(packages, arguments.repository, arguments.text or [], output)
            write_header(arguments.repository, LAYOUTS_HEADER, layouts, output)
    except (
        PackageError,
        BuilderBugError,
        tmpl.TemplateError,
        fmt.ModelFormatError,
        OSError,
    ) as error:
        sys.exit(f"build_character.py: {error}")


def write_header(
    repository: Path, header: Path, defines: dict[str, str], output: Path
) -> None:
    """Write to OUTPUT a copy of REPOSITORY's HEADER with DEFINES in place."""
    source = repository / header
    copy = output / header
    copy.parent.mkdir(parents=True, exist_ok=True)
    _ = copy.write_text(replace_defines(source.read_text(), defines, source))


def write_names(
    packages: list[Package], repository: Path, texts: list[Path], output: Path
) -> None:
    """Write to OUTPUT copies of the names header and TEXTS with the names changed.

    Each of TEXTS is a path within REPOSITORY, and its copy has the same
    path within OUTPUT.
    """
    write_header(repository, NAMES_HEADER, name_defines(packages), output)
    for text in texts:
        if text.is_absolute() or ".." in text.parts:
            msg = (
                f"--text {text} should be a path within the repository, as its copy is"
            )
            raise BuilderBugError(msg)
        copy = output / text
        copy.parent.mkdir(parents=True, exist_ok=True)
        renamed = rename_text((repository / text).read_text(encoding="utf-8"), packages)
        _ = copy.write_text(renamed, encoding="utf-8")


def check_report(package: Package, models: dict[str, list[BuiltModel]]) -> str:
    """Say that a package builds, and how much room its models leave."""
    sizes = ", ".join(
        f"{kind} {room_used(built[0]):.0%}" for kind, built in models.items()
    )
    return f"{package.name} can replace {package.slot}. Room used: {sizes}"


if __name__ == "__main__":
    main()
