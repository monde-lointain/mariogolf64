"""Bind a character's meshes to its template rig, one bone per face.

The game moves every triangle of a character rigidly with one joint: no
model blends joints, and no triangle has corners on two. So a face here
belongs to the one bone whose vertex group holds all its corners, at weight
1. The artist assigns a part's faces to its bone's group as usual, by
selecting them and pressing Assign; the vertices on the seam between two
parts are then in both groups, and each face still has one bone. An object
parented to a bone, with no bone groups, moves wholly with that bone.
"""

from dataclasses import dataclass, field

import bpy
from mathutils import Vector

from .problems import Report, agree, count

# A vertex is in a group when its weight is above this
WEIGHT_PRESENT = 0.001
# A weight below this is partial, as automatic or painted weights make
WEIGHT_FULL = 0.999
ARMATURE_MODIFIER_NAME = "Armature"


def part_bones(template: dict) -> list[str]:
    """Return the bones that carry parts of the template's own character, in order."""
    return [bone["name"] for bone in template["bones"] if bone["draws"]]


def parent_bone(rig: bpy.types.Object, mesh_object: bpy.types.Object) -> str | None:
    """Return the rig bone that an object is parented to, if it is."""
    if mesh_object.parent == rig and mesh_object.parent_type == "BONE":
        return mesh_object.parent_bone or None
    return None


@dataclass
class FaceBones:
    """The bone that each face of a mesh moves with, and the faces with none."""

    bones: list[str | None] = field(default_factory=list)
    # Faces with a corner in no bone's group
    ungrouped: list[int] = field(default_factory=list)
    # Faces whose corners are all grouped, but in no one group together
    spanning: list[int] = field(default_factory=list)
    # Faces whose corners are all in two or more of the same groups
    doubled: list[int] = field(default_factory=list)
    # Vertices with a weight between 0 and 1
    partial: list[int] = field(default_factory=list)
    # Vertices in two or more bones' groups: the seams between parts
    seam_vertices: int = 0
    # Whether the mesh has any vertex group named after a template bone
    has_bone_groups: bool = False

    @property
    def is_bound(self) -> bool:
        """Whether every face has one bone and no weight is partial."""
        return not (self.ungrouped or self.spanning or self.doubled or self.partial)


def face_bones(
    mesh_object: bpy.types.Object,
    mesh: bpy.types.Mesh,
    template: dict,
    fallback: str | None,
) -> FaceBones:
    """Work out each face's bone from the vertex groups named after template bones.

    MESH is the object's own mesh or its evaluated one; FALLBACK is the bone
    for faces with no grouped corner at all, as for an object parented to a
    bone.
    """
    bone_names = {bone["name"] for bone in template["bones"]}
    names = {
        group.index: group.name
        for group in mesh_object.vertex_groups
        if group.name in bone_names
    }
    result = FaceBones(has_bone_groups=bool(names))
    vertex_groups: list[frozenset[str]] = []
    for vertex in mesh.vertices:
        held = [
            (names[element.group], element.weight)
            for element in vertex.groups
            if element.group in names and element.weight > WEIGHT_PRESENT
        ]
        if any(weight < WEIGHT_FULL for _, weight in held):
            result.partial.append(vertex.index)
        result.seam_vertices += len(held) > 1
        vertex_groups.append(frozenset(name for name, _ in held))
    for polygon in mesh.polygons:
        corners = [vertex_groups[vertex] for vertex in polygon.vertices]
        shared = frozenset.intersection(*corners)
        bone: str | None = None
        if len(shared) == 1:
            bone = next(iter(shared))
        elif shared:
            result.doubled.append(polygon.index)
        elif not any(corners) and fallback is not None:
            bone = fallback
        elif all(corners):
            result.spanning.append(polygon.index)
        else:
            result.ungrouped.append(polygon.index)
        result.bones.append(bone)
    return result


def face_bone_problems(
    mesh_object: bpy.types.Object, found: FaceBones, report: Report
) -> None:
    """Report why some of a mesh's faces have no one bone, with them selectable."""
    name = mesh_object.name
    if found.ungrouped:
        advice = (
            "Select them in Edit Mode, pick their bone's group under Object Data > "
            "Vertex Groups and press Assign"
            if found.has_bone_groups
            else (
                "It has no vertex groups named after the rig's bones; select it and "
                "use Add Bone Groups"
            )
        )
        is_not = agree(len(found.ungrouped), "isn't", "aren't")
        report.error(
            name,
            f"{count(len(found.ungrouped), 'face')} {is_not} in any bone's group, so "
            f"nothing moves them. {advice}",
            object_name=name,
            faces=tuple(found.ungrouped),
        )
    if found.spanning:
        report.error(
            name,
            f"{count(len(found.spanning), 'face')} "
            f"{agree(len(found.spanning), 'spans', 'span')} two bones: no one bone's "
            "group holds all its corners. The game moves each face with one bone, so "
            "assign each of these faces to one bone's group",
            object_name=name,
            faces=tuple(found.spanning),
        )
    if found.doubled:
        report.error(
            name,
            f"{count(len(found.doubled), 'face')} "
            f"{agree(len(found.doubled), 'is', 'are')} wholly in two bones' groups, so "
            "which bone moves it is unclear. Remove it from all but one",
            object_name=name,
            faces=tuple(found.doubled),
        )
    if found.partial:
        report.error(
            name,
            f"{count(len(found.partial), 'vertex', 'vertices')} "
            f"{agree(len(found.partial), 'has', 'have')} a bone weight between 0 and "
            "1, as automatic or painted weights give. The game can't blend bones; "
            "select them and Assign them to one bone's group with Weight 1",
            object_name=name,
            vertices=tuple(found.partial),
        )


