"""Test lz_codec.py, which compresses the game's LZ files and their patches.

The oracle is the decoder, which follows the game's DecodeLzChunk and
DecodePatchedLzChunk: whatever the encoder writes must decode to what it was
given. The decoder itself is checked against the game's own files, whose
decompressed contents the template tool reads as models.
"""

import random
import struct
import unittest
from pathlib import Path

import lz_codec

REPOSITORY = Path(__file__).resolve().parents[2]
MODELS = REPOSITORY / "assets" / "character_models"
NEEDS_GAME_FILES = "needs the game's files: run make extract"
SEED = 64
# A run longer than one reference can copy, and one farther back than one
# can reach, so that the encoder must split or skip them
LONG_RUN = lz_codec.LENGTH_MAX * 3 + 1
FAR = lz_codec.DISTANCE_MAX + 5
# Repeats of ten bytes that take more than 64 KiB, one header word's worth
BEYOND_ONE_HEADER_WORD = 7000
# The original's files compress this much smaller than our encoder's, at most
ORIGINAL_SIZE_SLACK = 1.02


def _random_bytes(size: int) -> bytes:
    """Return SIZE bytes that are the same in every run."""
    return random.Random(SEED).randbytes(size)  # noqa: S311 - test data, not secrets


def _samples() -> dict[str, bytes]:
    """Return data that stresses the encoder's choices, by name."""
    noise = _random_bytes(4000)
    return {
        "empty": b"",
        "one word": b"\x12\x34",
        "noise": noise,
        "one word repeated": b"\xab\xcd" * LONG_RUN,
        "a run repeated": _random_bytes(10) * LONG_RUN,
        "a run repeated far back": noise[:40] + _random_bytes(FAR * 2) + noise[:40],
        "the end marker's word": b"\x00\x00" * 40,
    }


class EncodeTest(unittest.TestCase):
    """Check that encoded data decodes to what was encoded."""

    def test_round_trip(self) -> None:
        """Check that each sample decodes to itself after encoding."""
        for name, data in _samples().items():
            with self.subTest(sample=name):
                self.assertEqual(lz_codec.decode(lz_codec.encode(data)), data)

    def test_header_holds_size(self) -> None:
        """Check that the header holds the decompressed size, as the game reads it.

        The size takes both header words, as the models are over 64 KiB.
        """
        data = _random_bytes(10) * BEYOND_ONE_HEADER_WORD
        (size,) = struct.unpack(">I", lz_codec.encode(data)[: lz_codec.HEADER_SIZE])
        self.assertEqual(size, len(data))

    def test_repeats_are_compressed(self) -> None:
        """Check that repeated runs are sent as references, not literals."""
        data = _random_bytes(10) * LONG_RUN
        self.assertLess(len(lz_codec.encode(data)), len(data) // 4)

    def test_odd_size_is_refused(self) -> None:
        """Check that data that isn't whole words is refused."""
        with self.assertRaisesRegex(lz_codec.LzError, "its size, 3, is odd"):
            _ = lz_codec.encode(b"abc")

    def test_truncated_file_is_refused(self) -> None:
        """Check that a file cut short is refused rather than read past."""
        encoded = lz_codec.encode(_random_bytes(100))
        with self.assertRaisesRegex(lz_codec.LzError, "ends before its end marker"):
            _ = lz_codec.decode(encoded[:-4])


class PatchTest(unittest.TestCase):
    """Check that patches turn their base into their target."""

    def test_round_trip(self) -> None:
        """Check that each patch, applied to its base, makes its target."""
        base = _random_bytes(2000)
        changed_start = b"\xff\xff" * 3 + base[6:]
        changed_end = base[:-6] + b"\xff\xff" * 3
        long_change = base[:100] + _random_bytes(1200)[::-1] + base[1300:]
        scattered = bytearray(base)
        for position in range(0, len(base), 50):
            scattered[position] ^= 0xFF
        targets = {
            "unchanged": base,
            "changed at the start": changed_start,
            "changed at the end": changed_end,
            "a change longer than a command": long_change,
            "scattered changes": bytes(scattered),
        }
        for name, target in targets.items():
            with self.subTest(target=name):
                patch = lz_codec.encode_patch(base, target)
                self.assertEqual(lz_codec.apply_patch(base, patch), target)

    def test_other_size_is_refused(self) -> None:
        """Check that a patch can't change its base's size, as the game can't."""
        with self.assertRaisesRegex(lz_codec.LzError, "its base's size, 4 bytes"):
            _ = lz_codec.encode_patch(b"abcd", b"abcdef")


@unittest.skipUnless((MODELS / "charlie.bin").is_file(), NEEDS_GAME_FILES)
class GameFileTest(unittest.TestCase):
    """Check the codec against the game's own files."""

    def test_reencoded_models_decode_the_same(self) -> None:
        """Check that a model re-encoded decodes the same, and is no bigger."""
        for name in ("charlie", "charlie_reaction"):
            with self.subTest(model=name):
                original = (MODELS / f"{name}.bin").read_bytes()
                data = lz_codec.decode(original)
                encoded = lz_codec.encode(data)
                self.assertEqual(lz_codec.decode(encoded), data)
                self.assertLessEqual(len(encoded), len(original) * ORIGINAL_SIZE_SLACK)

    def test_reencoded_palette_patches_the_same(self) -> None:
        """Check that a palette's patch, re-encoded, makes the same palette."""
        base = lz_codec.decode((MODELS / "charlie.bin").read_bytes())
        for palette in range(1, 4):
            with self.subTest(palette=palette):
                original = (MODELS / f"charlie_palette_{palette}.bin").read_bytes()
                target = lz_codec.apply_patch(base, original)
                self.assertEqual(len(target), len(base))
                self.assertNotEqual(target, base)
                patch = lz_codec.encode_patch(base, target)
                self.assertEqual(lz_codec.apply_patch(base, patch), target)


if __name__ == "__main__":
    _ = unittest.main()
