"""Test the Mario Golf Character add-on inside Blender, without a window.

Run by tools/blender/run_tests.sh, which installs the add-on into a
throwaway Blender user folder and runs:

    blender -b --factory-startup --python THIS -- REPOSITORY WORK_DIR

The repository must be set up (make setup), as the tests make Charlie's
template from its files. Each test starts from an empty scene.
"""

import json
import math
import subprocess
import sys
import traceback
import warnings
from collections.abc import Callable
from pathlib import Path

import addon_utils
import bpy
from mathutils import Matrix, Vector

ADDON = "bl_ext.user_default.mario_golf_character"
SLOT = "charlie"
# Exported positions are rounded to hundredths of a model unit
POSITION_TOLERANCE = 0.02
# How far a rig bone may sit from where its template says, as float noise
BONE_TOLERANCE = 1e-5
TESTS: list[Callable[[], None]] = []


def test(function: Callable[[], None]) -> Callable[[], None]:
    """Register a test."""
    TESTS.append(function)
    return function


def arguments() -> tuple[Path, Path]:
    """Return the repository and the work folder, after Blender's own arguments."""
    repository, work = sys.argv[sys.argv.index("--") + 1 :]
    return Path(repository), Path(work)


REPOSITORY, WORK = arguments()


def package() -> object:
    """Return the add-on's package, as Blender loaded it."""
    return sys.modules[ADDON]


def submodule(name: str) -> object:
    """Return one of the add-on's modules, by its name in the package."""
    return sys.modules[f"{ADDON}.{name}"]


def reset() -> None:
    """Return an empty scene, and the add-on pointed at the repository."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    addon_utils.enable(ADDON, default_set=True)
    bpy.context.preferences.addons[ADDON].preferences.repository = str(REPOSITORY)


def add_rig() -> bpy.types.Object:
    """Add the template rig, as its panel button does, and return it."""
    result = bpy.ops.mg64.add_template_rig(slot=SLOT)
    assert result == {"FINISHED"}, result
    return bpy.context.view_layer.objects.active


def template() -> dict:
    """Return the template that the tests' rigs are added from."""
    return json.loads(
        (REPOSITORY / "build/character_templates" / f"{SLOT}.json").read_text()
    )


def material(name: str, color: tuple[float, float, float, float]) -> bpy.types.Material:
    """Return a new material whose viewport color is COLOR."""
    made = bpy.data.materials.new(name)
    made.diffuse_color = color
    return made


def reference_meshes() -> dict[str, bpy.types.Object]:
    """Return an unbound mesh of each bone's reference triangles, in its group."""
    rig_module = submodule("rig")
    shared = material("skin", (0.5, 0.25, 0.1, 1.0))
    meshes = {}
    for bone, part in template()["reference"].items():
        vertices: list[Vector] = []
        faces = []
        for triangle in part["positions"]:
            faces.append(tuple(range(len(vertices), len(vertices) + 3)))
            vertices += [rig_module.to_blender(corner) for corner in triangle]
        mesh = bpy.data.meshes.new(bone)
        mesh.from_pydata(vertices, [], faces)
        mesh_object = bpy.data.objects.new(bone, mesh)
        bpy.context.collection.objects.link(mesh_object)
        mesh_object.data.materials.append(shared)
        group = mesh_object.vertex_groups.new(name=bone)
        group.add(list(range(len(vertices))), 1.0, "REPLACE")
        meshes[bone] = mesh_object
    return meshes


def select(objects: list[bpy.types.Object], active: bpy.types.Object) -> None:
    """Select just OBJECTS, with ACTIVE active."""
    for selected in bpy.context.selected_objects:
        selected.select_set(state=False)
    for chosen in objects:
        chosen.select_set(state=True)
    bpy.context.view_layer.objects.active = active


