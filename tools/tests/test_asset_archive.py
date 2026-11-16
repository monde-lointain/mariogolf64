"""Test the asset archive's encoder and packing, which custom face sprites use.

The encoder's oracle is the decoder, which follows the game's DecodeByteRuns
and DecodeHalfwordRuns: whatever the encoder writes must decode to what it
was given. The game's own run-encoded assets are the samples that matter
most, and the decoder must read each of them as the size its header gives.
"""

import contextlib
import io
import random
import struct
import tempfile
import unittest
from pathlib import Path

import asset_archive
import decode_asset

REPOSITORY = Path(__file__).resolve().parents[2]
ASSETS = REPOSITORY / "assets" / "asset_archive"
NEEDS_GAME_FILES = "needs the game's files: run make extract"
SEED = 64
LONGEST = decode_asset.RUN_LENGTH_MAX
HALFWORD_RUNS = 2
# Fewer of the game's assets than this decoding as runs means the test
# isn't finding them
ENCODED_ASSETS_MIN = 500


def _random_bytes(size: int, values: int = 256) -> bytes:
    """Return SIZE bytes below VALUES that are the same in every run."""
    generator = random.Random(SEED)  # noqa: S311 - test data, not secrets
    return bytes(generator.randrange(values) for _ in range(size))


def _samples() -> dict[str, bytes]:
    """Return data that stresses the encoder's runs and copies, by name."""
    return {
        "empty": b"",
        "one byte": b"\x07",
        "two the same": b"\x07\x07",
        "longest run": b"\x01" * LONGEST,
        "one past the longest run": b"\x01" * (LONGEST + 1),
        "many runs": b"\x01" * (LONGEST * 3 + 5),
        "longest copy": bytes(range(LONGEST)),
        "one past the longest copy": bytes(range(LONGEST + 1)),
        "copy, run, copy": bytes(range(10)) + b"\xff" * 9 + bytes(range(5)),
        "zeros": bytes(1000),
        "noise": _random_bytes(3000),
        "few values": _random_bytes(3000, 3),
    }


def _encoded_assets() -> dict[str, tuple[int, bytes, bytes]]:
    """Return the game's run-encoded assets: each unit, decoded and as stored."""
    found: dict[str, tuple[int, bytes, bytes]] = {}
    for path in sorted(ASSETS.glob("*.bin")):
        stored = path.read_bytes()
        word = int.from_bytes(stored[: decode_asset.ENCODING_HEADER_SIZE], "big")
        method = word >> decode_asset.METHOD_SHIFT
        size = word & decode_asset.DECODED_SIZE_MASK
        if method not in {decode_asset.ENCODING_BYTE_RUNS, HALFWORD_RUNS}:
            continue
        unit = 1 if method == decode_asset.ENCODING_BYTE_RUNS else HALFWORD_RUNS
        try:
            decoded = decode_asset.decode_encoded(stored)
        except (IndexError, ValueError, struct.error):  # Not runs, but begins so
            continue
        if len(decoded) == size:
            found[path.name] = (unit, decoded, stored)
    return found


