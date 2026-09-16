# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

"""triepack — Native Python implementation of the Triepack .trp binary format."""

__version__ = "1.2.0"

from .decoder import decode
from .encoder import MAX_ALPHABET_SIZE, encode

__all__ = ["encode", "decode", "MAX_ALPHABET_SIZE", "__version__"]