def run(operator: Callable[[], set[str]]) -> set[str]:
    """Run an operator as the panel does: one that reports errors is cancelled."""
    try:
        return operator()
    except RuntimeError:
        # Blender raises for an operator's error reports when Python calls it
        return {"CANCELLED"}


def bind(rig: bpy.types.Object, meshes: list[bpy.types.Object]) -> set[str]:
    """Bind MESHES to RIG, as the panel's button does."""
    select([*meshes, rig], meshes[0])
    return run(bpy.ops.mg64.bind_to_rig)


def export(directory: Path) -> set[str]:
    """Export the scene's character to DIRECTORY, as the panel's button does."""
    bpy.context.scene.mg64.package_directory = str(directory)
    bpy.context.scene.mg64.character_name = "Test"
    return run(bpy.ops.mg64.export)


def exported(directory: Path) -> dict:
    """Return the character document exported to DIRECTORY."""
    return json.loads((directory / "character.json").read_text())


def positions_by_bone(document: dict) -> dict[str, list[tuple[float, ...]]]:
    """Return every exported triangle's corners, sorted, by bone."""
    found: dict[str, list[tuple[float, ...]]] = {}
    for part in document["parts"]:
        for triangle in part["triangles"]:
            corners = tuple(sorted(tuple(part["positions"][i]) for i in triangle))
            found.setdefault(part["bone"], []).append(corners)
    return {bone: sorted(triangles) for bone, triangles in found.items()}


def assert_close_triangles(actual: dict, expected: dict, tolerance: float) -> None:
    """Check that each bone has the same triangles, with corners within TOLERANCE."""
    assert actual.keys() == expected.keys(), (sorted(actual), sorted(expected))
    for bone, triangles in expected.items():
        assert len(actual[bone]) == len(triangles), bone
        for got, want in zip(actual[bone], triangles, strict=True):
            for got_corner, want_corner in zip(got, want, strict=True):
                assert math.dist(got_corner, want_corner) <= tolerance, (
                    bone,
                    got_corner,
                    want_corner,
                )


def error_messages(report: object) -> list[str]:
    """Return the errors in a bind or export report, as the artist reads them."""
    return [str(problem) for problem in report.errors]


@test
def rig_matches_template() -> None:
    """Check that the rig's bones sit where the template says, with its parents."""
    reset()
    rig = add_rig()
    rig_module = submodule("rig")
    for bone in template()["bones"]:
        made = rig.data.bones[bone["name"]]
        offset = made.head_local - rig_module.to_blender(bone["head"])
        assert offset.length < BONE_TOLERANCE, bone["name"]
        assert (made.parent.name if made.parent else None) == bone["parent"], bone[
            "name"
        ]


@test
def unfitted_export_keeps_reference_positions() -> None:
    """With the rig as added, the export's positions are the template's own."""
    reset()
    rig = add_rig()
    meshes = reference_meshes()
    assert bind(rig, list(meshes.values())) == {"FINISHED"}
    assert export(WORK / "unfitted") == {"FINISHED"}
    expected = {
        bone: sorted(
            tuple(sorted(tuple(c) for c in triangle)) for triangle in part["positions"]
        )
        for bone, part in template()["reference"].items()
    }
    assert_close_triangles(
        positions_by_bone(exported(WORK / "unfitted")), expected, POSITION_TOLERANCE
    )


def fit(rig: bpy.types.Object, meshes: dict, bones: list[str], turn: Matrix) -> None:
    """Move bones' heads and tails, and their parts, as an artist fitting a rig does."""
    select([rig], rig)
    bpy.ops.object.mode_set(mode="EDIT")
    for name in bones:
        bone = rig.data.edit_bones[name]
        bone.head, bone.tail = turn @ bone.head, turn @ bone.tail
    bpy.ops.object.mode_set(mode="OBJECT")
    for name in bones:
        if name in meshes:
            meshes[name].data.transform(turn)


