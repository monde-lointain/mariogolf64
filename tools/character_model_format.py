"""Read the game's character model files: their skeletons, animations and parts.

A character model file is LZ-compressed (see lz_codec.py). Decompressed, it
is loaded at segment 2, 0x02000000, and its CharacterModel entry in
src/characters/character_model.c gives the segment addresses of what it
holds (see include/characters/character_model.h), as its layout in
include/characters/model_layouts.h has them. The file has no table of its
own, so reading one needs the entry, which read_model_table() parses from
the two.

The main display list draws the skeleton: each joint pushes its matrix,
which AnimateCharacterJoints writes to segment 3 each frame, onto its
parent's, calls the display list of the joint's part, and pops it after its
children. A joint's part is modeled in the joint's own space, so a part
moves rigidly with its joint.
"""

import math
import re
import struct
from collections.abc import Callable, Iterator
from dataclasses import dataclass, field
from pathlib import Path
from typing import cast

import lz_codec

MODEL_SEGMENT = 0x02
FRAME_DATA_SEGMENT = 0x03
SEGMENT_SHIFT = 24
SEGMENT_OFFSET_MASK = 0x00FFFFFF
END_OF_DISPLAY_LIST_CHOICES = 0xFFFFFFFF
END_OF_JOINT_PATH = -1
END_OF_JOINT_ANIMATIONS = -1
END_OF_ANIMATION_EVENTS = -1

# The segment addresses' indices in a CharacterModel
SEGMENT_DISPLAY_LIST = 0
SEGMENT_CHOSEN_DISPLAY_LIST = 1
SEGMENT_DISPLAY_LIST_CHOICES = 2
SEGMENT_ANIMATIONS = 3
SEGMENT_POINTS = 4
SEGMENT_JOINTS = 5
SEGMENT_TEXTURE_ANIMATIONS = 6
SEGMENT_ANIMATED_TRIANGLES = 7
SEGMENT_TEXTURES = 8
SEGMENT_PORTRAIT_CAMERA = 9
PORTRAIT_CAMERA_VALUE_COUNT = 6
SEGMENT_ADDRESS_COUNT = 15

# The sizes of the structures in include/characters/character_model.h
JOINT_ANIMATION_SIZE = 0x34
CHARACTER_ANIMATION_SIZE = 12
ANIMATION_EVENT_SIZE = 4
CHARACTER_POINT_SIZE = 20
TEXTURE_ANIMATION_SIZE = 4
ANIMATED_TEXTURE_SIZE = 8
TRIANGLE_KEY_SET_SIZE = 8
TRIANGLE_TEXTURE_KEY_SIZE = 16
TRIANGLE_VERTEX_COUNT = 3
VEC3_SIZE = 12
QUATERNION_SIZE = 8
MATRIX_SIZE = 0x40
VTX_SIZE = 0x10
GFX_SIZE = 8
# The chosen display list is a slot that SelectCharacterDisplayList fills
# with a call to the chosen club, or an end, and the end that follows the call
CHOSEN_DISPLAY_LIST_SIZE = 2 * GFX_SIZE
POINTER_SIZE = 4

# Rotation keys are quaternions in units of 1/16384
QUATERNION_SCALE = 1.0 / 16384

# F3DEX2 opcodes, the top byte of a command's first word
G_VTX = 0x01
G_TRI1 = 0x05
G_TRI2 = 0x06
G_TEXTURE = 0xD7
G_POPMTX = 0xD8
G_GEOMETRYMODE = 0xD9
G_MTX = 0xDA
G_MOVEMEM = 0xDC
G_DL = 0xDE
G_ENDDL = 0xDF
G_LOADBLOCK = 0xF3
G_LOADTLUT = 0xF0
G_SETTILE = 0xF5
G_SETPRIMCOLOR = 0xFA
G_SETTIMG = 0xFD
# A G_DL's parameter that makes it a branch, which doesn't return
G_DL_NOPUSH = 1
# A G_MTX's parameter bits, the low byte of its first word XORed with PUSH
G_MTX_PUSH = 0x01
# The vertex buffer's size, in vertices
VERTEX_BUFFER_SIZE = 32
# Bytes a texel takes for each G_IM_SIZ value
TEXEL_SIZES = {0: 0.5, 1: 1, 2: 2, 3: 4}
# A MOVEMEM's length is (its field + 1) * 8 bytes
MOVEMEM_LENGTH_SHIFT = 19
MOVEMEM_LENGTH_MASK = 0x1F
MOVEMEM_LENGTH_UNIT = 8

