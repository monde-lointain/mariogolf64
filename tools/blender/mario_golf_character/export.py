"""Export a character modeled on a template rig as a character package.

Each face moves with one bone, as binding.py works out. The rig may have
been fitted to the model in Edit Mode, its bones moved and turned to where
the model's joints are, such as for a T-pose. The game's animations turn
each joint from the template's rest pose, so the export turns each bone's
faces from the fitted bone onto the template's, about the bone's head, and
keeps the fitted bones' lengths: the character keeps its proportions and
takes the template's pose.

Only a bone's direction counts, not its roll: each part turns the shortest
way from its fitted bone's direction to the template's. Roll is unreliable
for this, as moving a bone's tail keeps its roll number while its twist
changes, so a part is never twisted about its bone by the export.

The checks here are the barricade between what an artist made and the
package: anything the game can't show is reported as a Problem, in Blender's
terms, and nothing is written. The package builder checks again, for
packages made by hand, and its sizes and texture limits are its own.
"""

import json
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector

from . import binding
from . import rig as rigs
from .problems import Report, agree, count

PACKAGE_FORMAT = "mariogolf64-character"
PACKAGE_VERSION = 1
PACKAGE_FILE = "character.json"
TEXTURE_DIRECTORY = "textures"
MODEL_KINDS = ("main", "reaction")
CHANNEL_MAX = 255
RGBA_CHANNELS = 4
# Material roles
ROLE_COLOR = "COLOR"
ROLE_TEXTURE = "TEXTURE"
ROLE_FACE = "FACE"
POSITION_DIGITS = 2
NORMAL_DIGITS = 4
UV_DIGITS = 5
SRGB_LINEAR_LIMIT = 0.0031308
SRGB_LINEAR_SCALE = 12.92
SRGB_GAMMA = 2.4
SRGB_SCALE = 1.055
SRGB_OFFSET = 0.055
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
# 8 bits a channel, RGBA, deflate, adaptive filtering, not interlaced
PNG_RGBA_8 = (8, 6, 0, 0, 0)
PNG_NO_FILTER = b"\x00"


def linear_to_srgb(value: float) -> int:
    """Return a linear color channel as an 8-bit sRGB one, as Blender displays it."""
    value = min(max(value, 0.0), 1.0)
    if value <= SRGB_LINEAR_LIMIT:
        encoded = value * SRGB_LINEAR_SCALE
    else:
        encoded = SRGB_SCALE * value ** (1 / SRGB_GAMMA) - SRGB_OFFSET
    return round(encoded * CHANNEL_MAX)


def color_bytes(color: tuple[float, ...] | list[float]) -> list[int]:
    """Return a linear RGBA color as 8-bit sRGB with 8-bit alpha."""
    red, green, blue, alpha = color
    return [
        linear_to_srgb(red),
        linear_to_srgb(green),
        linear_to_srgb(blue),
        round(min(max(alpha, 0.0), 1.0) * CHANNEL_MAX),
    ]


def image_png(image: bpy.types.Image) -> bytes | None:
    """Return an image's pixels as an 8-bit RGBA PNG, or None when it has no pixels.

    The pixels are taken as Blender stores them, so an sRGB image keeps its
    colors and a painted one needn't be saved first.
    """
    width, height = image.size
    if width == 0 or height == 0:
        return None
    pixels = np.empty(width * height * image.channels, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    pixels = pixels.reshape(height, width, image.channels)
    if image.channels < RGBA_CHANNELS:
        opaque = np.ones(
            (height, width, RGBA_CHANNELS - image.channels), dtype=np.float32
        )
        pixels = np.concatenate([pixels, opaque], axis=2)
    # Blender's rows run bottom to top; a PNG's run top to bottom
    rows = np.round(np.clip(pixels[::-1], 0.0, 1.0) * CHANNEL_MAX).astype(np.uint8)
    raw = b"".join(PNG_NO_FILTER + row.tobytes() for row in rows)

    def chunk(kind: bytes, data: bytes) -> bytes:
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    header = struct.pack(">IIBBBBB", width, height, *PNG_RGBA_8)
    return (
        PNG_SIGNATURE
        + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(raw))
        + chunk(b"IEND", b"")
    )