def turn_about(
    rig: bpy.types.Object, bone: str, axis: Vector, degrees: float
) -> Matrix:
    """Return a turn about a bone's head."""
    pivot = rig.data.bones[bone].head_local.copy()
    return (
        Matrix.Translation(pivot)
        @ Matrix.Rotation(math.radians(degrees), 4, axis)
        @ Matrix.Translation(-pivot)
    )


@test
def fitted_bones_unpose_their_parts() -> None:
    """Check that turning bones and their parts together exports the same parts.

    That's what fitting the rig to a model posed unlike the template does.

    The turns are about axes square to the bones, where a bone's direction
    says all of its turn: the head nods, and the leg, whose bones lie in a
    plane, swings in it.
    """
    reset()
    rig = add_rig()
    meshes = reference_meshes()
    assert bind(rig, list(meshes.values())) == {"FINISHED"}
    assert export(WORK / "before_fit") == {"FINISHED"}
    fit(rig, meshes, ["head"], turn_about(rig, "head", Vector((1, 0, 0)), 25))
    leg = ["thigh.R", "shin.R", "foot.R"]
    thigh = rig.data.bones["thigh.R"]
    shin = rig.data.bones["shin.R"]
    plane_normal = (thigh.tail_local - thigh.head_local).cross(
        shin.tail_local - shin.head_local
    )
    fit(rig, meshes, leg, turn_about(rig, "thigh.R", plane_normal, 50))
    assert export(WORK / "after_fit") == {"FINISHED"}
    assert_close_triangles(
        positions_by_bone(exported(WORK / "after_fit")),
        positions_by_bone(exported(WORK / "before_fit")),
        POSITION_TOLERANCE * 2,
    )


@test
def longer_bones_change_proportions() -> None:
    """Check that lengthening a bone moves its children's heads in the skeleton."""
    reset()
    rig = add_rig()
    meshes = reference_meshes()
    assert bind(rig, list(meshes.values())) == {"FINISHED"}
    shin = rig.data.bones["shin.R"]
    lengthen = Matrix.Translation((shin.tail_local - shin.head_local) * 0.25)
    fit(rig, meshes, ["foot.R"], lengthen)
    select([rig], rig)
    bpy.ops.object.mode_set(mode="EDIT")
    rig.data.edit_bones["shin.R"].tail = rig.data.edit_bones["foot.R"].head
    bpy.ops.object.mode_set(mode="OBJECT")
    assert export(WORK / "longer") == {"FINISHED"}
    skeleton = exported(WORK / "longer")["skeleton"]
    bones = {bone["name"]: bone for bone in template()["bones"]}
    expected = (
        Vector(bones["foot.R"]["head"])
        + (Vector(bones["shin.R"]["tail"]) - Vector(bones["shin.R"]["head"])) * 0.25
    )
    assert (Vector(skeleton["foot.R"]) - expected).length < POSITION_TOLERANCE * 2, (
        skeleton["foot.R"],
        expected,
    )
    assert (
        Vector(skeleton["foot.L"]) - Vector(bones["foot.L"]["head"])
    ).length < POSITION_TOLERANCE


@test
def mirrored_object_keeps_winding() -> None:
    """Check that a negatively scaled object's faces export facing the way they show."""
    reset()
    rig = add_rig()
    bpy.ops.mesh.primitive_cube_add(size=0.2, location=(0, 0, 1.2))
    cube = bpy.context.active_object
    cube.scale.x = -1
    cube.data.materials.append(material("solid", (1, 1, 1, 1)))
    cube.vertex_groups.new(name="head").add(list(range(8)), 1.0, "REPLACE")
    assert bind(rig, [cube]) == {"FINISHED"}
    assert export(WORK / "mirrored") == {"FINISHED"}
    part = exported(WORK / "mirrored")["parts"][0]
    for triangle in part["triangles"]:
        a, b, c = (Vector(part["positions"][i]) for i in triangle)
        facing = (b - a).cross(c - a)
        normal = Vector(part["normals"][triangle[0]])
        assert facing.dot(normal) > 0, (facing, normal)