# The CharacterModel table's names in character_model.c
MODEL_TABLE_NAME = "g_CharacterModels"
OPENING_TABLE_NAME = "g_OpeningCharacterModels"


class ModelFormatError(ValueError):
    """A model file, or its table entry, isn't as the game reads it."""


def segment_offset(address: int) -> int:
    """Return the offset into its segment of a segment address."""
    return address & SEGMENT_OFFSET_MASK


def segment_of(address: int) -> int:
    """Return the segment number of a segment address."""
    return address >> SEGMENT_SHIFT


def model_address(offset: int) -> int:
    """Return the segment address of an offset into a model file."""
    return (MODEL_SEGMENT << SEGMENT_SHIFT) | offset


def frame_data_address(offset: int) -> int:
    """Return the segment address of an offset into a model's frame data."""
    return (FRAME_DATA_SEGMENT << SEGMENT_SHIFT) | offset


@dataclass(frozen=True)
class ModelEntry:
    """A CharacterModel: which file a model index loads, and where its parts are."""

    index: int
    rom_name: str
    segment_addresses: tuple[int, ...]
    shadow_scale: int
    base_name: str
    frame_data_start: int
    frame_data_end: int

    @property
    def is_patch(self) -> bool:
        """Whether the file patches a base file, as a palette's does."""
        return self.rom_name != self.base_name


# An entry names its file, then the macro that gives its layout
_ENTRY_PATTERN = re.compile(r"ModelRomBounds\((\w+)\),\s*(\w+_MODEL_LAYOUT)\b")
_LAYOUT_SOURCE = (
    r"#define (\w+_MODEL_LAYOUT)\s*\\\s*"
    r"\{([^}]*)\},\s*\\\s*(-?\d+),\s*"
    r"ModelRomBounds\((\w+)\),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+)"
)
_LAYOUT_PATTERN = re.compile(_LAYOUT_SOURCE)


def read_model_table(source: Path, layouts: Path) -> list[ModelEntry]:
    """Parse g_CharacterModels' entries, in model index order.

    SOURCE is the file that holds the table, and LAYOUTS the header whose
    macros give each entry's layout.
    """
    text = source.read_text()
    start = text.find(MODEL_TABLE_NAME + "[")
    end = text.find(OPENING_TABLE_NAME + "[")
    if start < 0 or end < start:
        msg = f"{source} has no {MODEL_TABLE_NAME} table before {OPENING_TABLE_NAME}"
        raise ModelFormatError(msg)
    found = cast(
        "list[tuple[str, str, str, str, str, str]]",
        _LAYOUT_PATTERN.findall(layouts.read_text()),
    )
    layout_of = {layout[0]: layout[1:] for layout in found}
    entries: list[ModelEntry] = []
    for match in _ENTRY_PATTERN.finditer(text, start, end):
        rom_name, macro = match.groups()
        if macro not in layout_of:
            msg = f"{layouts} doesn't define {macro}, which {source} uses"
            raise ModelFormatError(msg)
        segments, shadow, base_name, frame_start, frame_end = layout_of[macro]
        words = cast("list[str]", re.findall(r"0x[0-9A-Fa-f]+", segments))
        addresses = tuple(int(word, 16) for word in words)
        if len(addresses) != SEGMENT_ADDRESS_COUNT:
            msg = f"{rom_name}'s entry has {len(addresses)} segment addresses"
            raise ModelFormatError(msg)
        entries.append(
            ModelEntry(
                len(entries),
                rom_name,
                addresses,
                int(shadow),
                base_name,
                int(frame_start, 16),
                int(frame_end, 16),
            )
        )
    return entries


def find_entry(entries: list[ModelEntry], rom_name: str) -> ModelEntry:
    """Return the first entry that loads the file ROM_NAME."""
    for entry in entries:
        if entry.rom_name == rom_name:
            return entry
    msg = f"no character model loads a file named {rom_name}"
    raise ModelFormatError(msg)


Vec3 = tuple[float, float, float]
Quaternion = tuple[int, int, int, int]


@dataclass
class JointAnimation:
    """A joint's keyed translation, rotation (quaternion x, y, z, w) and scale."""

    translation_frames: list[int]
    translations: list[Vec3]
    rotation_frames: list[int]
    rotations: list[Quaternion]
    scale_frames: list[int]
    scales: list[Vec3]


@dataclass
class AnimationEvent:
    """Something that happens on a frame of an animation."""

    frame: int
    kind: int
    value: int


