"""Test build_character.py, which builds a custom character's model files.

The strongest check rebuilds Charlie from his own model: his parts and face,
written out as a package, must build into a model that the game poses into
the same triangles as his original. The game's files are needed for that,
so those tests are skipped until "make extract" has run.
"""

import contextlib
import dataclasses
import functools
import io
import json
import re
import tempfile
import unittest
import unittest.mock
from pathlib import Path
from typing import cast

import numpy as np

import build_character as bc
import character_model_format as fmt
import character_template as tmpl

REPOSITORY = Path(__file__).resolve().parents[2]
SLOT = "charlie"
HAVE_GAME_FILES = (REPOSITORY / "assets" / "character_models" / "charlie.bin").is_file()
NEEDS_GAME_FILES = "needs the game's files: run make extract"
# The frame of the first animation at which the models are posed and compared
POSE_FRAME_OFFSET = 7
# How many triangles a package takes to be too big for the game
TOO_MANY_TRIANGLES = 4000
# Built reaction triangles are placed by fitting, not copied, so they need
# only lie near the original's: within this many model units, at the median
REACTION_MEDIAN_DISTANCE_MAX = 4.0
SIGNED_BYTE_RANGE = 256
SIGNED_BYTE_MAX = 127
TEXEL_UNITS = 64
CHANNEL_MAX = 255

Corners = tuple[tuple[float, ...], ...]
Color = tuple[int, int, int, int]
# A triangle's corner as a package holds it: its position, normal and UV
Corner = tuple[list[float], list[float], list[float]]
WHITE: Color = (CHANNEL_MAX, CHANNEL_MAX, CHANNEL_MAX, CHANNEL_MAX)
Triangle = tuple[Corners, Color]


def _normal(vertex: fmt.DrawnVertex) -> list[float]:
    """Return a vertex's normal, as its signed color bytes hold it."""
    return [
        (c - SIGNED_BYTE_RANGE if c > SIGNED_BYTE_MAX else c) / SIGNED_BYTE_MAX
        for c in vertex.color_or_normal[:3]
    ]


def _uv(st: tuple[int, int], texture: fmt.AnimatedTexture) -> list[float]:
    """Return a vertex's texture coordinates as UVs, with V up as Blender has it."""
    s, t = st
    return [s / TEXEL_UNITS / texture.width, 1 - t / TEXEL_UNITS / texture.height]


@dataclasses.dataclass
class _PackageWriter:
    """Writes a model's triangles as a package's materials and parts."""

    directory: Path
    model: fmt.ModelFile
    materials: list[dict[str, object]] = dataclasses.field(default_factory=list)
    parts: dict[tuple[str, int], dict[str, list[list[float]]]] = dataclasses.field(
        default_factory=dict
    )
    _keys: dict[tuple[Color, int], int] = dataclasses.field(default_factory=dict)

    def image(self, texture: fmt.AnimatedTexture, name: str) -> str:
        """Write one of the model's textures as a PNG, and return its name."""
        tmpl.texture_image(self.model, texture).save(self.directory / name)
        return name

    def material(self, color: Color, texture: int) -> int:
        """Return the index of the material with COLOR or TEXTURE, adding it."""
        key = (color, texture)
        if key not in self._keys:
            material: dict[str, object] = {
                "name": f"material_{len(self.materials)}",
                "color": list(color),
            }
            if texture >= 0:
                material["texture"] = self.image(
                    self.model.textures[texture], f"texture_{texture}.png"
                )
            self._keys[key] = len(self.materials)
            self.materials.append(material)
        return self._keys[key]

    def add(self, bone: str, material: int, corners: list[Corner]) -> None:
        """Add a triangle's corners, each a position, a normal and a UV."""
        part = self.parts.setdefault(
            (bone, material),
            {"positions": [], "normals": [], "uvs": [], "triangles": []},
        )
        first = len(part["positions"])
        for position, normal, uv in corners:
            part["positions"].append(position)
            part["normals"].append(normal)
            part["uvs"].append(uv)
        part["triangles"].append([float(first + i) for i in range(3)])

    def document(self) -> dict[str, object]:
        """Return the package's character.json."""
        parts = [
            {"bone": bone, "material": material, **arrays}
            for (bone, material), arrays in self.parts.items()
        ]
        return {
            "format": bc.PACKAGE_FORMAT,
            "version": bc.PACKAGE_VERSION,
            "name": "Charlie II",
            "slot": SLOT,
            "materials": self.materials,
            "parts": parts,
        }