@test
def automatic_weights_are_refused_with_vertices_selected() -> None:
    """Blended weights stop the bind, say why, and select the vertices."""
    reset()
    rig = add_rig()
    bpy.ops.mesh.primitive_cube_add(size=0.2, location=(0, 0, 1.2))
    cube = bpy.context.active_object
    cube.vertex_groups.new(name="head").add([0, 1, 2, 3, 4, 5, 6, 7], 1.0, "REPLACE")
    cube.vertex_groups["head"].add([0], 0.5, "REPLACE")
    messages = error_messages(submodule("binding").bind(rig, cube, template()))
    assert any("between 0 and 1" in m and "1 vertex " in m for m in messages), messages
    assert bind(rig, [cube]) == {"CANCELLED"}
    assert [v.index for v in cube.data.vertices if v.select] == [0]
    assert cube.parent is None


@test
def faces_in_no_group_or_two_bones_are_refused() -> None:
    """Ungrouped and spanning faces stop the bind, each with its own advice."""
    reset()
    rig = add_rig()
    bpy.ops.mesh.primitive_plane_add(size=0.2, location=(0, 0, 1.2))
    plane = bpy.context.active_object
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.subdivide()  # 4 faces, 9 vertices
    bpy.ops.object.mode_set(mode="OBJECT")
    face_vertices = [set(p.vertices) for p in plane.data.polygons]
    head = plane.vertex_groups.new(name="head")
    chest = plane.vertex_groups.new(name="chest")
    head.add(list(face_vertices[0]), 1.0, "REPLACE")
    # Face 1's corners are split between the two bones; faces 2 and 3 have
    # ungrouped corners
    corners = list(face_vertices[1] - face_vertices[0])
    chest.add(corners, 1.0, "REPLACE")
    messages = error_messages(submodule("binding").bind(rig, plane, template()))
    assert any("1 face spans two bones" in m for m in messages), messages
    assert any(
        "2 faces aren't in any bone's group" in m and "Assign" in m for m in messages
    ), messages
    assert bind(rig, [plane]) == {"CANCELLED"}
    assert [p.index for p in plane.data.polygons if p.select] == [1, 2, 3]


@test
def add_bone_groups_fills_only_unassigned_faces() -> None:
    """Check that filling gives unassigned faces a bone and leaves the others alone."""
    reset()
    rig = add_rig()
    meshes = reference_meshes()
    head = meshes["head"]
    # Move the head's faces into the hand's group, which the nearest bone would undo
    head.vertex_groups["head"].name = "hand.R"
    others = [mesh for name, mesh in meshes.items() if name != "head"]
    for mesh in others:
        mesh.vertex_groups.clear()
    select([*meshes.values(), rig], head)
    assert bpy.ops.mg64.add_bone_groups(fill=True) == {"FINISHED"}
    binding = submodule("binding")
    found = binding.face_bones(head, head.data, template(), None)
    assert set(found.bones) == {"hand.R"}, set(found.bones)
    for name, mesh in meshes.items():
        found = binding.face_bones(mesh, mesh.data, template(), None)
        assert found.is_bound, name


@test
def mirror_without_vertex_groups_is_refused() -> None:
    """Check that binding refuses a Mirror modifier that doesn't mirror groups."""
    reset()
    rig = add_rig()
    bpy.ops.mesh.primitive_cube_add(size=0.2, location=(0.2, 0, 1.2))
    cube = bpy.context.active_object
    cube.vertex_groups.new(name="hand.R").add(list(range(8)), 1.0, "REPLACE")
    mirror = cube.modifiers.new("Mirror", "MIRROR")
    mirror.use_mirror_vertex_groups = False
    messages = error_messages(submodule("binding").bind(rig, cube, template()))
    assert any("doesn't mirror vertex groups" in m for m in messages), messages
    mirror.use_mirror_vertex_groups = True
    assert bind(rig, [cube]) == {"FINISHED"}
    assert cube.modifiers[-1].type == "ARMATURE"