@dataclass
class CharacterAnimation:
    """A span of the joints' keyframes, played as one animation."""

    events: list[AnimationEvent]
    start_frame: int
    frame_count: int
    animation_id: int


@dataclass
class CharacterPoint:
    """A point on the model, offset from the last joint of a path from the root."""

    joint_path: list[int]
    offset: Vec3
    point_id: int


@dataclass
class TextureAnimation:
    """How many frames a texture animation runs."""

    index: int
    frame_count: int


@dataclass
class TriangleTextureKey:
    """The texture an animated triangle shows from a frame of an animation on."""

    frame: int
    texture_index: int
    s: tuple[int, int, int]
    t: tuple[int, int, int]


@dataclass
class TriangleKeySet:
    """The keys that one texture animation gives an animated triangle."""

    texture_animation_index: int
    keys: list[TriangleTextureKey]


@dataclass
class AnimatedTriangle:
    """A triangle of the face, whose texture the texture animations change."""

    vertices: int
    load_texture_block: int
    key_sets: list[TriangleKeySet]
    texture_index: int
    s: tuple[int, int, int]
    t: tuple[int, int, int]


@dataclass
class AnimatedTexture:
    """An RGBA16 texture that an animated triangle can show."""

    address: int
    width: int
    height: int


@dataclass
class SkeletonNode:
    """A matrix the main display list pushes, and the display lists drawn in it.

    joint is the joint's index, or -1 for a fixed matrix in the file, whose
    address matrix_address holds.
    """

    joint: int
    matrix_address: int
    display_lists: list[int] = field(default_factory=list)
    children: list["SkeletonNode"] = field(default_factory=list)


@dataclass
class ModelFile:
    """A decompressed model file and the parts of it that its entry locates."""

    entry: ModelEntry
    data: bytes
    joints: list[JointAnimation]
    animations: list[CharacterAnimation]
    points: list[CharacterPoint]
    texture_animations: list[TextureAnimation]
    animated_triangles: list[AnimatedTriangle]
    textures: list[AnimatedTexture]
    skeleton: list[SkeletonNode]
    display_list_choices: list[int]
    portrait_camera: list[float]

    def joint_parents(self) -> list[int]:
        """Each joint's parent joint, -1 for a root."""
        parents = [-1] * len(self.joints)
        for node, parent in walk_skeleton(self.skeleton):
            if node.joint >= 0:
                parents[node.joint] = parent.joint if parent else -1
        return parents

    def joint_display_lists(self) -> list[list[int]]:
        """Return the display lists that each joint draws, but fixed matrices' lists."""
        lists: list[list[int]] = [[] for _ in self.joints]
        for node, _ in walk_skeleton(self.skeleton):
            if node.joint >= 0:
                lists[node.joint].extend(node.display_lists)
        return lists


def walk_skeleton(
    nodes: list[SkeletonNode], parent: SkeletonNode | None = None
) -> Iterator[tuple[SkeletonNode, SkeletonNode | None]]:
    """Visit each node, parents first, with its nearest joint parent."""
    for node in nodes:
        yield node, parent
        yield from walk_skeleton(node.children, node if node.joint >= 0 else parent)


class _Reader:
    """Reads big-endian values from a decompressed file at segment addresses."""

    def __init__(self, data: bytes) -> None:
        self.data: bytes = data

    def check(self, address: int, size: int) -> int:
        offset = segment_offset(address)
        if segment_of(address) != MODEL_SEGMENT or offset + size > len(self.data):
            msg = f"0x{address:08X} isn't in the {len(self.data):#x}-byte model file"
            raise ModelFormatError(msg)
        return offset

    def unpack(self, layout: str, address: int) -> tuple[int | float, ...]:
        offset = self.check(address, struct.calcsize(layout))
        return struct.unpack_from(layout, self.data, offset)

    def u32(self, address: int) -> int:
        return int(self.unpack(">I", address)[0])

    def s16(self, address: int) -> int:
        return int(self.unpack(">h", address)[0])

    def f32(self, address: int) -> float:
        return float(self.unpack(">f", address)[0])

    def vec3s(self, address: int, count: int) -> list[Vec3]:
        values: list[Vec3] = []
        for index in range(count):
            x, y, z = self.unpack(">3f", address + index * VEC3_SIZE)
            values.append((float(x), float(y), float(z)))
        return values

    def s16s(self, address: int, count: int) -> list[int]:
        return [self.s16(address + index * 2) for index in range(count)]


