"""Model a custom Mario Golf character on a template rig and export it.

The sidebar's Mario Golf tab takes a character through four steps: add the
template rig of the character it replaces, fit the rig to the model, assign
each part's faces to its bone's vertex group and bind, and export a package
that tools/build_character.py builds into the game.
"""

import subprocess
import sys
import textwrap
from pathlib import Path
from typing import override

import bpy
from bpy.props import (
    BoolProperty,
    CollectionProperty,
    EnumProperty,
    FloatVectorProperty,
    IntProperty,
    PointerProperty,
    StringProperty,
)

from . import binding, export, rig
from .problems import Problem, Report, count

TEMPLATE_DIRECTORY = "build/character_templates"
TEMPLATE_TOOL = "tools/character_template.py"
BUILD_TOOL = "tools/build_character.py"
REPOSITORY_PYTHON = ".venv/bin/python3"
REPOSITORY_MARKER = "mariogolf64.yaml"
DEFAULT_SLOT = "charlie"
DEFAULT_COLOR = (0.8, 0.8, 0.8, 1.0)
SIDEBAR_CATEGORY = "Mario Golf"
TOOL_TIMEOUT_SECONDS = 300
# Characters a popup line holds before it wraps
POPUP_LINE_WIDTH = 90
PREFERENCES_HINT = "Edit > Preferences > Add-ons > Mario Golf Character"


def _preferences(context: bpy.types.Context) -> "MG64Preferences":
    return context.preferences.addons[__package__].preferences


def _repository(context: bpy.types.Context) -> Path | None:
    path = _preferences(context).repository
    return Path(bpy.path.abspath(path)) if path else None


def _repository_problem(repository: Path | None) -> str | None:
    """Why the decompilation folder can't be used, if it can't."""
    if repository is None:
        return f"Set the Decompilation Folder in {PREFERENCES_HINT} first"
    if not (repository / REPOSITORY_MARKER).is_file():
        return (
            f"{repository} isn't the mariogolf64 decompilation. Fix the folder in "
            f"{PREFERENCES_HINT}"
        )
    return None


