# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

"""triepack — Native Python implementation of the Triepack .trp binary format."""

__version__ = "0.1.0"


def encode(data: dict) -> bytes:
    """Encode a dictionary into the .trp binary format.

    Args:
        data: The dictionary to encode.

    Returns:
        The encoded bytes.

    Raises:
        NotImplementedError: This function is not yet implemented.
    """
    raise NotImplementedError("triepack.encode is not yet implemented")


def decode(buffer: bytes) -> dict:
    """Decode a .trp binary buffer into a dictionary.

    Args:
        buffer: The .trp binary data.

    Returns:
        The decoded dictionary.

    Raises:
        NotImplementedError: This function is not yet implemented.
    """
    raise NotImplementedError("triepack.decode is not yet implemented")