def write_template_package(template: tmpl.CharacterTemplate, directory: Path) -> None:
    """Write the main model of TEMPLATE's slot as a package, in its bind pose."""
    model = template.main.model
    writer = _PackageWriter(directory, model)
    texture_indices = {texture.address: i for i, texture in enumerate(model.textures)}
    for bone in template.bones:
        origin = template.main.origins[bone.joint]
        for address in model.joint_display_lists()[bone.joint]:
            for triangle in fmt.draw_triangles(model.data, address):
                texture = texture_indices.get(triangle.texture_address, -1)
                color = WHITE if texture >= 0 else triangle.prim_color
                material = writer.material(color, texture)
                corners: list[Corner] = [
                    (
                        [v.position[axis] + origin[axis] for axis in range(3)],
                        _normal(v),
                        _uv(v.st, model.textures[texture])
                        if texture >= 0
                        else [0.0, 0.0],
                    )
                    for v in triangle.vertices
                ]
                writer.add(bone.name, material, corners)
    face: dict[str, object] = {"group": "eyes"}
    for kind, kind_model in template.models().items():
        indices = kind_model.face_groups()["eyes"]
        face[kind] = {
            str(index): writer.image(
                kind_model.model.textures[index], f"{kind}_face_{index}.png"
            )
            for index in indices
        }
    writer.materials.append({"name": "eyes", "color": [CHANNEL_MAX] * 4, "face": face})
    face_material = len(writer.materials) - 1
    frame_data = model.data[fmt.segment_offset(model.entry.frame_data_start) :]
    head = next(bone for bone in template.bones if bone.name == "head")
    origin = template.main.origins[head.joint]
    for triangle in model.animated_triangles:
        texture = model.textures[triangle.texture_index]
        vertices = fmt.read_vertices(frame_data, triangle.vertices, 3)
        face_corners: list[Corner] = [
            (
                [v.position[axis] + origin[axis] for axis in range(3)],
                _normal(v),
                _uv((s, t), texture),
            )
            for v, s, t in zip(vertices, triangle.s, triangle.t, strict=True)
        ]
        writer.add(head.name, face_material, face_corners)
    _ = (directory / bc.PACKAGE_FILE).write_text(json.dumps(writer.document()))


def read_built(original: fmt.ModelFile, built: bc.BuiltModel) -> fmt.ModelFile:
    """Read a built model file, as the game would with its layout."""
    entry = dataclasses.replace(
        original.entry,
        segment_addresses=built.segments,
        frame_data_start=built.frame_start,
        frame_data_end=built.frame_end,
    )
    return fmt.read_model(built.data, entry)


def posed_triangles(model: fmt.ModelFile, frame: float) -> list[Triangle]:
    """Return the triangles that a model draws at FRAME, sorted, with face ones."""
    matrices = fmt.joint_world_matrices(model, frame)
    frame_data = model.data[fmt.segment_offset(model.entry.frame_data_start) :]
    triangles: list[Triangle] = []
    for joint, addresses in enumerate(model.joint_display_lists()):
        for address in addresses:
            for triangle in fmt.draw_triangles(model.data, address, frame_data):
                corners = tuple(
                    tuple(
                        round(c, 1)
                        for c in fmt.transform_point(
                            (
                                float(v.position[0]),
                                float(v.position[1]),
                                float(v.position[2]),
                            ),
                            matrices[joint],
                        )
                    )
                    for v in triangle.vertices
                )
                triangles.append((tuple(sorted(corners)), triangle.prim_color))
    return sorted(triangles)