def _linked_node(socket: bpy.types.NodeSocket) -> bpy.types.Node | None:
    return socket.links[0].from_node if socket.is_linked else None


def _surface(material: bpy.types.Material) -> bpy.types.Node | None:
    """Return the shader that the material's active output draws."""
    if material.node_tree is None:
        return None
    outputs = [n for n in material.node_tree.nodes if n.type == "OUTPUT_MATERIAL"]
    output = next(
        (n for n in outputs if n.is_active_output), outputs[0] if outputs else None
    )
    return _linked_node(output.inputs["Surface"]) if output is not None else None


def _alpha_mix(
    surface: bpy.types.Node | None,
) -> tuple[bpy.types.Node, bpy.types.Node | None] | None:
    """Return an image's node and the shader it's drawn over by its alpha, if any.

    This is how painters often show a decal, such as eyes, over a color: a
    Mix Shader whose Factor is an Image Texture's Alpha, with the color's
    shader in its first slot, where the image is clear.
    """
    if surface is None or surface.type != "MIX_SHADER":
        return None
    factor = surface.inputs[0]
    if not factor.is_linked or factor.links[0].from_socket.name != "Alpha":
        return None
    image_node = factor.links[0].from_node
    if image_node.type != "TEX_IMAGE":
        return None
    return image_node, _linked_node(surface.inputs[1])


