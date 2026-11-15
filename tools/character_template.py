#!/usr/bin/env python3
"""Work out the skeleton a custom character is modeled on, from the game's files.

A custom character takes the place of a character slot, such as Charlie's,
and keeps that character's animations, so it is modeled on that character's
skeleton. The slot has two models, each with its own skeleton: the main one,
for the course and menus, and the reaction one, for the close-up after a
shot. A template describes both, from the user's own extracted files.

The main model's joints have no rotation at rest and keep fixed offsets from
their parents, so its rest pose, the bind pose, is found by adding the
offsets up from the root. A custom character is modeled in that pose, each
part bound to one bone, and moves rigidly with it, as the game's parts do.

The reaction model holds the same parts in model space, but some in other
places: Charlie's arms are closer in. So each main joint's part is matched to
the reaction joint whose part it fits, and the translation that fits it is
how far the reaction joint's space is from the bind pose for that bone.
"""

import argparse
import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import cast, final

import numpy as np
from numpy.typing import NDArray
from PIL import Image

import character_model_format as fmt

REACTION_SUFFIX = "_reaction"
TEMPLATE_FORMAT = "mariogolf64-character-template"
TEMPLATE_VERSION = 1
SUMMARY = "Work out the skeleton a custom character is modeled on, from its files."

Vec3 = tuple[float, float, float]
FloatArray = NDArray[np.float64]
# numpy's signatures that take array-likes name collections.abc.Buffer,
# which the type checker's Python 3.10 lacks, so the type checker is told
# to ignore that it can't fully know them where they're called

# The bone names of the main skeletons whose shape is known, by each joint's
# parent. Other skeletons' bones are named by joint number.
HUMAN_SKELETON_PARENTS = (
    -1,
    0,
    1,
    1,
    3,
    4,
    5,
    6,
    7,
    3,
    9,
    10,
    11,
    0,
    13,
    14,
    15,
    16,
    14,
    18,
    19,
)
HUMAN_BONE_NAMES = (
    "root",
    "chest",
    "head",
    "shoulders",
    "shoulder.R",
    "upper_arm.R",
    "forearm.R",
    "hand.R",
    "club",
    "shoulder.L",
    "upper_arm.L",
    "forearm.L",
    "hand.L",
    "hips_root",
    "hips",
    "thigh.R",
    "shin.R",
    "foot.R",
    "thigh.L",
    "shin.L",
    "foot.L",
)
KNOWN_SKELETONS: dict[tuple[int, ...], tuple[str, ...]] = {
    HUMAN_SKELETON_PARENTS: HUMAN_BONE_NAMES
}

ROOT_BONE = 0
# An RGBA5551 texel's color channels are 5 bits each
RGBA5551_CHANNEL_MASK = 0x1F
ALPHA_OPAQUE = 255
TEXTURES_SUFFIX = "_textures"

# How many times the reaction fit moves a part onto its nearest points
FIT_ITERATIONS = 12
# A reaction part fits a main one when, after the fit, the main part's points
# are this close to the reaction's on average, as a fraction of its size
FIT_TOLERANCE = 0.12
# Parts of the two models are taken for the same part, when placing the
# reaction model's root, if their bounding boxes differ by this little
SAME_SIZE_TOLERANCE = 2.0
# The face triangles of two groups are told apart by height: the highest
# group shows the eyes
EYES_GROUP = "eyes"
MOUTH_GROUP = "mouth"


class TemplateError(ValueError):
    """A slot's models can't serve as a template."""


@dataclass
class Bone:
    """A joint of the main model, as a custom character binds to it."""

    name: str
    joint: int
    parent: int
    head: Vec3  # Where the joint is in the bind pose
    draws: bool  # Whether the slot's character has a part on it


@dataclass
class FaceTriangle:
    """An animated triangle of a template model, where it is in the bind pose."""

    index: int
    group: str
    centroid: Vec3
    texture_indices: list[int]


@dataclass
class TemplateModel:
    """One of a slot's two models, and how a bone's part goes into it."""

    model: fmt.ModelFile
    # For each bone, the model's joint that draws its part, and the bind-pose
    # point that is that joint's origin
    joints: dict[int, int]
    origins: dict[int, Vec3]
    # For each of the model's joints, the bone whose place it follows: moving
    # a bone moves the joints that follow it
    joint_bones: dict[int, int] = field(default_factory=dict)
    face_triangles: list[FaceTriangle] = field(default_factory=list)

    def face_groups(self) -> dict[str, list[int]]:
        """Return the texture indices that each group of face triangles shows."""
        groups: dict[str, set[int]] = {}
        for triangle in self.face_triangles:
            groups.setdefault(triangle.group, set()).update(triangle.texture_indices)
        return {name: sorted(indices) for name, indices in groups.items()}


