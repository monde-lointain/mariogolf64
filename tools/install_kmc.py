#!/usr/bin/env python3
"""Install KMC GCC 2.7.2, the compiler that built the original, for this host.

    install_kmc.py DIRECTORY

The matching build compiles with KMC GCC 2.7.2 and assembles with its
binutils 2.6, as decompals rebuilt them for modern hosts. Each archive is
pinned to its release and checked against its SHA-256, so that a release
that moved or changed fails here instead of building a different ROM. The
programs go straight into DIRECTORY, where the Makefile looks for them.

decompals builds both for Linux on x86-64 and for Apple silicon Macs; its
Mac compiler is also built for Intel, but its Mac assembler is not.
"""

from __future__ import annotations

import hashlib
import http.client
import io
import platform
import sys
import tarfile
import urllib.parse
from dataclasses import dataclass
from http import HTTPStatus
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Callable

GCC_RELEASE = "https://github.com/decompals/mips-gcc-2.7.2/releases/download/v0.2"
BINUTILS_RELEASE = (
    "https://github.com/decompals/mips-binutils-2.6/releases/download/v0.4"
)
EXECUTABLE_MODE = 0o755
DOWNLOAD_TIMEOUT_SECONDS = 60
# GitHub sends a release's download on to the server that holds it
REDIRECTS_MAX = 5
REDIRECT_STATUSES = {
    HTTPStatus.MOVED_PERMANENTLY,
    HTTPStatus.FOUND,
    HTTPStatus.SEE_OTHER,
    HTTPStatus.TEMPORARY_REDIRECT,
    HTTPStatus.PERMANENT_REDIRECT,
}
USER_AGENT = "mariogolf64-install-kmc"


@dataclass(frozen=True)
class Archive:
    """A release archive of KMC tools and the SHA-256 that it must have."""

    url: str
    sha256: str


# Each host's compiler and assembler, keyed by what platform.system() and
# platform.machine() call it
ARCHIVES: dict[tuple[str, str], tuple[Archive, Archive]] = {
    ("Linux", "x86_64"): (
        Archive(
            f"{GCC_RELEASE}/gcc-2.7.2-linux.tar.gz",
            "a155059aa469928958afc3bf716f2bca75c238a3d0af315213c4fdbc7b66cb79",
        ),
        Archive(
            f"{BINUTILS_RELEASE}/binutils-2.6-linux.tar.gz",
            "d1a3c12d910e07034f9cf0ceeebc5e6adb6267a0fed1c8b6757477c6a9b2e4cb",
        ),
    ),
    ("Darwin", "arm64"): (
        Archive(
            f"{GCC_RELEASE}/gcc-2.7.2-mac-arm.tar.gz",
            "247d99868d438bca43c8d7ce09face50967055f1af20a28bb4fc2a06cf5c63ce",
        ),
        Archive(
            f"{BINUTILS_RELEASE}/binutils-2.6-mac.tar.gz",
            "a0dfe9eea01500ba50214c0f45bbee1106d69496a47685b7f45f28d387db08b2",
        ),
    ),
}


class InstallError(Exception):
    """A reason that the tools can't be installed, worded for the user."""


def archives_for(system: str, machine: str) -> tuple[Archive, Archive]:
    """Return the archives to install on a host, or explain why there are none."""
    archives = ARCHIVES.get((system, machine))
    if archives is None:
        hosts = ", ".join(f"{name} on {arch}" for name, arch in ARCHIVES)
        message = (
            f"KMC GCC has no build for {system} on {machine}; it runs on {hosts}. "
            "The modern build (make MODERN=1) and the PC port need no KMC GCC."
        )
        raise InstallError(message)
    return archives


