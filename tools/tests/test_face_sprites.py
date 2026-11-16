"""Test face_sprites.py, which draws custom characters' menu and scorecard faces.

The strongest check draws Charlie from his own model, with the cameras and
poses fitted to his original pictures: the head must land where the
original's is, and the outline must cover what the original's covers. The
game's files are needed for that, so those tests are skipped until "make
extract" has run. The indexing is checked by reading its pictures back.
"""

import functools
import tempfile
import unittest
from pathlib import Path
from typing import cast

import numpy as np
import numpy.typing as npt
from PIL import Image

# The tests run from their own directory, which unittest's discovery puts on
# the path
from test_build_character import (  # pyright: ignore[reportImplicitRelativeImport]
    write_template_package,
)

import build_character as bc
import character_template as tmpl
import decode_asset
import face_sprites as fs

REPOSITORY = Path(__file__).resolve().parents[2]
ASSETS = REPOSITORY / "assets" / "asset_archive"
HAVE_GAME_FILES = (REPOSITORY / "assets" / "character_models" / "charlie.bin").is_file()
NEEDS_GAME_FILES = "needs the game's files: run make extract"
SLOT = "charlie"
SEED = 64
RGBA16_COLORS = 1 << 16
# Charlie drawn from his own model is framed as his original, to within
# these
ZOOM_TOLERANCE = 1e-3
SHIFT_TOLERANCE = 0.01
# Of the pixels that his drawing or the original's covers, at least this
# share both cover: the fitted poses' error, and his drawn face's
OVERLAP_MIN = 0.8
# How far, on average, his colors may lie from the original's, out of 255:
# the light is fitted, and the originals were drawn by other means
COLOR_ERROR_MAX = 40
# Colors a test sheet uses, fewer than the palette's free entries
TEST_COLORS = 40

ByteArray = npt.NDArray[np.uint8]
FloatArray = npt.NDArray[np.float64]


def _sheet_size(sprites: fs.SpriteSet) -> tuple[int, int, int]:
    return (fs.OUTFIT_COUNT * fs.HEIGHT, len(sprites.pictures) * fs.WIDTH, fs.RGBA)


def _test_sheet(sprites: fs.SpriteSet) -> ByteArray:
    """Return a sheet of few colors, each exact in RGBA16, shared by the outfits.

    A pixel's index gives its color in every outfit, so each outfit's color
    follows from the first's; edge and solid pixels have their own colors.
    """
    rng = np.random.default_rng(SEED)
    levels = rng.integers(0, fs.CHANNEL_LEVELS + 1, (TEST_COLORS, fs.OUTFIT_COUNT, 3))
    # Widened as the RDP widens RGBA16's channels
    colors = (levels << 3 | levels >> 2).astype(np.uint8)
    rows, columns, _ = _sheet_size(sprites)
    choice = rng.integers(0, TEST_COLORS, (fs.HEIGHT, columns))
    kind = rng.integers(0, 3, (fs.HEIGHT, columns))  # Clear, edge, solid
    # Edge pixels take the first half of the colors, solid ones the rest
    choice = choice // 2 + np.not_equal(kind, 1) * (TEST_COLORS // 2)
    sheet = np.zeros((rows, columns, fs.RGBA), np.uint8)
    for outfit in range(fs.OUTFIT_COUNT):
        rows_of = slice(outfit * fs.HEIGHT, (outfit + 1) * fs.HEIGHT)
        sheet[rows_of, :, :3] = colors[choice, outfit]
        sheet[rows_of, :, 3] = np.array([0, fs.EDGE_ALPHA, fs.OPAQUE])[kind]
    # Below a shorter picture, such as the menu's, is nothing
    for column, picture in enumerate(sprites.pictures):
        for outfit in range(fs.OUTFIT_COUNT):
            top = outfit * fs.HEIGHT
            below = slice(top + picture.height, top + fs.HEIGHT)
            sheet[below, column * fs.WIDTH : (column + 1) * fs.WIDTH] = 0
    sheet[np.equal(sheet[..., 3], 0)] = 0
    return sheet


def _original_palettes() -> list[list[int]]:
    """Return palettes with every entry set, as the originals' stand-in."""
    return [
        [
            (outfit * fs.PALETTE_SIZE + entry) % RGBA16_COLORS
            for entry in range(fs.PALETTE_SIZE)
        ]
        for outfit in range(fs.OUTFIT_COUNT)
    ]