@dataclass
class CharacterTemplate:
    """A slot's skeleton and its two models, which a custom character replaces."""

    slot: str
    bones: list[Bone]
    main: TemplateModel
    reaction: TemplateModel

    def bone_index(self, name: str) -> int:
        """Return the index of the bone named NAME."""
        for bone in self.bones:
            if bone.name == name:
                return bone.joint
        names = ", ".join(bone.name for bone in self.bones)
        msg = f"{self.slot}'s skeleton has no bone named {name!r}; it has {names}"
        raise TemplateError(msg)

    def models(self) -> dict[str, TemplateModel]:
        """Return the two models by kind."""
        return {"main": self.main, "reaction": self.reaction}


def _part_points(model: fmt.ModelFile, joint: int) -> FloatArray:
    """Return the positions of the vertices that a joint's part draws, in its space."""
    points = [
        vertex.position
        for address in model.joint_display_lists()[joint]
        for triangle in fmt.draw_triangles(model.data, address)
        for vertex in triangle.vertices
    ]
    return np.array(points, dtype=np.float64).reshape(-1, 3)  # pyright: ignore[reportUnknownMemberType]


def _bind_offsets(model: fmt.ModelFile) -> list[FloatArray]:
    """Each joint's bind-pose position, from the root's."""
    parents = model.joint_parents()
    offsets: list[FloatArray | None] = [None] * len(model.joints)

    def resolve(joint: int) -> FloatArray:
        known = offsets[joint]
        if known is not None:
            return known
        parent = parents[joint]
        if parent < 0:
            offset = np.zeros(3)
        else:
            translation = np.array(
                model.joints[joint].translations[0], dtype=np.float64
            )
            offset = resolve(parent) + translation
        offsets[joint] = offset
        return offset

    return [resolve(joint) for joint in range(len(model.joints))]


def _fit_translation(
    moving: FloatArray, target: FloatArray, start: FloatArray
) -> tuple[FloatArray, float]:
    """Fit MOVING onto TARGET by translation alone, from START.

    Returns the translation and the mean distance left from each moved point
    to its nearest target point.
    """
    # np.array copies: ndarray.copy returns typing.Self, which the type
    # checker's Python 3.10 lacks
    translation = np.array(start)
    distances = np.zeros(len(moving))
    for _ in range(FIT_ITERATIONS):
        moved = moving + translation
        gaps = moved[:, None, :] - target[None, :, :]
        squared = cast("FloatArray", np.einsum("ijk,ijk->ij", gaps, gaps))
        nearest = np.argmin(squared, axis=1)  # pyright: ignore[reportUnknownMemberType]
        steps = target[nearest] - moved
        distances = np.linalg.norm(steps, axis=1)  # pyright: ignore[reportUnknownMemberType]
        translation = translation + steps.mean(axis=0)
    return translation, float(distances.mean())


def _same_size(points: FloatArray, others: FloatArray) -> bool:
    """Return whether two parts' bounding boxes are the same size, nearly."""
    extent = np.ptp(points, axis=0)
    other_extent = np.ptp(others, axis=0)
    return np.allclose(extent, other_extent, atol=SAME_SIZE_TOLERANCE)  # pyright: ignore[reportUnknownMemberType]


def _match_reaction_joints(
    main: fmt.ModelFile, reaction: fmt.ModelFile, bind: list[FloatArray]
) -> dict[int, tuple[int, FloatArray]]:
    """Match each main joint with a part to the reaction joint its part fits.

    Returns, by main joint, the reaction joint and the translation from the
    main joint's space to it. The fit starts from the bind pose, with the
    root placed where the parts of matching size agree it is.
    """
    main_parts = {j: _part_points(main, j) for j in range(len(main.joints))}
    reaction_parts = {j: _part_points(reaction, j) for j in range(len(reaction.joints))}
    main_parts = {j: points for j, points in main_parts.items() if len(points)}
    reaction_parts = {j: points for j, points in reaction_parts.items() if len(points)}
    root_guesses = [
        target.mean(axis=0) - points.mean(axis=0) - bind[joint]
        for joint, points in main_parts.items()
        for target in reaction_parts.values()
        if len(target) == len(points) and _same_size(target, points)
    ]
    root = np.median(np.array(root_guesses), axis=0) if root_guesses else np.zeros(3)
    matches: dict[int, tuple[int, FloatArray]] = {}
    for joint, points in main_parts.items():
        size = float(np.linalg.norm(np.ptp(points, axis=0))) or 1.0  # pyright: ignore[reportUnknownMemberType]
        best: tuple[float, int, FloatArray] | None = None
        for reaction_joint, target in reaction_parts.items():
            translation, distance = _fit_translation(points, target, bind[joint] + root)
            if best is None or distance < best[0]:
                best = (distance, reaction_joint, translation)
        if best is not None and best[0] <= FIT_TOLERANCE * size:
            matches[joint] = (best[1], best[2])
    return matches