def fetch(url: str) -> bytes:
    """Download a release archive, following GitHub's redirects over HTTPS only."""
    for _ in range(REDIRECTS_MAX + 1):
        status, location, body = get(url)
        if status in REDIRECT_STATUSES and location:
            url = urllib.parse.urljoin(url, location)
            continue
        if status != HTTPStatus.OK:
            message = f"{url} answered HTTP {status}"
            raise InstallError(message)
        return body
    message = f"{url} redirected more than {REDIRECTS_MAX} times"
    raise InstallError(message)


def get(url: str) -> tuple[int, str | None, bytes]:
    """Request a URL once, returning the status, any redirect and the body."""
    parts = urllib.parse.urlsplit(url)
    if parts.scheme != "https" or not parts.hostname:
        message = f"refusing to download {url} over anything but HTTPS"
        raise InstallError(message)
    target = f"{parts.path}?{parts.query}" if parts.query else parts.path
    connection = http.client.HTTPSConnection(
        parts.hostname, parts.port, timeout=DOWNLOAD_TIMEOUT_SECONDS
    )
    try:
        connection.request("GET", target, headers={"User-Agent": USER_AGENT})
        response = connection.getresponse()
        return response.status, response.getheader("Location"), response.read()
    finally:
        connection.close()


def verified(archive: Archive, data: bytes) -> bytes:
    """Return an archive's bytes once they match the pinned SHA-256."""
    actual = hashlib.sha256(data).hexdigest()
    if actual != archive.sha256:
        message = (
            f"{archive.url} has SHA-256 {actual}, not {archive.sha256}; "
            "the release has changed, so nothing was installed"
        )
        raise InstallError(message)
    return data


def program_members(data: bytes) -> list[tuple[str, bytes]]:
    """Return each program in an archive, as its name and its contents.

    The archives hold their programs at the top level and nothing else,
    such as cccp, a hard link to cpp, so anything that would land outside
    the directory, or is neither a file nor a link to an earlier one, means
    the archive isn't one of them. A link becomes a copy.
    """
    programs: dict[str, bytes] = {}
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tar:
        for member in tar.getmembers():
            name = top_level_name(member.name)
            if member.islnk():
                target = programs.get(top_level_name(member.linkname))
                if target is None:
                    message = f"{member.name!r} doesn't link to an earlier file"
                    raise InstallError(message)
                programs[name] = target
                continue
            contents = tar.extractfile(member) if member.isfile() else None
            if contents is None:
                message = f"unexpected entry {member.name!r} in a KMC archive"
                raise InstallError(message)
            programs[name] = contents.read()
    return list(programs.items())


def top_level_name(path: str) -> str:
    """Return an archive member's name, if it stays at the archive's top level."""
    name = path.removeprefix("./")
    if not name or "/" in name or name.startswith("."):
        message = f"unexpected entry {path!r} in a KMC archive"
        raise InstallError(message)
    return name


def install(
    directory: Path,
    archives: tuple[Archive, ...],
    download: Callable[[str], bytes] = fetch,
) -> None:
    """Download, check and unpack the archives into a directory.

    Every archive is checked before any file is written, so a failure
    leaves the directory as it was. Each program replaces any old one by
    renaming a new file over it, rather than writing into it, so that a
    program running from the old file, or another name linked to it, keeps
    what it had.
    """
    unpacked = [
        program_members(verified(archive, download(archive.url)))
        for archive in archives
    ]
    directory.mkdir(parents=True, exist_ok=True)
    for programs in unpacked:
        for name, contents in programs:
            staged = directory / f".{name}.new"
            _ = staged.write_bytes(contents)
            staged.chmod(EXECUTABLE_MODE)
            _ = staged.replace(directory / name)


def main(argv: list[str]) -> int:
    """Install the tools into the directory that argv names."""
    if len(argv) != 2:  # noqa: PLR2004 - the program's name and the directory
        print(__doc__, file=sys.stderr)
        return 2
    try:
        install(Path(argv[1]), archives_for(platform.system(), platform.machine()))
    except (InstallError, OSError) as error:
        print(f"install_kmc.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