def add_bone_groups(
    rig: bpy.types.Object, mesh_object: bpy.types.Object, template: dict, *, fill: bool
) -> int:
    """Give a mesh a vertex group for each part-carrying bone.

    With FILL, each face with no corner in a bone's group yet goes into the
    group of the bone nearest its center, as a start for the artist to
    correct; faces already assigned are left alone. Returns how many faces
    were filled. Bones with no part in the template are joints between
    parts; they get no group here, but a group named after one works too.
    """
    names = part_bones(template)
    groups = {
        name: mesh_object.vertex_groups.get(name)
        or mesh_object.vertex_groups.new(name=name)
        for name in names
    }
    if not fill:
        return 0
    found = face_bones(
        mesh_object, mesh_object.data, template, parent_bone(rig, mesh_object)
    )
    to_rig = rig.matrix_world.inverted() @ mesh_object.matrix_world
    bones = [
        (name, rig.data.bones[name].head_local, rig.data.bones[name].tail_local)
        for name in names
    ]
    mesh = mesh_object.data
    filled = 0
    for index in found.ungrouped:
        polygon = mesh.polygons[index]
        if any(_bone_groups_of(mesh.vertices[v], groups) for v in polygon.vertices):
            continue  # Partly assigned by the artist, so theirs to finish
        center = to_rig @ polygon.center
        nearest = min(
            bones, key=lambda bone: _segment_distance(center, bone[1], bone[2])
        )[0]
        groups[nearest].add(list(polygon.vertices), 1.0, "REPLACE")
        filled += 1
    return filled


def _bone_groups_of(
    vertex: bpy.types.MeshVertex, groups: dict[str, bpy.types.VertexGroup]
) -> bool:
    indices = {group.index for group in groups.values()}
    return any(e.group in indices and e.weight > WEIGHT_PRESENT for e in vertex.groups)


def bind(
    rig: bpy.types.Object, mesh_object: bpy.types.Object, template: dict
) -> Report:
    """Parent a mesh to the rig by an Armature modifier, once each face has one bone."""
    report = Report()
    found = face_bones(
        mesh_object, mesh_object.data, template, parent_bone(rig, mesh_object)
    )
    face_bone_problems(mesh_object, found, report)
    _check_mirror(mesh_object, report)
    if report.errors:
        return report
    if parent_bone(rig, mesh_object) is None:
        world = mesh_object.matrix_world.copy()
        mesh_object.parent = rig
        mesh_object.parent_type = "OBJECT"
        mesh_object.matrix_world = world
    armatures = [m for m in mesh_object.modifiers if m.type == "ARMATURE"]
    modifier = (
        armatures[0]
        if armatures
        else mesh_object.modifiers.new(ARMATURE_MODIFIER_NAME, "ARMATURE")
    )
    modifier.object = rig
    modifier.use_vertex_groups = True
    modifier.use_bone_envelopes = False
    # Mirrors and the like shape the model at rest, so the rig moves it last
    mesh_object.modifiers.move(
        list(mesh_object.modifiers).index(modifier), len(mesh_object.modifiers) - 1
    )
    if found.seam_vertices:
        report.warn(
            mesh_object.name,
            f"{count(found.seam_vertices, 'vertex', 'vertices')} on seams between "
            f"parts {agree(found.seam_vertices, 'bends', 'bend')} when you pose the "
            "rig, but in the game each part moves on its own, so seams can open. "
            "Overlap parts at joints to hide this, as the game's characters do",
        )
    return report


def _check_mirror(mesh_object: bpy.types.Object, report: Report) -> None:
    """Require Mirror modifiers to swap .L and .R groups, so each side has its bones."""
    for modifier in mesh_object.modifiers:
        if modifier.type == "MIRROR" and not modifier.use_mirror_vertex_groups:
            report.error(
                mesh_object.name,
                f"its Mirror modifier {modifier.name} doesn't mirror vertex groups, so "
                "the mirrored side would move with the wrong bones. Turn on its Vertex "
                "Groups option, under Data",
            )


def _segment_distance(point: Vector, head: Vector, tail: Vector) -> float:
    """How far a point is from the bone that runs from HEAD to TAIL."""
    axis = tail - head
    length_squared = axis.length_squared
    if length_squared == 0:
        return (point - head).length
    along = min(max((point - head).dot(axis) / length_squared, 0.0), 1.0)
    return (point - (head + axis * along)).length