def plain_part(**changes: object) -> dict[str, object]:
    """Return a part of one triangle on Charlie's head, with CHANGES made to it."""
    part: dict[str, object] = {
        "bone": "head",
        "material": 0,
        "positions": [[0, 100, 0], [10, 100, 0], [0, 110, 0]],
        "normals": [[0, 0, 1]] * 3,
        "uvs": [[0, 0]] * 3,
        "triangles": [[0, 1, 2]],
    }
    part.update(changes)
    return part


def minimal_document() -> dict[str, object]:
    """Return a package of one plain triangle on Charlie's head, which builds."""
    return {
        "format": bc.PACKAGE_FORMAT,
        "version": bc.PACKAGE_VERSION,
        "name": "Test",
        "slot": SLOT,
        "materials": [{"name": "skin", "color": [200, 150, 100, 255]}],
        "parts": [plain_part()],
    }


def many_triangles_document(count: int) -> dict[str, object]:
    """Return a package with COUNT separate triangles on Charlie's head."""
    positions: list[list[float]] = []
    triangles: list[list[int]] = []
    for index in range(count):
        x, y = (index % 60) * 2.0, 100 + (index // 60) * 2.0
        positions += [[x, y, 0], [x + 1, y, 0], [x, y + 1, 0]]
        triangles.append([3 * index, 3 * index + 1, 3 * index + 2])
    document = minimal_document()
    document["parts"] = [
        plain_part(
            positions=positions,
            normals=[[0, 0, 1]] * len(positions),
            uvs=[[0, 0]] * len(positions),
            triangles=triangles,
        )
    ]
    return document


class PackageDirectory:
    """A temporary package directory, for tests that write their own packages."""

    def __init__(self, test: unittest.TestCase) -> None:
        """Make the directory, which TEST removes when it ends."""
        temporary = tempfile.TemporaryDirectory()
        test.addCleanup(temporary.cleanup)
        self.path: Path = Path(temporary.name)

    def write(self, document: object) -> Path:
        """Write DOCUMENT as the package's character.json, and return the package."""
        _ = (self.path / bc.PACKAGE_FILE).write_text(json.dumps(document))
        return self.path


@functools.cache
def rebuilt_charlie() -> tuple[tmpl.CharacterTemplate, dict[str, list[bc.BuiltModel]]]:
    """Return Charlie's template, and his models rebuilt from a package of them."""
    template = tmpl.load_template(REPOSITORY, SLOT)
    with tempfile.TemporaryDirectory() as directory:
        write_template_package(template, Path(directory))
        models = bc.build_models(bc.read_package(Path(directory)), template)
    return template, models


@unittest.skipUnless(HAVE_GAME_FILES, NEEDS_GAME_FILES)
class RebuildCharlieTest(unittest.TestCase):
    """Rebuild Charlie from his own model, written out as a package."""

    @property
    def template(self) -> tmpl.CharacterTemplate:
        """Return Charlie's template."""
        return rebuilt_charlie()[0]

    @property
    def models(self) -> dict[str, list[bc.BuiltModel]]:
        """Return Charlie's models, rebuilt from a package of them."""
        return rebuilt_charlie()[1]

    def test_main_model_poses_as_the_original(self) -> None:
        """Check that the rebuilt main model draws the original's triangles."""
        original = self.template.main.model
        built = read_built(original, self.models["main"][0])
        frame = original.animations[0].start_frame + POSE_FRAME_OFFSET
        self.assertEqual(
            posed_triangles(built, frame), posed_triangles(original, frame)
        )

    def test_animation_data_is_the_original(self) -> None:
        """Check that each model keeps its original's animations and points."""
        for kind, built in self.models.items():
            original = self.template.models()[kind].model
            for model in built:
                with self.subTest(model=model.name):
                    rebuilt = read_built(original, model)
                    self.assertEqual(rebuilt.joints, original.joints)
                    self.assertEqual(rebuilt.animations, original.animations)
                    self.assertEqual(
                        rebuilt.texture_animations, original.texture_animations
                    )
                    self.assertEqual(
                        [(p.joint_path, p.point_id) for p in rebuilt.points],
                        [(p.joint_path, p.point_id) for p in original.points],
                    )

    def test_reaction_model_lies_near_the_original(self) -> None:
        """Check that the rebuilt reaction model's parts lie where the original's do."""
        original = self.template.models()["reaction"].model
        built = read_built(original, self.models["reaction"][0])
        frame = original.animations[0].start_frame + POSE_FRAME_OFFSET
        centers = [
            np.mean(np.array(corners), axis=0)
            for corners, _ in posed_triangles(built, frame)
        ]
        original_centers = np.array(
            [
                np.mean(np.array(corners), axis=0)
                for corners, _ in posed_triangles(original, frame)
            ]
        )
        distances = [
            float(np.min(np.linalg.norm(original_centers - center, axis=1)))  # pyright: ignore[reportUnknownMemberType]
            for center in centers
        ]
        self.assertLess(float(np.median(distances)), REACTION_MEDIAN_DISTANCE_MAX)

    def test_chosen_list_still_ends(self) -> None:
        """Check that the slot the game writes the club's call into ends after it.

        The game turns the slot's first command into a call, so the list
        must end with the command after it, or the club's list runs on.
        """
        base = self.models["main"][0]
        chosen = fmt.segment_offset(base.segments[fmt.SEGMENT_CHOSEN_DISPLAY_LIST])
        end = base.data[chosen + fmt.GFX_SIZE : chosen + fmt.CHOSEN_DISPLAY_LIST_SIZE]
        self.assertEqual(end, bc.END_DISPLAY_LIST)

    def test_palettes_differ_only_in_color(self) -> None:
        """Check that each palette file has its base's layout and size."""
        for built in self.models.values():
            base = built[0]
            for palette in built[1:]:
                with self.subTest(model=palette.name):
                    self.assertEqual(len(palette.data), len(base.data))
                    self.assertEqual(palette.segments, base.segments)


@unittest.skipUnless(HAVE_GAME_FILES, NEEDS_GAME_FILES)
class PackageSizeTest(unittest.TestCase):
    """Check what the builder says about packages too big for the game."""

    def test_small_package_builds(self) -> None:
        """Check that one triangle builds, and that its room used is reported."""
        package = bc.read_package(PackageDirectory(self).write(minimal_document()))
        models = bc.build_models(package, tmpl.load_template(REPOSITORY, SLOT))
        report = bc.check_report(package, models)
        self.assertRegex(report, r"^Test can replace charlie\. Room used: main \d+%")

    def test_too_many_triangles_are_refused(self) -> None:
        """Check that an oversized model is refused with what to do about it."""
        directory = PackageDirectory(self).write(
            many_triangles_document(TOO_MANY_TRIANGLES)
        )
        package = bc.read_package(directory)
        template = tmpl.load_template(REPOSITORY, SLOT)
        with self.assertRaises(bc.PackageError) as caught:
            _ = bc.build_models(package, template)
        message = str(caught.exception)
        self.assertIn("Test's main model is too big for the game", message)
        self.assertIn("Use fewer triangles", message)


class PackageErrorTest(unittest.TestCase):
    """Check that faulty packages are refused, saying where and what to do."""

    def refusal(self, document: object) -> str:
        """Return what the builder says when it refuses DOCUMENT."""
        directory = PackageDirectory(self).write(document)
        with self.assertRaises(bc.PackageError) as caught:
            _ = bc.read_package(directory)
        return str(caught.exception)

    def test_minimal_package_reads(self) -> None:
        """Check that the package the other tests break is itself sound."""
        package = bc.read_package(PackageDirectory(self).write(minimal_document()))
        self.assertEqual(package.slot, SLOT)
        self.assertEqual(package.parts[0].triangles, [(0, 1, 2)])

    def test_missing_file_says_to_export(self) -> None:
        """Check that a folder without a package says to export one."""
        directory = PackageDirectory(self).path
        with self.assertRaises(bc.PackageError) as caught:
            _ = bc.read_package(directory)
        self.assertEqual(
            str(caught.exception),
            f"{directory}: has no character.json; export it from Blender",
        )

    def test_other_json_is_not_a_package(self) -> None:
        """Check that JSON of another kind is refused as not a package."""
        message = self.refusal({"format": "something-else"})
        self.assertIn('isn\'t a character package; its "format" should be', message)

    def test_newer_version_is_named(self) -> None:
        """Check that a package of another version says which versions differ."""
        document = minimal_document()
        document["version"] = 2
        self.assertIn(
            "is version 2, but this build reads version 1", self.refusal(document)
        )

    def test_bad_color_names_the_material(self) -> None:
        """Check that a color out of range names its material and the rule."""
        document = minimal_document()
        document["materials"] = [{"name": "skin", "color": [300, 0, 0, 255]}]
        message = self.refusal(document)
        self.assertIn("materials[0] (skin).color", message)
        self.assertIn("four whole numbers from 0 to 255", message)

    def test_missing_image_is_named(self) -> None:
        """Check that a texture missing from the package is named."""
        document = minimal_document()
        document["materials"] = [
            {"name": "skin", "color": [255] * 4, "texture": "skin.png"}
        ]
        self.assertIn("skin.png isn't in the package", self.refusal(document))

    def test_image_outside_package_is_refused(self) -> None:
        """Check that a texture may not be taken from outside the package."""
        document = minimal_document()
        document["materials"] = [
            {"name": "skin", "color": [255] * 4, "texture": "../skin.png"}
        ]
        self.assertIn("should be inside the package", self.refusal(document))

    def test_unknown_material_is_refused(self) -> None:
        """Check that a part naming a material the package lacks is refused."""
        document = minimal_document()
        document["parts"] = [plain_part(material=1)]
        message = self.refusal(document)
        self.assertIn("parts[0]: its material should be an index below 1", message)

    def test_triangle_corner_out_of_range_is_refused(self) -> None:
        """Check that a triangle indexing a missing position is refused."""
        document = minimal_document()
        document["parts"] = [plain_part(triangles=[[0, 1, 3]])]
        self.assertIn(
            "each corner should index one of its 3 positions", self.refusal(document)
        )

    def test_not_json_is_refused(self) -> None:
        """Check that a file that isn't JSON is refused, naming it."""
        directory = PackageDirectory(self).path
        path = directory / bc.PACKAGE_FILE
        _ = path.write_text("{")
        with self.assertRaises(bc.PackageError) as caught:
            _ = bc.read_package(directory)
        self.assertIn(f"{path}: isn't JSON", str(caught.exception))


class CommandLineTest(unittest.TestCase):
    """Check what the command says, as an artist runs it."""

    def test_slot_replaced_twice_is_refused(self) -> None:
        """Check that two packages for one slot are refused, saying which."""
        first = PackageDirectory(self).write(minimal_document())
        second = PackageDirectory(self).write(minimal_document())
        with (
            self.assertRaises(SystemExit) as caught,
            contextlib.redirect_stdout(io.StringIO()),
        ):
            run_main(["--list-files", str(first), str(second)])
        self.assertEqual(
            caught.exception.code,
            (
                f"build_character.py: {second} and {first} both replace charlie, but "
                "a character can be replaced only once. Leave one of them out"
            ),
        )

    def test_building_needs_output_and_header(self) -> None:
        """Check that building without --output says which options to give."""
        package = PackageDirectory(self).write(minimal_document())
        errors = io.StringIO()
        with (
            self.assertRaises(SystemExit) as caught,
            contextlib.redirect_stderr(errors),
        ):
            run_main([str(package)])
        self.assertEqual(caught.exception.code, 2)
        self.assertIn("give --output, --check or --list-files", errors.getvalue())

    def test_list_files_names_every_file(self) -> None:
        """Check that --list-files names the eight files a slot's package replaces."""
        package = PackageDirectory(self).write(minimal_document())
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            run_main(["--list-files", str(package)])
        names = output.getvalue().split()
        self.assertEqual(len(names), 8)
        self.assertIn("charlie_reaction_palette_3", names)


def run_main(arguments: list[str]) -> None:
    """Run build_character.py's command with ARGUMENTS."""
    with unittest.mock.patch("sys.argv", ["build_character.py", *arguments]):
        bc.main()


class TextureTest(unittest.TestCase):
    """Check how images are prepared for the game's textures."""

    def test_over_keeps_soft_edges(self) -> None:
        """Check that drawing over a color blends by alpha and makes it opaque."""
        pixels = np.array(
            [[[255, 0, 0, 255], [255, 0, 0, 0], [255, 0, 0, 51]]], dtype=np.uint16
        )
        drawn = bc.draw_over(pixels, (0, 0, 255))
        expected = [[[255, 0, 0, 255], [0, 0, 255, 255], [51, 0, 204, 255]]]
        self.assertEqual(drawn.tolist(), expected)

    def test_bleed_colors_clear_texels_from_neighbors(self) -> None:
        """Check that clear texels take their opaque neighbors' colors.

        Texel 1 of a row or column lies between texels 0 and 2, and so does
        texel 3, as the texture wraps.
        """
        line = [[200, 100, 0, 255], [0, 0, 0, 0], [0, 100, 200, 255], [0, 0, 0, 0]]
        middle = [100, 100, 100]
        expected = [[200, 100, 0], middle, [0, 100, 200], middle]
        images = {"row": [line], "column": [[texel] for texel in line]}
        for direction, image in images.items():
            with self.subTest(direction=direction):
                pixels = np.array(image, dtype=np.uint16)
                bled = bc.bleed_edges(pixels, pixels[:, :, 3] > 0)
                rows = cast("list[list[list[int]]]", bled.tolist())
                texels = [texel for row in rows for texel in row]
                self.assertEqual([texel[:3] for texel in texels], expected)
                self.assertEqual([texel[3] for texel in texels], [255, 0, 255, 0])

    def test_bleed_leaves_clear_image_alone(self) -> None:
        """Check that an image with no opaque texel comes back as it was."""
        pixels = np.zeros((2, 2, 4), dtype=np.uint16)
        bled = bc.bleed_edges(pixels, pixels[:, :, 3] > 0)
        self.assertEqual(bled.tolist(), pixels.tolist())


def c_string_bytes(literals: str) -> bytes:
    """Return the bytes of C string LITERALS, as a C compiler reads them.

    A hexadecimal escape takes every hex digit after it, as C's does, so
    that a name spelled without a break would show here as a wrong byte.
    Adjacent literals join, as C's do.
    """
    out = bytearray()
    found = cast("list[str]", re.findall(r'"((?:[^"\\]|\\.)*)"', literals))
    for literal in found:
        position = 0
        while position < len(literal):
            if literal.startswith("\\x", position):
                digits = re.match(r"[0-9A-Fa-f]+", literal[position + 2 :])
                if digits is None:
                    raise ValueError(literals)
                out.append(int(digits.group(), 16) % SIGNED_BYTE_RANGE)
                if int(digits.group(), 16) >= SIGNED_BYTE_RANGE:
                    out.append(0)  # Too wide for a char: mark the bytes wrong
                position += 2 + len(digits.group())
            else:
                out += literal[position].encode("ascii")
                position += 1
    return bytes(out)


def menu_bytes(name: str) -> bytes:
    """Return NAME in the menus' font: lowercase letters raised by its offset."""
    return bytes(
        ord(c) + bc.LOWERCASE_CODE_OFFSET if c.islower() else ord(c) for c in name
    )


def header_defines() -> dict[str, str]:
    """Return character_names.h's macros' values, as the original's names."""
    header = (REPOSITORY / "include" / "characters" / "character_names.h").read_text()
    return dict(
        cast("list[tuple[str, str]]", re.findall(r"#\s*define (\w+) (.+)", header))
    )


def named_package(test: unittest.TestCase, name: str, slot: str = SLOT) -> bc.Package:
    """Return a minimal package named NAME that replaces SLOT."""
    document = minimal_document()
    document["name"] = name
    document["slot"] = slot
    return bc.read_package(PackageDirectory(test).write(document))


class NameTest(unittest.TestCase):
    """Check the names that a custom character gives the menus and text."""

    def test_spelling_reads_back_as_the_menu_font(self) -> None:
        """Check that every spelling, read as C reads it, is the name's bytes.

        The names put capitals that are hex digits after lowercase letters,
        whose escapes would take them in without a break.
        """
        names = ["Terry", "DaD", "Ab1", "Mr. T-1", "a", "zZ", "Eve 2", "BaBe-F0"]
        for name in [*names, *bc.SLOT_NAMES.values()]:
            with self.subTest(name=name):
                spelled = bc.menu_spelling(name)
                self.assertEqual(c_string_bytes(spelled), menu_bytes(name))

    def test_spelling_matches_the_originals(self) -> None:
        """Check that the original names spell as the game's menus have them.

        The original menus write a second word lowercase ("Baby mario"),
        which a custom name keeps as its artist wrote it, so for those only
        the bytes after the space differ.
        """
        defines = header_defines()
        lowered_second_word = {"baby_mario", "metal_mario"}
        for slot, name in bc.SLOT_NAMES.items():
            macro = slot.upper()
            with self.subTest(slot=slot):
                self.assertEqual(defines[f"{macro}_NAME"], f'"{name}"')
                contestant = bc.menu_spelling(name.replace(" ", "_"))
                self.assertEqual(defines[f"{macro}_CONTESTANT_NAME"], contestant)
                menu = defines[f"{macro}_MENU_NAME"]
                if slot in lowered_second_word:
                    first, second = name.split(" ")
                    menu_name = f"{first} {second.lower()}"
                    self.assertEqual(c_string_bytes(menu), menu_bytes(menu_name))
                else:
                    self.assertEqual(menu, bc.menu_spelling(name))

    def test_header_renames_the_slot(self) -> None:
        """Check that the names header's copy renames only the replaced slots."""
        output = PackageDirectory(self).path
        packages = [named_package(self, "Mr. Fox"), named_package(self, "Big D", "joe")]
        bc.write_names(packages, REPOSITORY, [], output)
        header = (output / bc.NAMES_HEADER).read_text()
        copied = dict(
            cast("list[tuple[str, str]]", re.findall(r"#define (\w+) (.+)", header))
        )
        original = header_defines()
        defines = {
            name: copied[name] for name in copied if copied[name] != original[name]
        }
        self.assertEqual(set(copied), set(original))
        self.assertEqual(
            defines,
            {
                "CHARLIE_NAME": '"Mr. Fox"',
                "CHARLIE_MENU_NAME": bc.menu_spelling("Mr. Fox"),
                "CHARLIE_CONTESTANT_NAME": bc.menu_spelling("Mr._Fox"),
                "JOE_NAME": '"Big D"',
                "JOE_MENU_NAME": bc.menu_spelling("Big D"),
                "JOE_CONTESTANT_NAME": bc.menu_spelling("Big_D"),
            },
        )

    def test_names_the_menus_cannot_show_are_refused(self) -> None:
        """Check that a name too long, empty or with other letters is refused.

        The longest name fits the game's buffer for it, with its terminator.
        """
        header = (REPOSITORY / "include" / "characters" / "character.h").read_text()
        found = re.search(r"#define IMPORTED_CHARACTER_NAME_LENGTH (\d+)", header)
        self.assertIsNotNone(found)
        longest = "A" * (int(found.group(1)) - 1 if found else 0)
        self.assertEqual(named_package(self, longest).name, longest)
        self.assertEqual(named_package(self, "Mr. T-1").name, "Mr. T-1")
        for name in ["", longest + "A", "Terry!", "T\u00e9rry", "Terry_Fox"]:
            with self.subTest(name=name), self.assertRaises(bc.PackageError) as caught:
                _ = named_package(self, name)
            self.assertIn(f'"{name}"', str(caught.exception))

    def test_name_defaults_to_the_slots(self) -> None:
        """Check that a package without a name keeps the slot's."""
        document = minimal_document()
        del document["name"]
        document["slot"] = "donkey_kong"
        package = bc.read_package(PackageDirectory(self).write(document))
        self.assertEqual(package.name, "DK")

    def test_text_renames_whole_lines_only(self) -> None:
        """Check that only lines that are the name, or its tournament, change."""
        text = "Charlie\nCharlie Tournament\r\nCharlie's ball\n Charlie\nPlum\nCharlie"
        renamed = bc.rename_text(text, [named_package(self, "Terry")])
        expected = "Terry\nTerry Tournament\r\nCharlie's ball\n Charlie\nPlum\nTerry"
        self.assertEqual(renamed, expected)

    def test_text_path_must_be_relative(self) -> None:
        """Check that a text file outside the repository is refused."""
        output = PackageDirectory(self).path
        text = output / "messages.txt"
        _ = text.write_text("Charlie\n")
        for path in [text, Path("../messages.txt")]:
            with self.subTest(path=path), self.assertRaises(bc.BuilderBugError):
                bc.write_names(
                    [named_package(self, "Terry")], REPOSITORY, [path], output
                )
        self.assertEqual(text.read_text(), "Charlie\n")


# A header whose definitions DefineTest replaces
DEFINES_HEADER = (
    "#ifndef H\n#define H\n"
    "#define ONE 1\n"
    "#define LONG \\\n    { 1, \\\n      2 }, \\\n    3\n"
    "#define ONE_MORE 4\n"
    "#endif\n"
)


class DefineTest(unittest.TestCase):
    """Check how a header's copy takes the definitions that replace its own."""

    def test_replaces_whole_definitions_only(self) -> None:
        """Check that a definition and its continued lines are replaced, not others.

        ONE is a prefix of ONE_MORE, which must keep its own definition.
        """
        replaced = bc.replace_defines(
            DEFINES_HEADER,
            {"ONE": "#define ONE 9", "LONG": "#define LONG 5"},
            Path("h.h"),
        )
        expected = (
            "#ifndef H\n#define H\n#define ONE 9\n#define LONG 5\n"
            "#define ONE_MORE 4\n#endif\n"
        )
        self.assertEqual(replaced, expected)

    def test_unknown_macro_is_a_bug(self) -> None:
        """Check that a macro the header lacks is reported, not added."""
        with self.assertRaises(bc.BuilderBugError) as caught:
            _ = bc.replace_defines(
                DEFINES_HEADER, {"TWO": "#define TWO 2"}, Path("h.h")
            )
        self.assertTrue(str(caught.exception).startswith("h.h doesn't define TWO."))

    def test_layouts_header_takes_every_slots_layout(self) -> None:
        """Check that each model file's layout macro is one the header defines.

        The table reads the header's own layouts back as the originals'.
        """
        repository_layouts = REPOSITORY / bc.LAYOUTS_HEADER
        table = fmt.read_model_table(
            REPOSITORY / "src" / "characters" / "character_model.c", repository_layouts
        )
        header = repository_layouts.read_text()
        for slot in bc.SLOT_NAMES:
            for names in bc.model_file_names(slot).values():
                defines = {name.upper() + bc.LAYOUT_SUFFIX: "" for name in names}
                _ = bc.replace_defines(header, defines, repository_layouts)
        self.assertEqual(len(table), 144)


if __name__ == "__main__":
    _ = unittest.main()
