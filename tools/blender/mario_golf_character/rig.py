"""Read a character slot's template and build its rig in Blender.

A template, which tools/character_template.py writes from the game's files,
holds the slot's skeleton in its bind pose, in the game's model units with
Y up and the character facing +Z. In Blender a character stands on Z and
faces -Y, as in the front view, so a model point (x, y, z) is the Blender
point (x, -z, y), scaled from model units to meters.
"""

import json
from pathlib import Path

import bpy
from mathutils import Vector

TEMPLATE_FORMAT = "mariogolf64-character-template"
TEMPLATE_VERSION = 1
# Model units per meter. Charlie, 1600 units tall, is 1.6 m tall.
MODEL_UNITS_PER_METER = 1000.0
# The custom properties that tie a rig to its template
TEMPLATE_PATH_PROPERTY = "mg64_template"
SLOT_PROPERTY = "mg64_slot"
REFERENCE_SUFFIX = " reference"
REFERENCE_DISPLAY = "WIRE"


class TemplateError(ValueError):
    """A template file can't be used."""


def to_blender(point: list[float] | tuple[float, ...]) -> Vector:
    """Return a model-space point, in meters, in Blender's axes."""
    x, y, z = point
    return Vector((x, -z, y)) / MODEL_UNITS_PER_METER


def to_model(point: Vector) -> tuple[float, float, float]:
    """Return a Blender point, in meters, in model space and units."""
    return (
        point.x * MODEL_UNITS_PER_METER,
        point.z * MODEL_UNITS_PER_METER,
        -point.y * MODEL_UNITS_PER_METER,
    )


def direction_to_model(direction: Vector) -> tuple[float, float, float]:
    """Return a Blender direction in model space's axes, unscaled."""
    return (direction.x, direction.z, -direction.y)


# Each template read, by path, with the modification time it was read at:
# panels ask for the template on every redraw
_templates: dict[Path, tuple[int, dict]] = {}


def read_template(path: Path) -> dict:
    """Read and check the template file at PATH; callers mustn't change the result."""
    try:
        modified = path.stat().st_mtime_ns
        cached = _templates.get(path)
        if cached is not None and cached[0] == modified:
            return cached[1]
        document = json.loads(path.read_text())
    except OSError as error:
        msg = f"Can't read the template {path}: {error.strerror}"
        raise TemplateError(msg) from None
    except json.JSONDecodeError:
        msg = f"{path} isn't a template; make it again with tools/character_template.py"
        raise TemplateError(msg) from None
    if document.get("format") != TEMPLATE_FORMAT:
        msg = f"{path} isn't a character template"
        raise TemplateError(msg)
    if document.get("version") != TEMPLATE_VERSION:
        msg = f"{path} is from another version of the tools; make it again"
        raise TemplateError(msg)
    _templates[path] = (modified, document)
    return document


def add_rig(context: bpy.types.Context, template_path: Path) -> bpy.types.Object:
    """Add the template's rig and reference mesh at the 3D cursor."""
    template = read_template(template_path)
    slot = template["slot"]
    armature = bpy.data.armatures.new(f"{slot} rig")
    rig = bpy.data.objects.new(f"{slot} rig", armature)
    rig[TEMPLATE_PATH_PROPERTY] = str(template_path)
    rig[SLOT_PROPERTY] = slot
    rig.show_in_front = True
    rig.location = context.scene.cursor.location
    context.collection.objects.link(rig)
    for selected in context.selected_objects:
        selected.select_set(state=False)
    context.view_layer.objects.active = rig
    rig.select_set(state=True)
    bpy.ops.object.mode_set(mode="EDIT")
    for bone in template["bones"]:
        edit_bone = armature.edit_bones.new(bone["name"])
        edit_bone.head = to_blender(bone["head"])
        edit_bone.tail = to_blender(bone["tail"])
        edit_bone.roll = 0.0
        # Bones without a part of the slot's character are the joints between
        # parts; they still move, so a model can use them
        if bone["parent"]:
            edit_bone.parent = armature.edit_bones[bone["parent"]]
    bpy.ops.object.mode_set(mode="OBJECT")
    _add_reference(context, rig, template)
    return rig


def _add_reference(
    context: bpy.types.Context, rig: bpy.types.Object, template: dict
) -> None:
    """Add the slot's own model as a wireframe to model beside, kept out of exports."""
    vertices: list[Vector] = []
    faces: list[tuple[int, int, int]] = []
    for part in template["reference"].values():
        for triangle in part["positions"]:
            start = len(vertices)
            vertices.extend(to_blender(corner) for corner in triangle)
            faces.append((start, start + 1, start + 2))
    mesh = bpy.data.meshes.new(rig.name + REFERENCE_SUFFIX)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    reference = bpy.data.objects.new(rig.name + REFERENCE_SUFFIX, mesh)
    reference.display_type = REFERENCE_DISPLAY
    reference.hide_select = True
    reference.hide_render = True
    reference.parent = rig
    context.collection.objects.link(reference)


def rig_template(rig: bpy.types.Object) -> tuple[Path, dict]:
    """Return the template that a rig was made from."""
    if TEMPLATE_PATH_PROPERTY not in rig:
        msg = f"{rig.name} wasn't made with Add Template Rig, so it has no template"
        raise TemplateError(msg)
    path = Path(rig[TEMPLATE_PATH_PROPERTY])
    return path, read_template(path)


def find_rig(context: bpy.types.Context) -> bpy.types.Object | None:
    """Return the rig that the active object is, or belongs to."""
    candidates = [context.active_object, *(context.selected_objects or [])]
    for candidate in candidates:
        current = candidate
        while current is not None:
            if current.type == "ARMATURE" and TEMPLATE_PATH_PROPERTY in current:
                return current
            current = current.parent
    rigs = [o for o in context.scene.objects if TEMPLATE_PATH_PROPERTY in o]
    return rigs[0] if len(rigs) == 1 else None


def face_texture_paths(
    template_path: Path, template: dict
) -> dict[str, dict[int, Path]]:
    """Return the template's face texture previews, by model kind and texture index."""
    return {
        kind: {
            int(index): template_path.parent / name
            for index, name in face["textures"].items()
        }
        for kind, face in template["faces"].items()
    }