def _principled(material: bpy.types.Material) -> bpy.types.Node | None:
    """Return the Principled BSDF whose color the material shows, if it shows one."""
    surface = _surface(material)
    mix = _alpha_mix(surface)
    if mix is not None:
        surface = mix[1]
    if surface is not None and surface.type == "BSDF_PRINCIPLED":
        return surface
    if material.node_tree is None:
        return None
    return next(
        (n for n in material.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None
    )


def base_color(material: bpy.types.Material) -> tuple[float, ...]:
    """Return the color a material shows.

    That's its Principled BSDF's Base Color, else its viewport color. For an
    image mixed over a color by its alpha, it's that color.
    """
    node = _principled(material)
    if node is not None and not node.inputs["Base Color"].is_linked:
        return tuple(node.inputs["Base Color"].default_value)
    return tuple(material.diffuse_color)


def base_image(material: bpy.types.Material) -> bpy.types.Image | None:
    """Return the image a material shows: one mixed over a color, else Base Color's."""
    mix = _alpha_mix(_surface(material))
    if mix is not None:
        return mix[0].image
    node = _principled(material)
    if node is None or not node.inputs["Base Color"].is_linked:
        return None
    source = node.inputs["Base Color"].links[0].from_node
    return source.image if source.type == "TEX_IMAGE" else None


@dataclass
class Unposing:
    """How each bone's faces turn from the fitted rig onto the template's pose."""

    rotations: dict[str, Matrix]
    fitted_heads: dict[str, Vector]
    heads: dict[str, Vector]

    def place(self, bone: str, point: Vector) -> Vector:
        """Where a point on BONE's faces goes in the template's pose."""
        return self.heads[bone] + self.rotations[bone] @ (
            point - self.fitted_heads[bone]
        )


def unposing(rig: bpy.types.Object, template: dict) -> Unposing:
    """Work out how the fitted rig's bones turn onto the template's."""
    bones = rig.data.bones
    rotations: dict[str, Matrix] = {}
    fitted: dict[str, Vector] = {}
    heads: dict[str, Vector] = {}
    for bone in template["bones"]:
        name = bone["name"]
        fitted_bone = bones[name]
        fitted_direction = fitted_bone.tail_local - fitted_bone.head_local
        template_direction = rigs.to_blender(bone["tail"]) - rigs.to_blender(
            bone["head"]
        )
        rotations[name] = fitted_direction.rotation_difference(
            template_direction
        ).to_matrix()
        fitted[name] = fitted_bone.head_local.copy()
        parent = bone["parent"]
        if parent is None:
            heads[name] = fitted[name]
        else:
            # The fitted offset from the parent, turned as the parent turns,
            # keeps the bone's length and takes the template's direction
            heads[name] = heads[parent] + rotations[parent] @ (
                fitted[name] - fitted[parent]
            )
    return Unposing(rotations, fitted, heads)


def check_rig(rig: bpy.types.Object, template: dict, report: Report) -> None:
    """Report the ways the rig's bones no longer match its template's."""
    names = {bone["name"] for bone in template["bones"]}
    rig_names = {bone.name for bone in rig.data.bones}
    for name in sorted(names - rig_names):
        report.error(
            rig.name,
            f"the bone {name} is missing, and the game's animations need it. Undo its "
            "deletion, or add the template rig again and refit it",
        )
    for name in sorted(rig_names - names):
        report.error(
            rig.name,
            f"the bone {name} isn't in the template, so the game can't move it. Delete "
            "it, or rename it back if it was renamed",
        )
    for bone in template["bones"]:
        if bone["name"] not in rig_names:
            continue
        parent = rig.data.bones[bone["name"]].parent
        if (parent.name if parent else None) != bone["parent"]:
            report.error(
                rig.name,
                f"the bone {bone['name']} should be a child of {bone['parent']}, as in "
                "the game. Set its Parent under Bone > Relations",
            )


def bound_meshes(rig: bpy.types.Object) -> list[bpy.types.Object]:
    """Return the meshes parented to the rig, which make up the character."""
    reference = rig.name + rigs.REFERENCE_SUFFIX
    return [
        child
        for child in rig.children_recursive
        if child.type == "MESH" and child.name != reference
    ]


@dataclass
class _MaterialEntry:
    index: int
    document: dict


class PackageWriter:
    """Gathers a rig's meshes and materials into a character package."""

    def __init__(self, rig: bpy.types.Object, directory: Path, name: str) -> None:
        """Prepare to write the character NAME, rigged by RIG, to DIRECTORY."""
        self.rig = rig
        self.directory = directory
        self.name = name
        _, self.template = rigs.rig_template(rig)
        self.report = Report()
        self.materials: dict[str, _MaterialEntry] = {}
        self.parts: dict[tuple[str, int], dict] = {}
        # Image name to its PNG's bytes and its path in the package
        self.images: dict[str, tuple[bytes, str]] = {}

    def _image_file(self, image: bpy.types.Image, user: str) -> str | None:
        """Return the package path of an image, which is written with the package."""
        if image.name not in self.images:
            png = image_png(image)
            if png is None:
                self.report.error(
                    image.name,
                    f"the image, used by {user}, has no pixels; its file may be "
                    "missing. Reload or replace it in the Image Editor",
                )
                return None
            file_name = f"{TEXTURE_DIRECTORY}/{bpy.path.clean_name(image.name)}.png"
            self.images[image.name] = (png, file_name)
        return self.images[image.name][1]

    def _material(self, material: bpy.types.Material) -> int:
        """Return the package index of a material, adding it the first time."""
        if material.name in self.materials:
            return self.materials[material.name].index
        settings = material.mg64
        color = base_color(material)
        document: dict = {
            "name": material.name,
            "color": color_bytes(color),
            "double_sided": not material.use_backface_culling,
        }
        if color[3] < 1.0 and settings.role == ROLE_COLOR:
            self.report.warn(
                material.name,
                "its color's alpha is ignored: the game draws colors solid. For holes, "
                "use a texture whose alpha is 0 there",
            )
        if settings.use_palettes:
            document["palettes"] = [
                color_bytes(palette) for palette in settings.palette_colors()
            ]
        image = base_image(material)
        if settings.role == ROLE_TEXTURE and image is None:
            self.report.error(
                material.name,
                "it's drawn as Texture, but no Image Texture feeds its Base Color. "
                "Connect one, or draw it as Color",
            )
        if settings.role in {ROLE_TEXTURE, ROLE_FACE} and image is not None:
            path = self._image_file(image, material.name)
            if path is not None:
                document["texture"] = path
        if settings.role == ROLE_FACE:
            document["face"] = self._face(material, has_still_image=image is not None)
        entry = _MaterialEntry(len(self.materials), document)
        self.materials[material.name] = entry
        return entry.index

    def _face(self, material: bpy.types.Material, *, has_still_image: bool) -> dict:
        settings = material.mg64
        group = settings.face_group
        slot = self.template["slot"]
        face: dict = {"group": group}
        missing: list[str] = []
        not_animated: list[str] = []
        for kind in MODEL_KINDS:
            face[kind] = {}
            indices = self.template["faces"][kind]["groups"].get(group)
            if indices is None:
                not_animated.append(kind)
                continue
            for index in indices:
                image = settings.face_image(kind, index)
                if image is None:
                    missing.append(f"{kind.capitalize()} {index}")
                    continue
                path = self._image_file(image, material.name)
                if path is not None:
                    face[kind][str(index)] = path
        if len(not_animated) == len(MODEL_KINDS):
            self.report.error(
                material.name,
                f"its Face Part is {group}, which {slot}'s models don't animate. Pick "
                "another Face Part, or draw it as Texture",
            )
        elif not_animated and not has_still_image:
            self.report.error(
                material.name,
                f"{slot}'s {not_animated[0]} model doesn't animate the {group}, so "
                "there it shows a still image. Connect an Image Texture to its Base "
                "Color for that",
            )
        if missing:
            self.report.error(
                material.name,
                "it needs an image in place of each of the template's "
                f"{', '.join(missing)}. Pick them under Material > Mario Golf",
            )
        return face

    def _check_mesh(self, mesh_object: bpy.types.Object) -> bool:
        """Report what in a mesh, as modeled, the game can't show; True when nothing."""
        report = Report()
        found = binding.face_bones(
            mesh_object,
            mesh_object.data,
            self.template,
            binding.parent_bone(self.rig, mesh_object),
        )
        binding.face_bone_problems(mesh_object, found, report)
        slots = mesh_object.material_slots
        no_material = tuple(
            polygon.index
            for polygon in mesh_object.data.polygons
            if polygon.material_index >= len(slots)
            or slots[polygon.material_index].material is None
        )
        if no_material:
            report.error(
                mesh_object.name,
                f"{count(len(no_material), 'face')} "
                f"{agree(len(no_material), 'has', 'have')} no material. Select them "
                "and assign one in the Material tab",
                object_name=mesh_object.name,
                faces=no_material,
            )
        used = sorted(
            {slot.material.name for slot in slots if slot.material}, key=str.casefold
        )
        textured = [
            name
            for name in used
            if bpy.data.materials[name].mg64.role in {ROLE_TEXTURE, ROLE_FACE}
        ]
        if textured and not mesh_object.data.uv_layers:
            report.error(
                mesh_object.name,
                f"it has no UV map, but {', '.join(textured)} "
                f"{agree(len(textured), 'draws', 'draw')} images on it. Unwrap it (U "
                "in Edit Mode)",
            )
        # The materials are checked now too, so the artist sees every problem at once
        for name in used:
            self._material(bpy.data.materials[name])
        self.report.extend(report)
        return not report.errors

    def _add_mesh(self, mesh_object: bpy.types.Object, unpose: Unposing) -> None:
        if not self._check_mesh(mesh_object):
            return
        depsgraph = bpy.context.evaluated_depsgraph_get()
        evaluated = mesh_object.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        try:
            self._add_faces(mesh_object, mesh, unpose)
        finally:
            evaluated.to_mesh_clear()

    def _add_faces(
        self, mesh_object: bpy.types.Object, mesh: bpy.types.Mesh, unpose: Unposing
    ) -> None:
        """Add an evaluated mesh's triangles to their bones' and materials' parts."""
        found = binding.face_bones(
            mesh_object, mesh, self.template, binding.parent_bone(self.rig, mesh_object)
        )
        if not found.is_bound:
            # The mesh as modeled passed, so its modifiers made these faces
            self.report.error(
                mesh_object.name,
                "its modifiers make faces that aren't wholly in one bone's group. "
                "Check that they keep vertex groups; apply them to find the faces",
            )
            return
        to_rig = self.rig.matrix_world.inverted() @ mesh_object.matrix_world
        normal_to_rig = to_rig.to_3x3().inverted().transposed()
        # A mirrored object turns its faces inside out unless their corners are swapped
        corner_order = (
            slice(None, None, -1) if to_rig.determinant() < 0 else slice(None)
        )
        mesh.calc_loop_triangles()
        uv_layer = mesh.uv_layers.active
        normals = mesh.corner_normals
        slots = mesh_object.material_slots
        for triangle in mesh.loop_triangles:
            bone = found.bones[triangle.polygon_index]
            if (
                triangle.material_index >= len(slots)
                or slots[triangle.material_index].material is None
            ):
                self.report.error(
                    mesh_object.name,
                    "its modifiers make faces with no material. Check their material "
                    "settings",
                )
                return
            material = self._material(slots[triangle.material_index].material)
            part = self.parts.setdefault(
                (bone, material),
                {
                    "bone": bone,
                    "material": material,
                    "positions": [],
                    "normals": [],
                    "uvs": [],
                    "triangles": [],
                },
            )
            rotation = unpose.rotations[bone]
            corners = []
            for vertex, loop in list(
                zip(triangle.vertices, triangle.loops, strict=True)
            )[corner_order]:
                position = unpose.place(bone, to_rig @ mesh.vertices[vertex].co)
                normal = (
                    rotation @ (normal_to_rig @ normals[loop].vector)
                ).normalized()
                uv = uv_layer.uv[loop].vector if uv_layer else Vector((0.0, 0.0))
                part["positions"].append(
                    [round(c, POSITION_DIGITS) for c in rigs.to_model(position)]
                )
                part["normals"].append(
                    [round(c, NORMAL_DIGITS) for c in rigs.direction_to_model(normal)]
                )
                part["uvs"].append([round(uv.x, UV_DIGITS), round(uv.y, UV_DIGITS)])
                corners.append(len(part["positions"]) - 1)
            part["triangles"].append(corners)

    def _gather(self) -> Unposing | None:
        """Gather the parts at the rig's rest pose, whatever pose it is in."""
        meshes = bound_meshes(self.rig)
        hidden = [mesh.name for mesh in meshes if mesh.hide_render]
        if hidden:
            self.report.warn(
                ", ".join(hidden),
                "left out: hidden in renders (the camera icon in the Outliner)",
            )
        meshes = [mesh for mesh in meshes if not mesh.hide_render]
        if not meshes:
            self.report.error(
                self.rig.name,
                "no meshes are bound to it. Select the character's meshes and use Bind "
                "to Rig",
            )
            return None
        pose_position = self.rig.data.pose_position
        self.rig.data.pose_position = "REST"
        try:
            bpy.context.view_layer.update()
            unpose = unposing(self.rig, self.template)
            for mesh_object in meshes:
                self._add_mesh(mesh_object, unpose)
        finally:
            self.rig.data.pose_position = pose_position
            bpy.context.view_layer.update()
        return unpose

    def write(self) -> Report:
        """Write the package, unless the rig or its meshes have problems."""
        check_rig(self.rig, self.template, self.report)
        if self.report.errors:
            return self.report
        unpose = self._gather()
        if unpose is None or self.report.errors:
            return self.report
        document = {
            "format": PACKAGE_FORMAT,
            "version": PACKAGE_VERSION,
            "name": self.name,
            "slot": self.template["slot"],
            "skeleton": {
                name: [round(c, POSITION_DIGITS) for c in rigs.to_model(head)]
                for name, head in unpose.heads.items()
            },
            "materials": [
                entry.document
                for entry in sorted(self.materials.values(), key=lambda e: e.index)
            ],
            "parts": list(self.parts.values()),
        }
        (self.directory / TEXTURE_DIRECTORY).mkdir(parents=True, exist_ok=True)
        for png, file_name in self.images.values():
            (self.directory / file_name).write_bytes(png)
        (self.directory / PACKAGE_FILE).write_text(
            json.dumps(document, indent=1) + "\n"
        )
        triangles = sum(len(part["triangles"]) for part in self.parts.values())
        self.report.summary = (
            f"Exported {self.name} in place of {self.template['slot']}: "
            f"{count(triangles, 'triangle')}, "
            f"{count(len(self.materials), 'material')}, "
            f"{count(len(self.images), 'image')}"
        )
        return self.report


def export_character(rig: bpy.types.Object, directory: Path, name: str) -> Report:
    """Export the character modeled on RIG to the package DIRECTORY."""
    directory.mkdir(parents=True, exist_ok=True)
    return PackageWriter(rig, directory, name).write()