def _read_joints(reader: _Reader, address: int) -> list[JointAnimation]:
    joints: list[JointAnimation] = []
    while (counts := reader.unpack(">3h", address))[0] != END_OF_JOINT_ANIMATIONS:
        translation_count, rotation_count, scale_count = (
            int(count) for count in counts
        )
        pointers = [int(p) for p in reader.unpack(">6I", address + 8)]
        rotations: list[Quaternion] = []
        for index in range(rotation_count):
            x, y, z, w = reader.unpack(">4h", pointers[4] + index * QUATERNION_SIZE)
            rotations.append((int(x), int(y), int(z), int(w)))
        joints.append(
            JointAnimation(
                reader.s16s(pointers[0], translation_count),
                reader.vec3s(pointers[3], translation_count),
                reader.s16s(pointers[1], rotation_count),
                rotations,
                reader.s16s(pointers[2], scale_count),
                reader.vec3s(pointers[5], scale_count),
            )
        )
        address += JOINT_ANIMATION_SIZE
    return joints


def _read_animations(reader: _Reader, address: int) -> list[CharacterAnimation]:
    animations: list[CharacterAnimation] = []
    while (events_address := reader.u32(address)) != 0:
        events: list[AnimationEvent] = []
        event = events_address
        while (frame := reader.s16(event)) != END_OF_ANIMATION_EVENTS:
            _, kind, value = reader.unpack(">hBB", event)
            events.append(AnimationEvent(frame, int(kind), int(value)))
            event += ANIMATION_EVENT_SIZE
        _, start, count, animation_id = reader.unpack(">IhhB", address)
        animations.append(
            CharacterAnimation(events, int(start), int(count), int(animation_id))
        )
        address += CHARACTER_ANIMATION_SIZE
    return animations


def _read_points(reader: _Reader, address: int) -> list[CharacterPoint]:
    points: list[CharacterPoint] = []
    while (path_address := reader.u32(address)) != 0:
        path: list[int] = []
        while (joint := reader.s16(path_address + 2 * len(path))) != END_OF_JOINT_PATH:
            path.append(joint)
        offset = reader.vec3s(address + POINTER_SIZE, 1)[0]
        points.append(CharacterPoint(path, offset, reader.s16(address + 16)))
        address += CHARACTER_POINT_SIZE
    return points


def _read_texture_animations(reader: _Reader, address: int) -> list[TextureAnimation]:
    animations: list[TextureAnimation] = []
    while (index := reader.s16(address)) >= 0:
        animations.append(TextureAnimation(index, reader.s16(address + 2)))
        address += TEXTURE_ANIMATION_SIZE
    return animations


def _read_coordinates(reader: _Reader, address: int) -> tuple[list[int], list[int]]:
    values = [int(value) for value in reader.unpack(">6h", address)]
    return values[:TRIANGLE_VERTEX_COUNT], values[TRIANGLE_VERTEX_COUNT:]


def _read_animated_triangles(reader: _Reader, address: int) -> list[AnimatedTriangle]:
    triangles: list[AnimatedTriangle] = []
    while (triangle := reader.u32(address)) != 0:
        vertices, load_block, key_sets_address, key_set_count, texture_index = (
            int(value) for value in reader.unpack(">IIIhh", triangle)
        )
        s, t = _read_coordinates(reader, triangle + 16)
        key_sets: list[TriangleKeySet] = []
        for set_index in range(key_set_count):
            count, animation_index, keys_address = (
                int(value)
                for value in reader.unpack(
                    ">hhI", key_sets_address + set_index * TRIANGLE_KEY_SET_SIZE
                )
            )
            keys: list[TriangleTextureKey] = []
            for key_index in range(count):
                key = keys_address + key_index * TRIANGLE_TEXTURE_KEY_SIZE
                frame, texture = (int(value) for value in reader.unpack(">hh", key))
                key_s, key_t = _read_coordinates(reader, key + 4)
                keys.append(
                    TriangleTextureKey(frame, texture, _triple(key_s), _triple(key_t))
                )
            key_sets.append(TriangleKeySet(animation_index, keys))
        triangles.append(
            AnimatedTriangle(
                vertices, load_block, key_sets, texture_index, _triple(s), _triple(t)
            )
        )
        address += POINTER_SIZE
    return triangles


def _triple(values: list[int]) -> tuple[int, int, int]:
    return (values[0], values[1], values[2])


