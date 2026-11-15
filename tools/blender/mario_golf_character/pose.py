"""Pose a fitted rig as its template stands, and read the pose for the export.

The game starts its animations from the template's pose, so the export
takes each part as the rig's pose shows it: what the artist sees lined
up with the template's wireframe guide is what the game starts from.

Match Template Pose gives a start. Only the joints count, the bones'
heads, which are what fitting moves to the model's: each bone turns so
its children's joints lie, as nearly as they can, the way the template's
do from its own, the least turn that does it. A bone at the end of a
chain, such as a foot, a hand or the head, has no child joints to say
which way it points, and a bone's tail says nothing the artist meant, so
these turn with the bone they hang from. Whatever doesn't then line up,
as feet often don't, the artist turns in Pose Mode.
"""

import math
from dataclasses import dataclass

import bpy
import numpy as np
from mathutils import Matrix, Vector

from . import rig as rigs

# Joint offsets shorter than this, in meters, give no direction: such as
# the joints that sit on their parent's
JOINT_OFFSET_MIN = 1e-4
# Joints this near to a line, as a fraction of their spread along it, are
# taken as on it: they leave a twist about it open, so the least turn is
# taken, which Blender works out exactly
IN_LINE = 1e-3
# A pose bone this near the identity hasn't been posed
UNPOSED_TOLERANCE = 1e-6


def _fitting_turn(fitted: list[Vector], wanted: list[Vector]) -> Matrix:
    """Find the turn that best takes the FITTED directions onto the WANTED.

    Joints in a line leave a twist about it open; then the turn is the least
    that takes the longest offset onto its wanted direction.
    """
    correlation = np.zeros((3, 3))
    for start, end in zip(fitted, wanted, strict=True):
        correlation += np.outer(
            np.array(start.normalized()), np.array(end.normalized())
        )
    u, spread, vt = np.linalg.svd(correlation)
    if spread[1] <= IN_LINE * spread[0]:
        longest = max(range(len(fitted)), key=lambda i: fitted[i].length)
        return fitted[longest].rotation_difference(wanted[longest]).to_matrix()
    # A reflection fits no better than the turn beside it
    handedness = np.sign(np.linalg.det(vt.T @ u.T)) or 1.0
    turn = vt.T @ np.diag([1.0, 1.0, handedness]) @ u.T
    return Matrix(turn.tolist())


def template_turns(rig: bpy.types.Object, template: dict) -> dict[str, Matrix]:
    """How Match Template Pose turns each bone, in the rig's space, from its rest."""
    bones = rig.data.bones
    turns: dict[str, Matrix] = {}
    for bone in template["bones"]:
        name = bone["name"]
        head = bones[name].head_local
        template_head = rigs.to_blender(bone["head"])
        fitted: list[Vector] = []
        wanted: list[Vector] = []
        for child in template["bones"]:
            if child["parent"] != name:
                continue
            offset = bones[child["name"]].head_local - head
            template_offset = rigs.to_blender(child["head"]) - template_head
            if min(offset.length, template_offset.length) > JOINT_OFFSET_MIN:
                fitted.append(offset)
                wanted.append(template_offset)
        if fitted:
            turns[name] = _fitting_turn(fitted, wanted)
        elif bone["parent"] is not None:
            turns[name] = turns[bone["parent"]].copy()
        else:
            turns[name] = Matrix.Identity(3)
    return turns


def match_template_pose(rig: bpy.types.Object, template: dict) -> None:
    """Pose RIG so that its model stands as the template does."""
    turns = template_turns(rig, template)
    bones = rig.data.bones
    moves: dict[str, Matrix] = {}
    for bone in template["bones"]:
        name = bone["name"]
        head = bones[name].head_local
        parent = bone["parent"]
        # A joint goes where its parent's move takes it, then turns about itself
        moved_head = moves[parent] @ head if parent is not None else head
        moves[name] = (
            Matrix.Translation(moved_head)
            @ turns[name].to_4x4()
            @ Matrix.Translation(-head)
        )
        # Blender works out a bone's pose from its parent's, so parents go first
        rig.pose.bones[name].matrix = moves[name] @ bones[name].matrix_local
        bpy.context.view_layer.update()


def is_unposed(rig: bpy.types.Object) -> bool:
    """Whether every bone of RIG is in its rest pose."""
    identity = Matrix.Identity(4)
    return all(
        all(
            math.isclose(a, b, abs_tol=UNPOSED_TOLERANCE)
            for row, identity_row in zip(bone.matrix_basis, identity, strict=True)
            for a, b in zip(row, identity_row, strict=True)
        )
        for bone in rig.pose.bones
    )


def turn_degrees(turn: Matrix) -> float:
    """How far a turn turns, in degrees."""
    return math.degrees(turn.to_quaternion().angle)


@dataclass
class GamePose:
    """Where the rig's pose takes each bone's faces, and its joints."""

    moves: dict[str, Matrix]
    heads: dict[str, Vector]

    def place(self, bone: str, point: Vector) -> Vector:
        """Where a point on BONE's faces goes in the pose."""
        return self.moves[bone] @ point

    def turn_normal(self, bone: str, normal: Vector) -> Vector:
        """Which way a normal of BONE's faces points in the pose."""
        return (
            self.moves[bone].to_3x3().inverted_safe().transposed() @ normal
        ).normalized()


def game_pose(rig: bpy.types.Object) -> GamePose:
    """Read the pose the rig shows, which must be evaluated in Pose Position."""
    moves = {}
    heads = {}
    for pose_bone in rig.pose.bones:
        moves[pose_bone.name] = (
            pose_bone.matrix @ pose_bone.bone.matrix_local.inverted_safe()
        )
        heads[pose_bone.name] = pose_bone.head.copy()
    return GamePose(moves, heads)