class IndexTest(unittest.TestCase):
    """Check that a sheet's colors come back from its palettes and indices."""

    sprites: fs.SpriteSet = fs.load_sprite_sets()[SLOT]

    def test_rgba16_round_trip(self) -> None:
        """Check that every RGBA16 color reads back as itself."""
        for color in range(RGBA16_COLORS):
            red, green, blue, opaque = fs.from_rgba16(color)
            levels = [
                v * fs.CHANNEL_LEVELS / fs.CHANNEL_MAX for v in (red, green, blue)
            ]
            self.assertEqual(fs.to_rgba16(levels, opaque=opaque), color)

    def test_rgba16_reads_as_the_textures_do(self) -> None:
        """Check that RGBA16 colors read as decode_asset.py reads textures'."""
        for color in range(RGBA16_COLORS):
            red, green, blue, opaque = fs.from_rgba16(color)
            pixel = decode_asset.convert_rgba16(color)
            self.assertEqual((red, green, blue), pixel[:3])
            self.assertEqual(opaque, pixel[3] == fs.OPAQUE)

    def test_few_colors_index_exactly(self) -> None:
        """Check that a sheet of fewer colors than entries reads back exactly."""
        sheet = _test_sheet(self.sprites)
        faces = fs.index_sheet(sheet, self.sprites, _original_palettes())
        shown = fs.faces_sheet(self.sprites, faces)
        covered = sheet[..., 3] > 0
        self.assertEqual(shown[..., 3].tolist(), sheet[..., 3].tolist())
        self.assertEqual(shown[covered].tolist(), sheet[covered].tolist())

    def test_kept_entries_and_alpha_bits(self) -> None:
        """Check that each palette keeps the original's clear entries.

        Edge pixels' entries are clear, as the original's are, and the
        character's opaque; the background is the background entry.
        """
        originals = _original_palettes()
        sheet = _test_sheet(self.sprites)
        faces = fs.index_sheet(sheet, self.sprites, originals)
        for outfit, palette in enumerate(faces.palettes):
            for kept in fs.KEPT_INDICES:
                self.assertEqual(palette[kept], originals[outfit][kept])
        alpha = sheet[: fs.HEIGHT, :, 3]
        for column, picture in enumerate(faces.pictures):
            seen = alpha[: len(picture), column * fs.WIDTH : (column + 1) * fs.WIDTH]
            drawn_alpha = cast("list[list[int]]", seen.tolist())
            indices = cast("list[list[int]]", picture.tolist())
            for drawn, index in zip(
                (a for row in drawn_alpha for a in row),
                (i for row in indices for i in row),
                strict=True,
            ):
                bit = faces.palettes[0][index] & fs.OPAQUE_BIT
                if drawn == 0:
                    self.assertEqual(index, fs.BACKGROUND_INDEX)
                else:
                    self.assertNotIn(index, fs.KEPT_INDICES)
                    self.assertEqual(bit, int(drawn == fs.OPAQUE))

    def test_many_colors_stay_near(self) -> None:
        """Check that a sheet of more colors than entries is near each pixel."""
        rng = np.random.default_rng(SEED)
        rows, columns, _ = _sheet_size(self.sprites)
        sheet = np.full((rows, columns, fs.RGBA), fs.OPAQUE, np.uint8)
        # A smooth ramp, as a shaded character is, in each outfit
        ramp = np.arange(columns) * fs.CHANNEL_MAX // columns
        sheet[..., 0] = ramp[None, :]
        sheet[..., 1] = rng.integers(100, 110, (rows, columns))
        sheet[..., 2] = np.arange(rows)[:, None] * fs.CHANNEL_MAX // rows
        faces = fs.index_sheet(sheet, self.sprites, _original_palettes())
        shown = fs.faces_sheet(self.sprites, faces).astype(int)
        covered = shown[..., 3] > 0
        error = np.abs(shown[..., :3] - sheet[..., :3].astype(int))[covered]
        self.assertLess(float(error.mean()), 6)

    def test_assets_decode_to_the_pictures(self) -> None:
        """Check that the asset files hold the palettes and pictures."""
        faces = fs.index_sheet(
            _test_sheet(self.sprites), self.sprites, _original_palettes()
        )
        files = fs.asset_files(self.sprites, faces)
        for outfit, palette in enumerate(faces.palettes):
            data = files[self.sprites.first_palette + outfit]
            self.assertEqual(len(data), fs.PALETTE_SIZE * fs.PALETTE_ENTRY.size)
            self.assertEqual(fs.read_palette(data), palette)
        for picture, indices in zip(self.sprites.pictures, faces.pictures, strict=True):
            decoded = decode_asset.decode_encoded(files[picture.asset])
            self.assertEqual(decoded, indices.tobytes())
            self.assertEqual(len(decoded), picture.height * fs.WIDTH)