def _read_textures(
    reader: _Reader, address: int, triangles: list[AnimatedTriangle]
) -> list[AnimatedTexture]:
    """Read as many textures as the animated triangles use; the table has no end."""
    used = [triangle.texture_index for triangle in triangles]
    used += [
        key.texture_index
        for triangle in triangles
        for key_set in triangle.key_sets
        for key in key_set.keys
    ]
    count = max(used, default=-1) + 1
    textures: list[AnimatedTexture] = []
    for index in range(count):
        texture_address, width, height = reader.unpack(
            ">IBB", address + index * ANIMATED_TEXTURE_SIZE
        )
        textures.append(AnimatedTexture(int(texture_address), int(width), int(height)))
    return textures


def commands(data: bytes, address: int) -> Iterator[tuple[int, int, int]]:
    """Yield (offset, word 0, word 1) of each command of a display list in DATA.

    Stops after its G_ENDDL or a branch.
    """
    offset = segment_offset(address)
    while offset + GFX_SIZE <= len(data):
        word0, word1 = struct.unpack_from(">II", data, offset)
        yield offset, word0, word1
        opcode = word0 >> SEGMENT_SHIFT
        if opcode == G_ENDDL or (
            opcode == G_DL and (word0 >> 16) & 0xFF == G_DL_NOPUSH
        ):
            return
        offset += GFX_SIZE
    msg = f"the display list at 0x{address:08X} runs off the end of the file"
    raise ModelFormatError(msg)