def _bone_names(parents: list[int]) -> list[str]:
    names = KNOWN_SKELETONS.get(tuple(parents))
    if names is not None:
        return list(names)
    return [f"joint_{joint:02d}" for joint in range(len(parents))]


def _frame_vertices(model: fmt.ModelFile, address: int) -> list[fmt.DrawnVertex]:
    """Return the three vertices of an animated triangle, in the file's frame data."""
    start = fmt.segment_offset(model.entry.frame_data_start)
    end = fmt.segment_offset(model.entry.frame_data_end)
    frame_data = model.data[start:end]
    return fmt.read_vertices(frame_data, address, fmt.TRIANGLE_VERTEX_COUNT)


def _triangle_joints(model: fmt.ModelFile) -> dict[int, int]:
    """Which joint's part loads each animated triangle, by its load list's address."""
    joints: dict[int, int] = {}
    for joint, addresses in enumerate(model.joint_display_lists()):
        for address in addresses:

            def note(called: int, joint: int = joint) -> None:
                _ = joints.setdefault(called, joint)

            _ = fmt.draw_triangles(model.data, address, on_call=note)
    return joints


def _group_face_triangles(model: fmt.ModelFile) -> list[set[int]]:
    """Group the animated triangles that show any of the same textures."""
    groups: list[set[int]] = []
    textures: list[set[int]] = []
    for index, triangle in enumerate(model.animated_triangles):
        shown = {triangle.texture_index} | {
            key.texture_index for key_set in triangle.key_sets for key in key_set.keys
        }
        joined = {index}
        for group_index in reversed(range(len(groups))):
            if textures[group_index] & shown:
                joined |= groups.pop(group_index)
                shown |= textures.pop(group_index)
        groups.append(joined)
        textures.append(shown)
    return groups


def _face_triangles(
    model: fmt.ModelFile, joint_bones: dict[int, int], origins: dict[int, Vec3]
) -> list[FaceTriangle]:
    """Place each animated triangle in the bind pose, and name its group."""
    triangle_joints = _triangle_joints(model)
    placed: dict[int, tuple[Vec3, list[int]]] = {}
    for index, triangle in enumerate(model.animated_triangles):
        joint = triangle_joints.get(triangle.load_texture_block)
        bone = joint_bones.get(joint, -1) if joint is not None else -1
        if bone < 0:
            name = model.entry.rom_name
            msg = f"no bone's part draws animated triangle {index} of {name}"
            raise TemplateError(msg)
        vertices = _frame_vertices(model, triangle.vertices)
        origin = origins[bone]
        centroid = tuple(
            sum(vertex.position[axis] for vertex in vertices)
            / fmt.TRIANGLE_VERTEX_COUNT
            + origin[axis]
            for axis in range(3)
        )
        shown = sorted(
            {triangle.texture_index}
            | {
                key.texture_index
                for key_set in triangle.key_sets
                for key in key_set.keys
            }
        )
        placed[index] = ((centroid[0], centroid[1], centroid[2]), shown)
    groups = _group_face_triangles(model)
    groups.sort(key=lambda group: -max(placed[index][0][1] for index in group))
    names = [EYES_GROUP, MOUTH_GROUP] + [f"face_{n}" for n in range(2, len(groups))]
    triangles: list[FaceTriangle] = []
    for name, group in zip(names, groups, strict=False):
        for index in sorted(group):
            centroid, shown = placed[index]
            triangles.append(FaceTriangle(index, name, centroid, shown))
    triangles.sort(key=lambda triangle: triangle.index)
    return triangles


def _as_vec3(values: FloatArray) -> Vec3:
    x, y, z = cast("list[float]", values.tolist())
    return (x, y, z)