class FramingTest(unittest.TestCase):
    """Check that a character's head is fitted into the original's box."""

    def test_head_fits_inside_and_centered(self) -> None:
        """Check that a head of any shape fills the box one way, centered."""
        want = (2.0, 3.0, 12.0, 23.0)  # 10 wide, 20 tall
        heads = {
            "tall": (5.0, 5.0, 7.0, 13.0),
            "wide": (-4.0, 1.0, 12.0, 5.0),
            "same shape": (0.0, 0.0, 1.0, 2.0),
        }
        center = (16.0, 16.0)
        for name, have in heads.items():
            zoom, shift = fs.framing(want, have, center)
            fitted = [
                (have[i] - center[i % 2]) * zoom + center[i % 2] + shift[i % 2]
                for i in range(4)
            ]
            with self.subTest(head=name):
                self.assertGreaterEqual(fitted[0], want[0] - 1e-9)
                self.assertGreaterEqual(fitted[1], want[1] - 1e-9)
                self.assertLessEqual(fitted[2], want[2] + 1e-9)
                self.assertLessEqual(fitted[3], want[3] + 1e-9)
                filled = [fitted[2] - fitted[0] - 10, fitted[3] - fitted[1] - 20]
                self.assertAlmostEqual(max(filled), 0)
                self.assertAlmostEqual(fitted[0] + fitted[2], want[0] + want[2])
                self.assertAlmostEqual(fitted[1] + fitted[3], want[1] + want[3])


class SheetFileTest(unittest.TestCase):
    """Check an artist's painted sheet."""

    sprites: fs.SpriteSet = fs.load_sprite_sets()[SLOT]

    def directory(self) -> Path:
        """Return a directory for the sheets, which goes when the test ends."""
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        return Path(temporary.name)

    def test_sheet_reads_back(self) -> None:
        """Check that a sheet saved as PNG reads back as it was."""
        sheet = _test_sheet(self.sprites)
        path = self.directory() / fs.SHEET_FILE
        Image.fromarray(sheet).save(path)
        self.assertEqual(fs.read_sheet(path, self.sprites).tolist(), sheet.tolist())

    def test_wrong_size_says_the_right_one(self) -> None:
        """Check that a sheet of the wrong size gives the size it should be."""
        rows, columns, _ = _sheet_size(self.sprites)
        path = self.directory() / fs.SHEET_FILE
        Image.new("RGBA", (columns, rows - 1)).save(path)
        with self.assertRaises(fs.SpriteError) as caught:
            _ = fs.read_sheet(path, self.sprites)
        self.assertIn(f"{columns}x{rows}", str(caught.exception))
        self.assertIn(str(path), str(caught.exception))


@functools.cache
def charlie() -> tuple[tmpl.CharacterTemplate, bc.Package, Path]:
    """Return Charlie's template, and his main model as a package in a directory."""
    template = tmpl.load_template(REPOSITORY, SLOT)
    directory = Path(tempfile.mkdtemp())
    write_template_package(template, directory)
    return template, bc.read_package(directory), directory


def tearDownModule() -> None:
    """Remove Charlie's package, if a test wrote it."""
    if charlie.cache_info().currsize:
        _, _, directory = charlie()
        for path in directory.iterdir():
            path.unlink()
        directory.rmdir()


def original_picture(
    sprites: fs.SpriteSet, picture: fs.Picture
) -> tuple[npt.NDArray[np.bool_], npt.NDArray[np.int64]]:
    """Return which of an original picture's pixels are opaque, and their colors."""
    palette = fs.read_palette(
        (ASSETS / fs.asset_name(sprites.first_palette)).read_bytes()
    )
    opaque = np.array([color & fs.OPAQUE_BIT for color in palette], np.bool_)
    colors = np.array([fs.from_rgba16(color)[:3] for color in palette], np.int64)
    data = decode_asset.decode_encoded(
        (ASSETS / fs.asset_name(picture.asset)).read_bytes()
    )
    indices = np.array(list(data[: picture.height * fs.WIDTH]), np.int64)
    pixels = indices.reshape(picture.height, fs.WIDTH)  # pyright: ignore[reportUnknownMemberType]
    return opaque[pixels], colors[pixels]


