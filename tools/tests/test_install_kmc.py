"""Test the KMC installer's choice of archives, its checks and its unpacking.

Nothing here downloads: each test hands install() archives built in memory,
with their SHA-256 pinned as a release's would be.
"""

import hashlib
import io
import stat
import tarfile
import tempfile
import unittest
from collections.abc import Callable
from pathlib import Path

import install_kmc


def make_archive(entries: dict[str, bytes]) -> bytes:
    """Return a .tar.gz holding each entry as a plain file."""
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w:gz") as tar:
        for name, contents in entries.items():
            info = tarfile.TarInfo(name)
            info.size = len(contents)
            tar.addfile(info, io.BytesIO(contents))
    return buffer.getvalue()


def make_special_archive(name: str, kind: bytes, target: str = "") -> bytes:
    """Return a .tar.gz holding one member of another kind, such as a link."""
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w:gz") as tar:
        info = tarfile.TarInfo(name)
        info.type = kind  # noqa: V101 - tarfile reads it
        info.linkname = target
        tar.addfile(info)
    return buffer.getvalue()


def make_tree_archive(build: Callable[[Path], None], names: list[str]) -> bytes:
    """Return a .tar.gz of the named entries of a tree, as tar archives them."""
    buffer = io.BytesIO()
    with (
        tempfile.TemporaryDirectory() as root,
        tarfile.open(fileobj=buffer, mode="w:gz") as tar,
    ):
        build(Path(root))
        for name in names:
            tar.add(Path(root) / name, arcname=name, recursive=False)
    return buffer.getvalue()


def hard_link_cccp_to_cpp(root: Path) -> None:
    """Lay out cpp and cccp as the compiler's archive has them: one file, two names."""
    _ = (root / "cpp").write_bytes(b"preprocessor")
    (root / "cccp").hardlink_to(root / "cpp")


def pinned(url: str, data: bytes) -> install_kmc.Archive:
    """Return an archive whose pinned hash is the data's own."""
    return install_kmc.Archive(url, hashlib.sha256(data).hexdigest())


class ArchivesForTest(unittest.TestCase):
    """Each supported host gets its own archives; others get a reason."""

    def test_linux_x86_64_gets_the_linux_builds(self) -> None:
        """Check that Linux on x86-64 gets the Linux compiler and assembler."""
        compiler, assembler = install_kmc.archives_for("Linux", "x86_64")
        self.assertTrue(compiler.url.endswith("gcc-2.7.2-linux.tar.gz"))
        self.assertTrue(assembler.url.endswith("binutils-2.6-linux.tar.gz"))

    def test_apple_silicon_gets_the_arm_compiler(self) -> None:
        """Check that Apple silicon gets the Arm compiler and the Mac assembler."""
        compiler, assembler = install_kmc.archives_for("Darwin", "arm64")
        self.assertTrue(compiler.url.endswith("gcc-2.7.2-mac-arm.tar.gz"))
        self.assertTrue(assembler.url.endswith("binutils-2.6-mac.tar.gz"))

    def test_other_hosts_are_told_what_still_works(self) -> None:
        """Check that other hosts are pointed at the builds without KMC GCC."""
        for system, machine in [
            ("Linux", "aarch64"),
            ("Darwin", "x86_64"),
            ("Windows", "AMD64"),
        ]:
            with self.subTest(system=system, machine=machine):
                with self.assertRaises(install_kmc.InstallError) as raised:
                    _ = install_kmc.archives_for(system, machine)
                self.assertIn("MODERN=1", str(raised.exception))

    def test_every_pin_is_a_sha256_over_https(self) -> None:
        """Check that each archive comes over HTTPS, pinned to a full SHA-256."""
        for archives in install_kmc.ARCHIVES.values():
            for archive in archives:
                with self.subTest(url=archive.url):
                    self.assertTrue(archive.url.startswith("https://"))
                    self.assertEqual(
                        len(bytes.fromhex(archive.sha256)), hashlib.sha256().digest_size
                    )


def install_from_memory(
    directory: Path, archives: dict[str, bytes], pins: tuple[install_kmc.Archive, ...]
) -> None:
    """Install the pinned archives, serving each download from memory."""
    install_kmc.install(directory, pins, download=archives.__getitem__)