def load_template(repository: Path, slot: str) -> CharacterTemplate:
    """Read the template of SLOT, such as "charlie", from the extracted files."""
    table = fmt.read_model_table(
        repository / "src" / "characters" / "character_model.c"
    )
    models_dir = repository / "assets" / "character_models"
    try:
        main_entry = fmt.find_entry(table, slot)
        reaction_entry = fmt.find_entry(table, slot + REACTION_SUFFIX)
    except fmt.ModelFormatError:
        slots = ", ".join(
            entry.rom_name
            for entry in table
            if not entry.is_patch and not entry.rom_name.endswith(REACTION_SUFFIX)
        )
        msg = f"{slot!r} isn't a character slot; the slots are {slots}"
        raise TemplateError(msg) from None
    if not (models_dir / f"{slot}.bin").exists():
        msg = f"{models_dir} has no {slot}.bin; run make setup to extract the game"
        raise TemplateError(msg)
    main = fmt.load_model(models_dir, main_entry)
    reaction = fmt.load_model(models_dir, reaction_entry)
    return build_template(slot, main, reaction)


def build_template(
    slot: str, main: fmt.ModelFile, reaction: fmt.ModelFile
) -> CharacterTemplate:
    """Work out the template from a slot's two decompressed models."""
    parents = main.joint_parents()
    bind = _bind_offsets(main)
    matches = _match_reaction_joints(main, reaction, bind)
    if not matches:
        msg = f"none of {slot}'s parts could be found in its reaction model"
        raise TemplateError(msg)
    # The root goes where the matched parts agree the bind pose stands
    root = np.median(
        np.array(
            [translation - bind[joint] for joint, (_, translation) in matches.items()]
        ),
        axis=0,
    )
    heads = [bind[joint] + root for joint in range(len(bind))]
    draws = {j for j in range(len(main.joints)) if len(_part_points(main, j))}
    names = _bone_names(parents)
    bones = [
        Bone(
            names[joint], joint, parents[joint], _as_vec3(heads[joint]), joint in draws
        )
        for joint in range(len(main.joints))
    ]
    main_origins = {joint: _as_vec3(heads[joint]) for joint in range(len(bones))}
    reaction_joints: dict[int, int] = {}
    reaction_origins: dict[int, Vec3] = {}
    for joint in range(len(bones)):
        # A bone without a match goes with its nearest matched ancestor's joint
        matched = joint
        while matched >= 0 and matched not in matches:
            matched = parents[matched]
        if matched < 0:
            matched = min(matches)
        reaction_joint, translation = matches[matched]
        reaction_joints[joint] = reaction_joint
        # A reaction point is a main joint's point moved by the translation
        reaction_origins[joint] = _as_vec3(heads[matched] - translation)
    main_bones = {joint: joint for joint in range(len(bones))}
    main_template = TemplateModel(main, main_bones, main_origins, main_bones)
    main_template.face_triangles = _face_triangles(main, main_bones, main_origins)
    reaction_bones = {
        joint: bone
        for bone, joint in sorted(reaction_joints.items())
        if bone in matches
    }
    reaction_template = TemplateModel(
        reaction,
        reaction_joints,
        reaction_origins,
        _follow_ancestors(reaction.joint_parents(), reaction_bones),
    )
    reaction_template.face_triangles = _face_triangles(
        reaction, reaction_bones, reaction_origins
    )
    return CharacterTemplate(slot, bones, main_template, reaction_template)


def _follow_ancestors(parents: list[int], bones: dict[int, int]) -> dict[int, int]:
    """Give each joint without a bone its nearest ancestor's, or the root bone's."""
    followed: dict[int, int] = {}
    for joint in range(len(parents)):
        ancestor = joint
        while ancestor >= 0 and ancestor not in bones:
            ancestor = parents[ancestor]
        followed[joint] = bones[ancestor] if ancestor >= 0 else ROOT_BONE
    return followed


def bone_tail(template: CharacterTemplate, bone: Bone) -> Vec3:
    """Where to draw a bone's tail: toward its child, or on along its parent's line."""
    children = [other for other in template.bones if other.parent == bone.joint]
    head = np.array(bone.head)
    if children:
        tail = np.mean([np.array(child.head) for child in children], axis=0)
    elif bone.parent >= 0:
        parent_head = np.array(template.bones[bone.parent].head)
        direction = head - parent_head
        tail = head + direction * 0.5
    else:
        tail = head + np.array([0.0, 100.0, 0.0])
    if math.dist(_as_vec3(tail), _as_vec3(head)) < 1.0:
        tail = head + np.array([0.0, 50.0, 0.0])
    return _as_vec3(tail)


