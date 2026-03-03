# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

"""triepack — Native Python implementation of the Triepack .trp binary format."""

__version__ = "0.1.0"

from .decoder import decode
from .encoder import encode

__all__ = ["encode", "decode", "__version__"]
