# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

"""triepack — Native Python implementation of the Triepack .trp binary format."""

__version__ = "1.3.0"

from .decoder import decode
from .encoder import MAX_ALPHABET_SIZE, encode

#: Version of the on-disk .trp format this implementation writes. Distinct
#: from the library version: it changes only when the bytes change.
FORMAT_VERSION_MAJOR = 1
FORMAT_VERSION_MINOR = 0


def version():
    """Return metadata about this build.

    Every triepack implementation answers with the same shape, so a polyglot
    system can ask each one what it is.
    """
    major, minor, patch = (int(p) for p in __version__.split("."))
    return {
        "name": "triepack",
        "implementation": "python",
        "version": __version__,
        "version_major": major,
        "version_minor": minor,
        "version_patch": patch,
        "format_version_major": FORMAT_VERSION_MAJOR,
        "format_version_minor": FORMAT_VERSION_MINOR,
        "max_alphabet_size": MAX_ALPHABET_SIZE,
    }


__all__ = [
    "encode",
    "decode",
    "version",
    "MAX_ALPHABET_SIZE",
    "FORMAT_VERSION_MAJOR",
    "FORMAT_VERSION_MINOR",
    "__version__",
]