def _moved(points: FloatArray, turn: FloatArray, offset: FloatArray) -> FloatArray:
    """Return POINTS turned by TURN, then moved by OFFSET."""
    turned = points @ turn.T  # pyright: ignore[reportUnknownMemberType, reportUnknownVariableType]
    return cast("FloatArray", turned) + offset


@unittest.skipUnless(HAVE_GAME_FILES, NEEDS_GAME_FILES)
class DrawCharlieTest(unittest.TestCase):
    """Draw Charlie from his own model, as the originals were fitted to."""

    sprites: fs.SpriteSet = fs.load_sprite_sets()[SLOT]

    def test_head_is_framed_as_the_originals(self) -> None:
        """Check that his head, framed to the original's box, needs no change."""
        template, package, _ = charlie()
        heads = fs.bone_heads(template, package.skeleton)
        points = np.concatenate(  # pyright: ignore[reportUnknownMemberType]
            [np.array(p.positions) for p in package.parts if p.bone == fs.HEAD_BONE]
        )
        for name, want in self.sprites.head_boxes.items():
            picture = next(p for p in self.sprites.pictures if p.name == name)
            turn, offset = fs.posed_bones(template, heads, picture.pose)[fs.HEAD_BONE]
            have = fs.head_box(_moved(points, turn, offset), picture.camera)
            zoom, shift = fs.framing(want, have, picture.camera.center)
            with self.subTest(framing=name):
                self.assertAlmostEqual(zoom, 1, delta=ZOOM_TOLERANCE)
                self.assertLess(max(abs(shift[0]), abs(shift[1])), SHIFT_TOLERANCE)

    def test_drawing_covers_the_originals(self) -> None:
        """Check that each drawn picture covers what the original's covers.

        Where both cover, the colors must be near, but not in the beaten
        pictures: his face images fit his main model's face, not his
        reaction model's, which the beaten pictures show.
        """
        template, package, _ = charlie()
        sheet = fs.draw_sheet(package, template, self.sprites)
        for column, picture in enumerate(self.sprites.pictures):
            original, colors = original_picture(self.sprites, picture)
            seen = sheet[: picture.height, column * fs.WIDTH : (column + 1) * fs.WIDTH]
            drawn: npt.NDArray[np.bool_] = np.equal(seen[..., 3], fs.OPAQUE)
            both = original & drawn
            overlap = int(both.sum()) / int((original | drawn).sum())
            error = np.abs(seen[..., :3].astype(np.int64) - colors)[both]
            with self.subTest(picture=picture.name):
                self.assertGreater(overlap, OVERLAP_MIN)
                if not picture.name.startswith("beaten"):
                    self.assertLess(float(error.mean()), COLOR_ERROR_MAX)

    def test_painted_sheet_takes_the_drawings_place(self) -> None:
        """Check that a package's own face_sprites.png is used as it is."""
        template, package, directory = charlie()
        sheet = _test_sheet(self.sprites)
        painted = directory / fs.SHEET_FILE
        Image.fromarray(sheet).save(painted)
        try:
            used = fs.sheet_for(package, template, self.sprites)
        finally:
            painted.unlink()
        self.assertEqual(used.tolist(), sheet.tolist())

    def test_files_replace_the_slots_assets(self) -> None:
        """Check that the written files are the slot's assets and a sheet."""
        template, package, _ = charlie()
        with tempfile.TemporaryDirectory() as output:
            written = fs.write_face_sprites(package, template, ASSETS, Path(output))
            names = sorted(path.name for path in written)
        first = self.sprites.first_palette
        assets = [first + outfit for outfit in range(fs.OUTFIT_COUNT)]
        assets += [picture.asset for picture in self.sprites.pictures]
        expected = [fs.asset_name(asset) for asset in assets]
        self.assertEqual(names, sorted([*expected, f"{SLOT}_{fs.SHEET_FILE}"]))


if __name__ == "__main__":
    _ = unittest.main()