def _reference_mesh(
    template: CharacterTemplate,
) -> dict[str, dict[str, list[object]]]:
    """Return the main model's triangles in the bind pose, by bone, to model beside."""
    model = template.main.model
    parts: dict[str, dict[str, list[object]]] = {}
    for bone in template.bones:
        origin = template.main.origins[bone.joint]
        for address in model.joint_display_lists()[bone.joint]:
            for triangle in fmt.draw_triangles(model.data, address):
                part = parts.setdefault(bone.name, {"positions": [], "colors": []})
                part["positions"].append(
                    [
                        [
                            round(vertex.position[axis] + origin[axis], 1)
                            for axis in range(3)
                        ]
                        for vertex in triangle.vertices
                    ]
                )
                part["colors"].append(list(triangle.prim_color))
    return parts


def texture_image(model: fmt.ModelFile, texture: fmt.AnimatedTexture) -> Image.Image:
    """Return an animated texture of a model, as an image."""
    start = fmt.segment_offset(texture.address)
    size = texture.width * texture.height
    texels = np.frombuffer(model.data, ">u2", size, start).astype(np.uint16)  # pyright: ignore[reportUnknownMemberType]
    channels = [(texels >> shift & RGBA5551_CHANNEL_MASK) << 3 for shift in (11, 6, 1)]
    alpha = np.where(texels & 1, ALPHA_OPAQUE, 0)  # pyright: ignore[reportUnknownMemberType]
    pixels = np.stack([*channels, alpha], axis=-1).astype(np.uint8)  # pyright: ignore[reportUnknownMemberType]
    shaped = pixels.reshape(texture.height, texture.width, 4)  # pyright: ignore[reportUnknownMemberType]
    return Image.fromarray(shaped, "RGBA")


def write_face_textures(
    template: CharacterTemplate, directory: Path
) -> dict[str, dict[int, str]]:
    """Write the textures that each model's face shows, so modelers can see them.

    Returns their file names by model kind and texture index.
    """
    directory.mkdir(parents=True, exist_ok=True)
    names: dict[str, dict[int, str]] = {}
    for kind, model in template.models().items():
        names[kind] = {}
        for indices in model.face_groups().values():
            for index in indices:
                name = f"{template.slot}_{kind}_{index}.png"
                texture_image(model.model, model.model.textures[index]).save(
                    directory / name
                )
                names[kind][index] = f"{directory.name}/{name}"
    return names


def template_document(
    template: CharacterTemplate, textures: dict[str, dict[int, str]]
) -> dict[str, object]:
    """Return the template as the Blender add-on reads it, in model units, Y up."""
    main = template.main.model
    points: list[dict[str, object]] = []
    for point in main.points:
        joint = point.joint_path[-1]
        head = template.main.origins[joint]
        points.append(
            {
                "id": point.point_id,
                "bone": template.bones[joint].name,
                "position": [
                    round(head[axis] + point.offset[axis], 2) for axis in range(3)
                ],
            }
        )
    return {
        "format": TEMPLATE_FORMAT,
        "version": TEMPLATE_VERSION,
        "slot": template.slot,
        "bones": [
            {
                "name": bone.name,
                "parent": template.bones[bone.parent].name
                if bone.parent >= 0
                else None,
                "head": [round(value, 2) for value in bone.head],
                "tail": [round(value, 2) for value in bone_tail(template, bone)],
                "draws": bone.draws,
            }
            for bone in template.bones
        ],
        "points": points,
        "faces": {
            kind: {
                "groups": model.face_groups(),
                "textures": textures[kind],
                "triangles": [
                    {
                        "group": triangle.group,
                        "centroid": [round(value, 1) for value in triangle.centroid],
                    }
                    for triangle in model.face_triangles
                ],
            }
            for kind, model in template.models().items()
        },
        "reference": _reference_mesh(template),
    }


@final
class Arguments(argparse.Namespace):
    """The command line's arguments."""

    repository: Path = Path()
    slot: str = ""
    output: Path = Path()


def main() -> None:
    """Write the template of the slot that the arguments name."""
    parser = argparse.ArgumentParser(description=SUMMARY)
    _ = parser.add_argument(
        "--repository", type=Path, default=Path(), help="the decompilation's root"
    )
    _ = parser.add_argument("slot", help='the character slot, such as "charlie"')
    _ = parser.add_argument("output", type=Path, help="the template file to write")
    arguments = parser.parse_args(namespace=Arguments())
    try:
        template = load_template(arguments.repository, arguments.slot)
    except (TemplateError, fmt.ModelFormatError, OSError) as error:
        sys.exit(f"character_template.py: {error}")
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    textures = write_face_textures(
        template, arguments.output.with_name(arguments.output.stem + TEXTURES_SUFFIX)
    )
    document = template_document(template, textures)
    _ = arguments.output.write_text(json.dumps(document, indent=1) + "\n")


if __name__ == "__main__":
    main()