class InstallTest(unittest.TestCase):
    """install() writes the programs only when every archive checks out."""

    def test_unpacks_every_program_as_an_executable(self) -> None:
        """Check that each archive's programs land in the directory, executable."""
        gcc = make_archive({"gcc": b"compiler", "cc1": b"proper"})
        binutils = make_archive({"./as": b"assembler"})
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root) / "gcc_kmc"
            pins = (pinned("g", gcc), pinned("b", binutils))
            install_from_memory(directory, {"g": gcc, "b": binutils}, pins)
            for name, contents in [
                ("gcc", b"compiler"),
                ("cc1", b"proper"),
                ("as", b"assembler"),
            ]:
                with self.subTest(name=name):
                    path = directory / name
                    self.assertEqual(path.read_bytes(), contents)
                    self.assertTrue(path.stat().st_mode & stat.S_IXUSR)

    def test_replaces_rather_than_writes_through_old_programs(self) -> None:
        """Check that a name hard-linked to an old program keeps the old one."""
        data = make_archive({"cc1": b"release"})
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root) / "gcc_kmc"
            directory.mkdir()
            kept = Path(root) / "kept"
            _ = kept.write_bytes(b"local build")
            (directory / "cc1").hardlink_to(kept)
            install_from_memory(directory, {"a": data}, (pinned("a", data),))
            self.assertEqual((directory / "cc1").read_bytes(), b"release")
            self.assertEqual(kept.read_bytes(), b"local build")
            self.assertEqual(sorted(p.name for p in directory.iterdir()), ["cc1"])

    def test_a_changed_archive_installs_nothing(self) -> None:
        """Check that one archive whose hash differs stops every write."""
        gcc = make_archive({"gcc": b"compiler"})
        binutils = make_archive({"as": b"assembler"})
        stale = install_kmc.Archive(
            "b", hashlib.sha256(b"the release as pinned").hexdigest()
        )
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root) / "gcc_kmc"
            with self.assertRaises(install_kmc.InstallError):
                install_from_memory(
                    directory, {"g": gcc, "b": binutils}, (pinned("g", gcc), stale)
                )
            self.assertFalse(directory.exists())

    def test_entries_outside_the_directory_are_refused(self) -> None:
        """Check that members that would leave or hide in the directory are refused."""
        for name in ["../gcc", "bin/gcc", "/usr/bin/gcc", ".hidden"]:
            data = make_archive({name: b"x"})
            with self.subTest(name=name), tempfile.TemporaryDirectory() as root:
                directory = Path(root) / "gcc_kmc"
                with self.assertRaises(install_kmc.InstallError):
                    install_from_memory(directory, {"a": data}, (pinned("a", data),))
                self.assertFalse(directory.exists())

    def test_a_hard_link_becomes_a_copy(self) -> None:
        """Check that cccp, a hard link to cpp, is installed as its own file."""
        data = make_tree_archive(hard_link_cccp_to_cpp, ["cpp", "cccp"])
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root) / "gcc_kmc"
            install_from_memory(directory, {"a": data}, (pinned("a", data),))
            self.assertEqual((directory / "cccp").read_bytes(), b"preprocessor")
            self.assertEqual((directory / "cpp").read_bytes(), b"preprocessor")

    def test_other_members_are_refused(self) -> None:
        """Check that only files, and links to files before them, are taken."""
        cases = {
            "a link to a later member": make_special_archive(
                "cccp", tarfile.LNKTYPE, "cpp"
            ),
            "a link out of the directory": make_special_archive(
                "cccp", tarfile.LNKTYPE, "../cpp"
            ),
            "a symbolic link": make_special_archive(
                "gcc", tarfile.SYMTYPE, "/usr/bin/gcc"
            ),
            "a directory": make_special_archive("bin", tarfile.DIRTYPE),
        }
        for case, data in cases.items():
            with self.subTest(case=case), tempfile.TemporaryDirectory() as root:
                directory = Path(root) / "gcc_kmc"
                with self.assertRaises(install_kmc.InstallError):
                    install_from_memory(directory, {"a": data}, (pinned("a", data),))
                self.assertFalse(directory.exists())

    def test_downloads_only_over_https(self) -> None:
        """Check that a URL that isn't HTTPS is never opened."""
        with self.assertRaises(install_kmc.InstallError):
            _ = install_kmc.fetch("http://github.com/decompals/mips-gcc-2.7.2")


if __name__ == "__main__":
    _ = unittest.main()