class EncodeRunsTest(unittest.TestCase):
    """Check that the encoder writes runs that the game's decoder reads back."""

    def test_round_trip(self) -> None:
        """Check that every sample decodes to itself, in bytes and halfwords."""
        for name, data in _samples().items():
            for unit in (1, HALFWORD_RUNS):
                whole = data[: len(data) - len(data) % unit]
                with self.subTest(name=name, unit=unit):
                    encoded = decode_asset.encode_runs(whole, unit)
                    self.assertEqual(decode_asset.decode_runs(encoded, unit), whole)

    def test_runs_are_short(self) -> None:
        """Check that a run takes two bytes, and copying costs a byte in 127."""
        self.assertEqual(len(decode_asset.encode_runs(bytes(1000), 1)), 2 * 8 + 1)
        # No byte repeats the one before, so nothing is a run
        noise = bytes(i * 101 % 251 for i in range(LONGEST * 4))
        self.assertLessEqual(
            len(decode_asset.encode_runs(noise, 1)), len(noise) + 4 + 1
        )

    def test_asset_header_gives_size_and_method(self) -> None:
        """Check that an encoded asset decodes, as ReadEncodedAsset reads it."""
        data = _random_bytes(777, 4)
        encoded = decode_asset.encode_asset(data)
        self.assertEqual(decode_asset.decode_encoded(encoded), data)
        word = int.from_bytes(encoded[: decode_asset.ENCODING_HEADER_SIZE], "big")
        self.assertEqual(word >> decode_asset.METHOD_SHIFT, 1)
        self.assertEqual(word & decode_asset.DECODED_SIZE_MASK, len(data))

    @unittest.skipUnless(ASSETS.is_dir(), NEEDS_GAME_FILES)
    def test_game_assets_round_trip(self) -> None:
        """Check that the game's run-encoded assets encode and decode back.

        The game's assets come from more than one encoder, so their bytes
        aren't the oracle, but in all they mustn't take less room than ours.
        """
        found = _encoded_assets()
        self.assertGreater(len(found), ENCODED_ASSETS_MIN)
        ours = theirs = 0
        for name, (unit, decoded, stored) in found.items():
            with self.subTest(asset=name):
                body = decode_asset.encode_runs(decoded, unit)
                self.assertEqual(decode_asset.decode_runs(body, unit), decoded)
            ours += decode_asset.ENCODING_HEADER_SIZE + len(body)
            theirs += len(stored)
        self.assertLessEqual(ours, theirs)


class ArchiveTest(unittest.TestCase):
    """Check that packing puts replacements in their assets' places."""

    def directories(self) -> tuple[Path, Path]:
        """Return an archive's directory of three assets, and one for replacements."""
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        assets = Path(temporary.name) / "assets"
        replacements = Path(temporary.name) / "replacements"
        assets.mkdir()
        replacements.mkdir()
        for asset in range(3):
            _ = (assets / f"{asset:03X}.bin").write_bytes(bytes([asset]) * 5)
        return assets, replacements

    def refusal(self, assets: Path, replacements: Path | None = None) -> str:
        """Return what reading the assets says as it exits."""
        stderr = io.StringIO()
        with (
            self.assertRaises(SystemExit) as caught,
            contextlib.redirect_stderr(stderr),
        ):
            _ = asset_archive.read_assets(assets, replacements)
        return str(caught.exception.code)

    def test_split_reads_back_what_was_packed(self) -> None:
        """Check that an archive splits into the assets it was packed from."""
        directory, _ = self.directories()
        assets = asset_archive.read_assets(directory)
        split = directory.parent / "split"
        asset_archive.split(asset_archive.pack(assets), split)
        self.assertEqual(asset_archive.read_assets(split), assets)

    def test_replacement_takes_its_assets_place(self) -> None:
        """Check that a replacement is packed in place of its asset alone."""
        directory, replacements = self.directories()
        _ = (replacements / "001.bin").write_bytes(b"new")
        assets = asset_archive.read_assets(directory, replacements)
        self.assertEqual(assets, [bytes(5), b"new", b"\x02" * 5])

    def test_replacement_of_no_asset_is_refused(self) -> None:
        """Check that a replacement for an asset the archive lacks is named."""
        directory, replacements = self.directories()
        stray = replacements / "0FF.bin"
        _ = stray.write_bytes(b"new")
        self.assertIn(str(stray), self.refusal(directory, replacements))

    def test_missing_asset_is_named(self) -> None:
        """Check that a gap in the assets' IDs is named, not packed shifted."""
        directory, _ = self.directories()
        (directory / "001.bin").unlink()
        self.assertIn("expected asset 1", self.refusal(directory))


if __name__ == "__main__":
    _ = unittest.main()
