"""What the add-on tells an artist about their character.

Every check says what it found the same way: what the problem is about (an
object, material, image or bone, by the name the artist gave it), one or two
sentences on what is wrong and what to do, in modeling terms, and the faces
or vertices at fault, so the panel can select them. Errors stop a bind or an
export; warnings don't, but tell of something the game shows differently
from Blender.
"""

from dataclasses import dataclass, field
from typing import override


@dataclass(frozen=True)
class Problem:
    """One thing an artist should fix or know about."""

    subject: str
    sentence: str
    # The mesh object whose faces or vertices are at fault, if any
    object_name: str | None = None
    faces: tuple[int, ...] = ()
    vertices: tuple[int, ...] = ()

    @override
    def __str__(self) -> str:
        return f"{self.subject}: {self.sentence}"

    @property
    def has_selection(self) -> bool:
        """Whether there are faces or vertices to select for it."""
        return self.object_name is not None and bool(self.faces or self.vertices)


@dataclass
class Report:
    """What a bind or export found, and what it did."""

    errors: list[Problem] = field(default_factory=list)
    warnings: list[Problem] = field(default_factory=list)
    # What was done, when nothing stopped it
    summary: str = ""

    def error(self, subject: str, sentence: str, **at_fault: object) -> None:
        """Note a problem that stops the bind or export."""
        self.errors.append(Problem(subject, sentence, **at_fault))  # type: ignore[arg-type]

    def warn(self, subject: str, sentence: str, **at_fault: object) -> None:
        """Note something the artist should know, which doesn't stop anything."""
        self.warnings.append(Problem(subject, sentence, **at_fault))  # type: ignore[arg-type]

    def extend(self, other: "Report") -> None:
        """Take another report's problems."""
        self.errors += other.errors
        self.warnings += other.warnings


def count(number: int, singular: str, plural: str | None = None) -> str:
    """Return a number with its noun: "1 face", "3 faces"."""
    return f"{number} {singular if number == 1 else plural or singular + 's'}"


def agree(number: int, singular: str, plural: str) -> str:
    """Return the verb that agrees with NUMBER, such as "is" for 1 and "are" for 2."""
    return singular if number == 1 else plural