def _run_tool(repository: Path, arguments: list[str]) -> tuple[bool, str]:
    """Run one of the repository's tools; whether it worked, and what it said."""
    python = repository / REPOSITORY_PYTHON
    if not python.exists():
        python = Path(sys.executable)
    try:
        result = subprocess.run(
            [str(python), *arguments],
            cwd=repository,
            capture_output=True,
            text=True,
            timeout=TOOL_TIMEOUT_SECONDS,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        return False, f"Couldn't run {arguments[0]}: {error}"
    message = (result.stdout + result.stderr).strip()
    # The tools name themselves in errors; the artist knows what they ran
    return result.returncode == 0, message.removeprefix(Path(arguments[0]).name + ": ")


def _select_in_mesh(mesh: bpy.types.Mesh, problem: Problem) -> None:
    """Select the faces and vertices that a problem is about."""
    for index in problem.faces:
        polygon = mesh.polygons[index]
        polygon.select = True
        for vertex in polygon.vertices:
            mesh.vertices[vertex].select = True
    for index in problem.vertices:
        mesh.vertices[index].select = True


def _select_at_fault(context: bpy.types.Context, problems: list[Problem]) -> list[str]:
    """Select the faces and vertices that problems are about; return their objects."""
    objects: list[str] = []
    for problem in problems:
        mesh_object = bpy.data.objects.get(problem.object_name)
        if (
            not problem.has_selection
            or mesh_object is None
            or mesh_object.type != "MESH"
        ):
            continue
        mesh = mesh_object.data
        if problem.object_name not in objects:
            objects.append(problem.object_name)
            for elements in (mesh.vertices, mesh.edges, mesh.polygons):
                elements.foreach_set("select", [False] * len(elements))
        _select_in_mesh(mesh, problem)
    if objects:
        for selected in context.selected_objects:
            selected.select_set(state=False)
        for name in objects:
            bpy.data.objects[name].select_set(state=True)
        context.view_layer.objects.active = bpy.data.objects[objects[0]]
    return objects


def show_report(
    operator: bpy.types.Operator, context: bpy.types.Context, report: Report, title: str
) -> None:
    """Tell the artist what a step found, in the Info log, by selection and a popup."""
    at_fault = _select_at_fault(context, report.errors)
    lines: list[str] = []
    for problem in report.errors:
        operator.report({"ERROR"}, str(problem))
        lines.append(f"• {problem}")
    for problem in report.warnings:
        operator.report({"WARNING"}, str(problem))
        lines.append(f"• {problem}")
    if at_fault:
        lines.append(
            f"The faces at fault are selected in {', '.join(at_fault)}; press Tab to "
            "see them."
        )
    if report.summary:
        operator.report({"INFO"}, report.summary)
    if not lines or bpy.app.background:
        return
    icon = "ERROR" if report.errors else "INFO"
    heading = title if report.errors else report.summary or title
    wrapped = [
        part
        for line in lines
        for part in textwrap.wrap(line, POPUP_LINE_WIDTH, subsequent_indent="  ")
    ]

    def draw(menu: bpy.types.Menu, _context: bpy.types.Context) -> None:
        for line in wrapped:
            menu.layout.label(text=line)

    context.window_manager.popup_menu(draw, title=heading, icon=icon)


class MG64Preferences(bpy.types.AddonPreferences):
    """Where the decompilation is, whose tools make templates and check packages."""

    bl_idname = __package__

    repository: StringProperty(
        name="Decompilation Folder",
        description="The mariogolf64 repository, set up with make setup",
        subtype="DIR_PATH",
    )

    @override
    def draw(self, _context: bpy.types.Context) -> None:
        self.layout.prop(self, "repository")


class MG64FaceImage(bpy.types.PropertyGroup):
    """The image a face material shows in place of one of the template's textures."""

    kind: StringProperty()
    index: IntProperty()
    image: PointerProperty(type=bpy.types.Image, name="Image")
    # The template's own texture, shown beside it for reference
    template_image: PointerProperty(type=bpy.types.Image)


# Each list of Face Part choices made, kept because Blender shows the
# strings that a choices function returns without holding on to them
_face_group_items: dict[tuple[str, ...], list[tuple[str, str, str]]] = {}


def _face_groups(
    _self: bpy.types.PropertyGroup, context: bpy.types.Context | None
) -> list:
    groups = {"eyes"}
    found = rig.find_rig(context) if context else None
    if found is not None:
        try:
            _, template = rig.rig_template(found)
        except rig.TemplateError:
            template = None
        if template:
            for face in template["faces"].values():
                groups.update(face["groups"])
    key = tuple(sorted(groups))
    if key not in _face_group_items:
        _face_group_items[key] = [
            (group, group.capitalize(), f"The template's animated {group}")
            for group in key
        ]
    return _face_group_items[key]


class MG64MaterialSettings(bpy.types.PropertyGroup):
    """How the game draws a material."""

    role: EnumProperty(
        name="Drawn As",
        items=[
            (export.ROLE_COLOR, "Color", "Its Base Color, lit"),
            (
                export.ROLE_TEXTURE,
                "Texture",
                "The Image Texture that feeds its Base Color, lit",
            ),
            (
                export.ROLE_FACE,
                "Face",
                "Images that the game's animations swap, as for blinking",
            ),
        ],
        default=export.ROLE_COLOR,
    )
    face_group: EnumProperty(name="Face Part", items=_face_groups)
    face_images: CollectionProperty(type=MG64FaceImage)
    use_palettes: BoolProperty(
        name="Alternate Colors",
        description="Give it other colors in the game's three alternate color sets",
    )
    palette_1: FloatVectorProperty(
        name="Colors 2",
        subtype="COLOR",
        size=4,
        min=0.0,
        max=1.0,
        default=DEFAULT_COLOR,
    )
    palette_2: FloatVectorProperty(
        name="Colors 3",
        subtype="COLOR",
        size=4,
        min=0.0,
        max=1.0,
        default=DEFAULT_COLOR,
    )
    palette_3: FloatVectorProperty(
        name="Colors 4",
        subtype="COLOR",
        size=4,
        min=0.0,
        max=1.0,
        default=DEFAULT_COLOR,
    )

    def palette_colors(self) -> list[tuple[float, ...]]:
        """Return the material's colors in the alternate color sets."""
        return [tuple(self.palette_1), tuple(self.palette_2), tuple(self.palette_3)]

    def face_image(self, kind: str, index: int) -> bpy.types.Image | None:
        """Return the image shown in place of a template texture, if one is picked."""
        for entry in self.face_images:
            if entry.kind == kind and entry.index == index:
                return entry.image
        return None


def _rig_or_report(
    operator: bpy.types.Operator, context: bpy.types.Context
) -> bpy.types.Object | None:
    found = rig.find_rig(context)
    if found is None:
        operator.report(
            {"ERROR"},
            "Select the character's rig first, or add one with Add Template Rig",
        )
    return found


def _template_or_report(
    operator: bpy.types.Operator, found: bpy.types.Object
) -> dict | None:
    try:
        return rig.rig_template(found)[1]
    except rig.TemplateError as error:
        operator.report({"ERROR"}, str(error))
        return None


def _selected_meshes(
    context: bpy.types.Context, found: bpy.types.Object
) -> list[bpy.types.Object]:
    reference = found.name + rig.REFERENCE_SUFFIX
    return [
        o for o in context.selected_objects if o.type == "MESH" and o.name != reference
    ]


class MG64_OT_add_template_rig(bpy.types.Operator):
    """Add the rig of the character to replace, with its model as a wireframe guide."""

    bl_idname = "mg64.add_template_rig"
    bl_label = "Add Template Rig"
    bl_options = {"REGISTER", "UNDO"}

    slot: StringProperty(
        name="Character", default=DEFAULT_SLOT, description="The character to replace"
    )

    @override
    def execute(self, context: bpy.types.Context) -> set[str]:
        repository = _repository(context)
        problem = _repository_problem(repository)
        if problem is not None:
            self.report({"ERROR"}, problem)
            return {"CANCELLED"}
        path = repository / TEMPLATE_DIRECTORY / f"{self.slot}.json"
        if not path.exists():
            ok, message = _run_tool(
                repository,
                [TEMPLATE_TOOL, "--repository", str(repository), self.slot, str(path)],
            )
            if not ok:
                self.report(
                    {"ERROR"}, message or f"Couldn't make the template of {self.slot}"
                )
                return {"CANCELLED"}
        if context.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        try:
            added = rig.add_rig(context, path)
        except rig.TemplateError as error:
            self.report({"ERROR"}, str(error))
            return {"CANCELLED"}
        self.report(
            {"INFO"},
            f"Added {added.name}. Next, in Edit Mode, move its bones to your model's "
            "joints",
        )
        return {"FINISHED"}


class MG64_OT_add_bone_groups(bpy.types.Operator):
    """Give the selected meshes a vertex group for each bone, to assign faces to."""

    bl_idname = "mg64.add_bone_groups"
    bl_label = "Add Bone Groups"
    bl_options = {"REGISTER", "UNDO"}

    fill: BoolProperty(
        name="Fill Unassigned Faces",
        description=(
            "Put each face that's in no bone's group into its nearest bone's, as a "
            "start to correct"
        ),
        default=True,
    )

    @override
    def execute(self, context: bpy.types.Context) -> set[str]:
        found = _rig_or_report(self, context)
        if found is None:
            return {"CANCELLED"}
        template = _template_or_report(self, found)
        meshes = _selected_meshes(context, found)
        if template is None:
            return {"CANCELLED"}
        if not meshes:
            self.report({"ERROR"}, "Select the meshes to add bone groups to")
            return {"CANCELLED"}
        if context.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        filled = sum(
            binding.add_bone_groups(found, mesh, template, fill=self.fill)
            for mesh in meshes
        )
        names = ", ".join(mesh.name for mesh in meshes)
        if filled:
            self.report(
                {"INFO"},
                f"Added bone groups to {names} and put {count(filled, 'face')} in "
                "their nearest bone's group. Check them in Edit Mode, then Bind to Rig",
            )
        else:
            self.report(
                {"INFO"},
                f"Added bone groups to {names}. Assign each part's faces to its bone's "
                "group",
            )
        return {"FINISHED"}


class MG64_OT_bind_to_rig(bpy.types.Operator):
    """Bind the selected meshes to the rig, once each face is in one bone's group."""

    bl_idname = "mg64.bind_to_rig"
    bl_label = "Bind to Rig"
    bl_options = {"REGISTER", "UNDO"}

    @override
    def execute(self, context: bpy.types.Context) -> set[str]:
        found = _rig_or_report(self, context)
        if found is None:
            return {"CANCELLED"}
        template = _template_or_report(self, found)
        meshes = _selected_meshes(context, found)
        if template is None:
            return {"CANCELLED"}
        if not meshes:
            self.report({"ERROR"}, "Select the meshes to bind")
            return {"CANCELLED"}
        if context.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        report = Report()
        for mesh_object in meshes:
            report.extend(binding.bind(found, mesh_object, template))
        if not report.errors:
            names = ", ".join(mesh.name for mesh in meshes)
            report.summary = f"Bound {names}. Pose the rig to see how the parts move"
        show_report(self, context, report, "Can't bind yet")
        return {"CANCELLED"} if report.errors else {"FINISHED"}


class MG64_OT_list_face_images(bpy.types.Operator):
    """List the template's images that this face material needs replacements for."""

    bl_idname = "mg64.list_face_images"
    bl_label = "List Face Images"
    bl_options = {"REGISTER", "UNDO"}

    @override
    def execute(self, context: bpy.types.Context) -> set[str]:
        found = _rig_or_report(self, context)
        material = context.material
        if found is None or material is None:
            return {"CANCELLED"}
        try:
            path, template = rig.rig_template(found)
        except rig.TemplateError as error:
            self.report({"ERROR"}, str(error))
            return {"CANCELLED"}
        previews = rig.face_texture_paths(path, template)
        settings = material.mg64
        listed = {(entry.kind, entry.index): entry for entry in settings.face_images}
        for kind, face in template["faces"].items():
            for index in face["groups"].get(settings.face_group, []):
                entry = listed.get((kind, index))
                if entry is None:
                    entry = settings.face_images.add()
                    entry.kind = kind
                    entry.index = index
                preview = previews.get(kind, {}).get(index)
                if preview is not None and preview.exists():
                    entry.template_image = bpy.data.images.load(
                        str(preview), check_existing=True
                    )
                    entry.template_image.preview_ensure()
        return {"FINISHED"}


class MG64_OT_export(bpy.types.Operator):
    """Export the character as a package for the game's build, and check it builds."""

    bl_idname = "mg64.export"
    bl_label = "Export Character"

    @override
    def execute(self, context: bpy.types.Context) -> set[str]:
        found = _rig_or_report(self, context)
        if found is None:
            return {"CANCELLED"}
        settings = context.scene.mg64
        if not settings.package_directory:
            self.report({"ERROR"}, "Choose a Package Folder to export to")
            return {"CANCELLED"}
        if not settings.character_name.strip():
            self.report({"ERROR"}, "Give the character a Name")
            return {"CANCELLED"}
        if context.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        directory = Path(bpy.path.abspath(settings.package_directory))
        try:
            report = export.export_character(
                found, directory, settings.character_name.strip()
            )
        except rig.TemplateError as error:
            self.report({"ERROR"}, str(error))
            return {"CANCELLED"}
        except OSError as error:
            self.report(
                {"ERROR"},
                f"Couldn't write the package to {directory}: {error.strerror}",
            )
            return {"CANCELLED"}
        if not report.errors:
            self._check_build(context, directory, report)
        show_report(self, context, report, "Can't export yet")
        return {"CANCELLED"} if report.errors else {"FINISHED"}

    @staticmethod
    def _check_build(
        context: bpy.types.Context, directory: Path, report: Report
    ) -> None:
        """Check that the game's build takes the package, as far as the add-on can't."""
        repository = _repository(context)
        if _repository_problem(repository) is not None:
            report.warn(
                "Package",
                "not checked against the game's limits: set the Decompilation Folder",
            )
            return
        ok, message = _run_tool(
            repository,
            [BUILD_TOOL, "--check", "--repository", str(repository), str(directory)],
        )
        if ok:
            report.summary += f". {message}"
        else:
            report.error("The game's build", message)


class MG64SceneSettings(bpy.types.PropertyGroup):
    """The export settings kept with the file."""

    character_name: StringProperty(name="Name", default="Custom character")
    package_directory: StringProperty(
        name="Package Folder",
        subtype="DIR_PATH",
        default="//character_package/",
        # Relative to the blend, so the package travels with it
        options={"PATH_SUPPORTS_BLEND_RELATIVE"},
    )


class MG64_PT_character(bpy.types.Panel):
    """The steps from a model to a character package."""

    bl_label = "Custom Character"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = SIDEBAR_CATEGORY

    @override
    def draw(self, context: bpy.types.Context) -> None:
        layout = self.layout
        found = rig.find_rig(context)
        step = layout.box()
        step.label(text="1. Rig", icon="ARMATURE_DATA")
        if found is None:
            problem = _repository_problem(_repository(context))
            if problem is not None:
                step.label(
                    text="Set the Decompilation Folder in Preferences", icon="ERROR"
                )
            step.operator(MG64_OT_add_template_rig.bl_idname).slot = DEFAULT_SLOT
        else:
            step.label(text=f"{found.name}, in place of {found.get(rig.SLOT_PROPERTY)}")
        step = layout.box()
        step.label(text="2. Fit", icon="EDITMODE_HLT")
        step.label(text="In Edit Mode, move bones to your model's joints")
        step = layout.box()
        step.label(text="3. Parts", icon="GROUP_VERTEX")
        step.label(text="Assign each part's faces to its bone's group")
        step.operator(MG64_OT_add_bone_groups.bl_idname)
        step.operator(MG64_OT_bind_to_rig.bl_idname)
        if found is not None:
            step.label(
                text=f"{count(len(export.bound_meshes(found)), 'mesh', 'meshes')} bound"
            )
        step = layout.box()
        step.label(text="4. Export", icon="EXPORT")
        settings = context.scene.mg64
        step.prop(settings, "character_name")
        step.prop(settings, "package_directory")
        step.operator(MG64_OT_export.bl_idname)


class MG64_PT_material(bpy.types.Panel):
    """How the game draws the active material."""

    bl_label = "Mario Golf"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "material"

    @override
    @classmethod
    def poll(cls, context: bpy.types.Context) -> bool:
        return context.material is not None

    @override
    def draw(self, context: bpy.types.Context) -> None:
        layout = self.layout
        material = context.material
        settings = material.mg64
        layout.prop(settings, "role", expand=True)
        layout.prop(material, "use_backface_culling", text="Hide Back Faces")
        if settings.role == export.ROLE_FACE:
            self._draw_face(context, layout, settings)
        layout.prop(settings, "use_palettes")
        if settings.use_palettes:
            column = layout.column(align=True)
            column.prop(settings, "palette_1")
            column.prop(settings, "palette_2")
            column.prop(settings, "palette_3")

    @staticmethod
    def _draw_face(
        context: bpy.types.Context,
        layout: bpy.types.UILayout,
        settings: MG64MaterialSettings,
    ) -> None:
        layout.prop(settings, "face_group")
        found = rig.find_rig(context)
        if found is None:
            layout.label(
                text="Add the template rig to see its face images", icon="INFO"
            )
            return
        try:
            _, template = rig.rig_template(found)
        except rig.TemplateError as error:
            layout.label(text=str(error), icon="ERROR")
            return
        needed = [
            (kind, index)
            for kind, face in template["faces"].items()
            for index in face["groups"].get(settings.face_group, [])
        ]
        listed = {(entry.kind, entry.index) for entry in settings.face_images}
        if not set(needed) <= listed:
            layout.operator(MG64_OT_list_face_images.bl_idname)
            return
        layout.label(text="Pick an image in place of each of the template's:")
        for entry in settings.face_images:
            if (entry.kind, entry.index) not in needed:
                continue
            row = layout.row()
            if entry.template_image and entry.template_image.preview:
                row.template_icon(
                    icon_value=entry.template_image.preview.icon_id, scale=2.0
                )
            row.label(text=f"{entry.kind.capitalize()} {entry.index}")
            row.prop(entry, "image", text="")


CLASSES = (
    MG64Preferences,
    MG64FaceImage,
    MG64MaterialSettings,
    MG64SceneSettings,
    MG64_OT_add_template_rig,
    MG64_OT_add_bone_groups,
    MG64_OT_bind_to_rig,
    MG64_OT_list_face_images,
    MG64_OT_export,
    MG64_PT_character,
    MG64_PT_material,
)


def register() -> None:
    """Add the add-on's settings, operators and panels to Blender."""
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.Material.mg64 = PointerProperty(type=MG64MaterialSettings)
    bpy.types.Scene.mg64 = PointerProperty(type=MG64SceneSettings)


def unregister() -> None:
    """Remove what register() added."""
    del bpy.types.Scene.mg64
    del bpy.types.Material.mg64
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