@test
def face_material_needs_every_image() -> None:
    """Check that a face material names the template images it still needs."""
    reset()
    rig = add_rig()
    meshes = reference_meshes()
    assert bind(rig, list(meshes.values())) == {"FINISHED"}
    face = material("eyes", (1, 1, 1, 1))
    face.mg64.role = "FACE"
    face.mg64.face_group = "eyes"
    meshes["head"].data.materials.append(face)
    meshes["head"].data.polygons[0].material_index = 1
    meshes["head"].data.uv_layers.new()
    report = submodule("export").export_character(rig, WORK / "faceless", "Test")
    messages = error_messages(report)
    assert any("Main 1, Main 2, Main 3, Reaction 5" in m for m in messages), messages
    assert not (WORK / "faceless" / "character.json").exists()


@test
def package_folder_takes_blend_relative_paths() -> None:
    """Check that the default Package Folder, beside the blend, sets quietly."""
    reset()
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        bpy.context.scene.mg64.package_directory = "//character_package/"


@test
def export_builds() -> None:
    """Check that the game's build accepts an exported package."""
    reset()
    rig = add_rig()
    meshes = reference_meshes()
    assert bind(rig, list(meshes.values())) == {"FINISHED"}
    assert export(WORK / "builds") == {"FINISHED"}
    checked = subprocess.run(
        [
            str(REPOSITORY / ".venv/bin/python3"),
            "tools/build_character.py",
            "--check",
            str(WORK / "builds"),
        ],
        cwd=REPOSITORY,
        capture_output=True,
        text=True,
        check=False,
    )
    assert checked.returncode == 0, checked.stderr


@test
def colors_are_exported_as_displayed() -> None:
    """Check that linear colors are exported as the sRGB bytes Blender shows."""
    export_module = submodule("export")
    assert export_module.color_bytes((0.0, 0.5, 1.0, 1.0)) == [0, 188, 255, 255]


@test
def image_mixed_over_a_color_exports_both() -> None:
    """Check that an image mixed over a color by its alpha exports both.

    Painted eyes are often set up so.
    """
    reset()
    decal = material("decal", (0.8, 0.8, 0.8, 1))
    nodes, links = decal.node_tree.nodes, decal.node_tree.links
    output = next(n for n in nodes if n.type == "OUTPUT_MATERIAL")
    over = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
    under = nodes.new("ShaderNodeBsdfPrincipled")
    under.inputs["Base Color"].default_value = (1.0, 0.5, 0.0, 1.0)
    image_node = nodes.new("ShaderNodeTexImage")
    image_node.image = bpy.data.images.new("decal", 8, 8, alpha=True)
    mix = nodes.new("ShaderNodeMixShader")
    links.new(image_node.outputs["Color"], over.inputs["Base Color"])
    links.new(image_node.outputs["Alpha"], mix.inputs[0])
    links.new(under.outputs["BSDF"], mix.inputs[1])
    links.new(over.outputs["BSDF"], mix.inputs[2])
    links.new(mix.outputs["Shader"], output.inputs["Surface"])
    export_module = submodule("export")
    assert export_module.color_bytes(export_module.base_color(decal)) == [
        255,
        188,
        0,
        255,
    ]
    assert export_module.base_image(decal) == image_node.image


def main() -> None:
    """Run every test, report each, and exit with failure if any failed."""
    failures = 0
    for function in TESTS:
        try:
            function()
        # Every failure is reported, then the next test runs
        except Exception:  # noqa: BLE001, PERF203 - see above
            failures += 1
            print(f"FAIL {function.__name__}\n{traceback.format_exc()}")
        else:
            print(f"pass {function.__name__}")
    print(f"{len(TESTS) - failures} of {len(TESTS)} passed")
    sys.exit(1 if failures else 0)


main()