def _read_skeleton(data: bytes, address: int) -> list[SkeletonNode]:
    """Rebuild the matrix tree that the main display list pushes and pops."""
    roots: list[SkeletonNode] = []
    stack: list[SkeletonNode] = []
    for _, word0, word1 in commands(data, address):
        opcode = word0 >> SEGMENT_SHIFT
        if opcode == G_MTX:
            # The command stores its parameters with PUSH inverted
            parameters = (word0 & 0xFF) ^ G_MTX_PUSH
            if not parameters & G_MTX_PUSH:
                msg = (
                    f"the main display list loads a matrix without a push: {word0:08X}"
                )
                raise ModelFormatError(msg)
            is_joint = segment_of(word1) == FRAME_DATA_SEGMENT
            node = SkeletonNode(
                segment_offset(word1) // MATRIX_SIZE if is_joint else -1,
                0 if is_joint else word1,
            )
            (stack[-1].children if stack else roots).append(node)
            stack.append(node)
        elif opcode == G_POPMTX:
            for _ in range(word1 // MATRIX_SIZE):
                _ = stack.pop()
        elif opcode == G_DL:
            if not stack:
                msg = "the main display list draws before it pushes a joint"
                raise ModelFormatError(msg)
            stack[-1].display_lists.append(word1)
    return roots


def _read_choices(reader: _Reader, address: int) -> list[int]:
    choices: list[int] = []
    while (choice := reader.u32(address)) != END_OF_DISPLAY_LIST_CHOICES:
        choices.append(choice)
        address += POINTER_SIZE
    return choices


def read_model(data: bytes, entry: ModelEntry) -> ModelFile:
    """Read the parts of a decompressed model file that ENTRY locates."""
    reader = _Reader(data)
    segments = entry.segment_addresses
    triangles = _read_animated_triangles(reader, segments[SEGMENT_ANIMATED_TRIANGLES])
    portrait = [
        reader.f32(segments[SEGMENT_PORTRAIT_CAMERA + index])
        for index in range(PORTRAIT_CAMERA_VALUE_COUNT)
    ]
    return ModelFile(
        entry,
        data,
        _read_joints(reader, segments[SEGMENT_JOINTS]),
        _read_animations(reader, segments[SEGMENT_ANIMATIONS]),
        _read_points(reader, segments[SEGMENT_POINTS]),
        _read_texture_animations(reader, segments[SEGMENT_TEXTURE_ANIMATIONS]),
        triangles,
        _read_textures(reader, segments[SEGMENT_TEXTURES], triangles),
        _read_skeleton(data, segments[SEGMENT_DISPLAY_LIST]),
        _read_choices(reader, segments[SEGMENT_DISPLAY_LIST_CHOICES]),
        portrait,
    )


def load_model(models_dir: Path, entry: ModelEntry) -> ModelFile:
    """Decompress and read ENTRY's file from MODELS_DIR, patching a palette's base."""
    data = lz_codec.decode((models_dir / f"{entry.base_name}.bin").read_bytes())
    if entry.is_patch:
        data = lz_codec.apply_patch(
            data, (models_dir / f"{entry.rom_name}.bin").read_bytes()
        )
    return read_model(data, entry)


# Matrices are 4x4, row-major, and act on row vectors as the game's do: a
# point p maps to p * M, so the product A * B applies A first.
Matrix = list[list[float]]


def multiply(first: Matrix, second: Matrix) -> Matrix:
    """Return the matrix that applies FIRST, then SECOND."""
    return [
        [
            sum(first[row][k] * second[k][column] for k in range(4))
            for column in range(4)
        ]
        for row in range(4)
    ]


def transform_point(point: Vec3, matrix: Matrix) -> Vec3:
    """Map a point through a matrix."""
    x, y, z = point
    return (
        x * matrix[0][0] + y * matrix[1][0] + z * matrix[2][0] + matrix[3][0],
        x * matrix[0][1] + y * matrix[1][1] + z * matrix[2][1] + matrix[3][1],
        x * matrix[0][2] + y * matrix[1][2] + z * matrix[2][2] + matrix[3][2],
    )


def quaternion_matrix(quaternion: tuple[float, float, float, float]) -> Matrix:
    """Return the rotation of a quaternion (x, y, z, w) of any length.

    This is as QuaternionToMatrix finds it.
    """
    x, y, z, w = quaternion
    length_squared = x * x + y * y + z * z + w * w
    scale = 2.0 / length_squared if length_squared > 0 else 0.0
    xs, ys, zs = x * scale, y * scale, z * scale
    wx, wy, wz = w * xs, w * ys, w * zs
    xx, yy, zz = x * xs, y * ys, z * zs
    xy, yz, xz = x * ys, y * zs, x * zs
    return [
        [1.0 - (yy + zz), xy + wz, xz - wy, 0.0],
        [xy - wz, 1.0 - (xx + zz), yz + wx, 0.0],
        [xz + wy, yz - wx, 1.0 - (xx + yy), 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def _keyed_value(frames: list[int], values: list[Vec3], frame: float) -> Vec3:
    """Return a linearly keyed value at FRAME, held outside the first and last keys."""
    if frame < frames[0]:
        return values[0]
    for index in range(1, len(frames)):
        if frame < frames[index]:
            progress = (frame - frames[index - 1]) / (frames[index] - frames[index - 1])
            start, end = values[index - 1], values[index]
            return (
                start[0] + (end[0] - start[0]) * progress,
                start[1] + (end[1] - start[1]) * progress,
                start[2] + (end[2] - start[2]) * progress,
            )
    return values[-1]


SLERP_THRESHOLD = 1e-06


def _keyed_rotation(
    frames: list[int], rotations: list[Quaternion], frame: float
) -> tuple[float, float, float, float]:
    """Return the slerped rotation at FRAME, as AnimateCharacterJoints finds it."""

    def scaled(index: int) -> tuple[float, float, float, float]:
        x, y, z, w = rotations[index]
        return (
            x * QUATERNION_SCALE,
            y * QUATERNION_SCALE,
            z * QUATERNION_SCALE,
            w * QUATERNION_SCALE,
        )

    if frame < frames[0]:
        return scaled(0)
    for index in range(1, len(frames)):
        if frame < frames[index]:
            start, end = scaled(index - 1), scaled(index)
            progress = (frame - frames[index - 1]) / (frames[index] - frames[index - 1])
            dot = sum(a * b for a, b in zip(start, end, strict=True))
            if 1.0 - dot < SLERP_THRESHOLD:
                start_weight, end_weight = 1.0 - progress, progress
            else:
                angle = math.acos(max(-1.0, min(1.0, dot)))
                sine = math.sin(angle)
                start_weight = math.sin(angle - progress * angle) / sine
                end_weight = math.sin(progress * angle) / sine
            x, y, z, w = (
                start_weight * a + end_weight * b
                for a, b in zip(start, end, strict=True)
            )
            return (x, y, z, w)
    return scaled(len(frames) - 1)


def joint_local_matrix(
    joint: JointAnimation, frame: float, translation_offset: Vec3 = (0.0, 0.0, 0.0)
) -> Matrix:
    """Return a joint's matrix in its parent's at FRAME: scale, rotate, translate."""
    x, y, z = _keyed_value(joint.translation_frames, joint.translations, frame)
    sx, sy, sz = _keyed_value(joint.scale_frames, joint.scales, frame)
    matrix = quaternion_matrix(
        _keyed_rotation(joint.rotation_frames, joint.rotations, frame)
    )
    for column in range(3):
        matrix[0][column] *= sx
        matrix[1][column] *= sy
        matrix[2][column] *= sz
    matrix[3][0] = x + translation_offset[0]
    matrix[3][1] = y + translation_offset[1]
    matrix[3][2] = z + translation_offset[2]
    return matrix


def joint_world_matrices(
    model: ModelFile,
    frame: float,
    translation_offsets: dict[int, Vec3] | None = None,
) -> list[Matrix]:
    """Return each joint's model-space matrix at FRAME, as the main list composes it."""
    offsets = translation_offsets or {}
    parents = model.joint_parents()
    world: list[Matrix | None] = [None] * len(model.joints)

    def resolve(index: int) -> Matrix:
        cached = world[index]
        if cached is not None:
            return cached
        local = joint_local_matrix(
            model.joints[index], frame, offsets.get(index, (0.0, 0.0, 0.0))
        )
        parent = parents[index]
        matrix = local if parent < 0 else multiply(local, resolve(parent))
        world[index] = matrix
        return matrix

    return [resolve(index) for index in range(len(model.joints))]


@dataclass
class DrawnVertex:
    """A vertex as a display list loads it."""

    position: tuple[int, int, int]
    st: tuple[int, int]
    color_or_normal: tuple[int, int, int, int]


@dataclass
class DrawnTriangle:
    """A triangle a display list draws, with the primitive color it's drawn in."""

    vertices: tuple[DrawnVertex, DrawnVertex, DrawnVertex]
    prim_color: tuple[int, int, int, int]
    texture_address: int


def read_vertices(data: bytes, address: int, count: int) -> list[DrawnVertex]:
    """Read COUNT Vtx at a model address."""
    offset = segment_offset(address)
    vertices: list[DrawnVertex] = []
    for index in range(count):
        x, y, z, _, s, t, r, g, b, a = struct.unpack_from(
            ">hhhHhhBBBB", data, offset + index * VTX_SIZE
        )
        vertices.append(DrawnVertex((x, y, z), (s, t), (r, g, b, a)))
    return vertices


class _TriangleDrawer:
    """Follows display lists as the RSP would, keeping the triangles drawn."""

    def __init__(
        self,
        sources: dict[int, bytes],
        on_call: Callable[[int], None] | None,
    ) -> None:
        self.sources: dict[int, bytes] = sources
        self.on_call: Callable[[int], None] | None = on_call
        self.buffer: list[DrawnVertex | None] = [None] * VERTEX_BUFFER_SIZE
        self.triangles: list[DrawnTriangle] = []
        self.prim: tuple[int, int, int, int] = (255, 255, 255, 255)
        self.texture: int = 0
        self.handlers: dict[int, Callable[[int, int], None]] = {
            G_VTX: self._load_vertices,
            G_TRI1: self._draw,
            G_TRI2: self._draw,
            G_SETPRIMCOLOR: self._set_prim,
            G_SETTIMG: self._set_texture,
            G_DL: self._call,
        }

    def run(self, list_address: int, list_data: bytes) -> None:
        """Follow the list at LIST_ADDRESS in LIST_DATA."""
        for _, word0, word1 in commands(list_data, list_address):
            handler = self.handlers.get(word0 >> SEGMENT_SHIFT)
            if handler is not None:
                handler(word0, word1)

    def _load_vertices(self, word0: int, word1: int) -> None:
        count = (word0 >> 12) & 0xFF
        end = (word0 & 0xFF) >> 1
        source = self.sources.get(segment_of(word1), b"")
        # Vertices it can't read leave holes, so that no triangle is drawn
        # from those the load replaced
        loaded = read_vertices(source, word1, count) if source else [None] * count
        self.buffer[end - count : end] = loaded

    def _draw(self, word0: int, word1: int) -> None:
        for indices in _triangle_indices(word0 >> SEGMENT_SHIFT, word0, word1):
            a, b, c = (self.buffer[index] for index in indices)
            if a is not None and b is not None and c is not None:
                self.triangles.append(DrawnTriangle((a, b, c), self.prim, self.texture))

    def _set_prim(self, _word0: int, word1: int) -> None:
        self.prim = (
            word1 >> 24,
            (word1 >> 16) & 0xFF,
            (word1 >> 8) & 0xFF,
            word1 & 0xFF,
        )

    def _set_texture(self, _word0: int, word1: int) -> None:
        self.texture = word1

    def _call(self, _word0: int, word1: int) -> None:
        if self.on_call:
            self.on_call(word1)
        source = self.sources.get(segment_of(word1))
        if source:
            self.run(word1, source)
        else:
            # The list it can't follow may load vertices
            self.buffer[:] = [None] * VERTEX_BUFFER_SIZE


def draw_triangles(
    data: bytes,
    address: int,
    frame_data: bytes = b"",
    on_call: Callable[[int], None] | None = None,
) -> list[DrawnTriangle]:
    """List the triangles a part's display list draws, following its calls.

    Vertices loaded from the frame data (segment 3) come from FRAME_DATA when
    given; calls to other segments are skipped, after ON_CALL hears of them.
    """
    drawer = _TriangleDrawer(
        {MODEL_SEGMENT: data, FRAME_DATA_SEGMENT: frame_data}, on_call
    )
    drawer.run(address, data)
    return drawer.triangles


def _triangle_indices(
    opcode: int, word0: int, word1: int
) -> list[tuple[int, int, int]]:
    """Return the vertex buffer indices of the one or two triangles of a command."""

    def unpack(word: int) -> tuple[int, int, int]:
        return ((word >> 16 & 0xFF) // 2, (word >> 8 & 0xFF) // 2, (word & 0xFF) // 2)

    if opcode == G_TRI1:
        return [unpack(word0)]
    return [unpack(word0), unpack(word1)]


@dataclass
class Blob:
    """A run of a file that display lists reference, and what it holds."""

    start: int
    end: int
    kind: str


def _command_data(opcode: int, word0: int, word1: int) -> Blob | None:
    """Return the data that a command reads and gives the size of, if it does."""
    start = segment_offset(word1)
    if opcode == G_VTX:
        count = (word0 >> 12) & 0xFF
        return Blob(start, start + count * VTX_SIZE, "vertices")
    if opcode == G_MTX:
        return Blob(start, start + MATRIX_SIZE, "matrix")
    if opcode == G_MOVEMEM:
        units = ((word0 >> MOVEMEM_LENGTH_SHIFT) & MOVEMEM_LENGTH_MASK) + 1
        length = units * MOVEMEM_LENGTH_UNIT
        return Blob(start, start + length, "movemem")
    return None


def referenced_blobs(data: bytes, roots: list[int]) -> list[Blob]:
    """Find what the display lists at ROOTS reference in the file, them included.

    Follows calls within the file. Each texture's size comes from the load
    that follows its G_SETTIMG; vertices, matrices and MOVEMEM data are sized
    by their commands.
    """
    blobs: list[Blob] = []
    seen: set[int] = set()
    pending = [root for root in roots if segment_of(root) == MODEL_SEGMENT]
    while pending:
        address = pending.pop()
        if address in seen:
            continue
        seen.add(address)
        texture_address = 0
        texel_size = 0.0
        last_offset = segment_offset(address)
        for offset, word0, word1 in commands(data, address):
            last_offset = offset
            opcode = word0 >> SEGMENT_SHIFT
            is_local = segment_of(word1) == MODEL_SEGMENT
            data_blob = _command_data(opcode, word0, word1) if is_local else None
            if opcode == G_DL and is_local:
                pending.append(word1)
            elif data_blob is not None:
                blobs.append(data_blob)
            elif opcode == G_SETTIMG:
                texture_address = word1 if is_local else 0
                texel_size = TEXEL_SIZES[(word0 >> 19) & 3]
            elif opcode in {G_LOADBLOCK, G_LOADTLUT} and texture_address:
                start = segment_offset(texture_address)
                if opcode == G_LOADBLOCK:
                    texel_count = ((word1 >> 12) & 0xFFF) + 1
                else:
                    texel_count = ((word1 >> 14) & 0x3FF) + 1
                size = math.ceil(texel_count * texel_size)
                blobs.append(Blob(start, start + size, "texture"))
        blobs.append(
            Blob(segment_offset(address), last_offset + GFX_SIZE, "display list")
        )
    return blobs


def merge_blobs(blobs: list[Blob]) -> list[Blob]:
    """Merge overlapping or touching blobs of the same kind into runs."""
    merged: list[Blob] = []
    for blob in sorted(blobs, key=lambda b: (b.start, b.end)):
        if merged and blob.start <= merged[-1].end:
            last = merged[-1]
            last.end = max(last.end, blob.end)
            if last.kind != blob.kind:
                last.kind = "mixed"
            continue
        merged.append(Blob(blob.start, blob.end, blob.kind))
    return merged
