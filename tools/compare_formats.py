#!/usr/bin/env python3
"""compare_formats.py -- one table, every format, every corpus.

Measures the same logical content encoded by each format, so the numbers are
comparable across a row:

  * key-list corpora  -- just the key set; there are no values
  * JSON corpora      -- keys and values

triepack v1 comes from the Python binding. triepack v2 comes from
tools/v2_prototype.c, which verifies every key through its own serialised
bytes before reporting a size. Value-store size for v2 is computed from the
format's own value encoding (4-bit tag + payload), because the prototype
stores synthetic integers rather than the corpus's real values.

Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""

import gzip
import heapq
import json
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "bindings", "python"))

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PROTOTYPE = os.path.join(ROOT, "build", "tools", "v2_prototype")


# --------------------------------------------------------------------------
# Optional dependencies: report what is missing rather than failing.
# --------------------------------------------------------------------------

def _try(name):
    try:
        return __import__(name)
    except ImportError:
        return None


bson = _try("bson")
msgpack = _try("msgpack")
triepack = _try("triepack")


# --------------------------------------------------------------------------
# Value sizing, using triepack's own value encoding
# --------------------------------------------------------------------------

def leb128_len(v):
    n = 1
    while v >= 0x80:
        v >>= 7
        n += 1
    return n


def value_bits(v):
    """Bits one value occupies: a 4-bit tag plus its payload."""
    if v is None:
        return 4
    if isinstance(v, bool):
        return 4 + 1
    if isinstance(v, int):
        zig = (v << 1) ^ (v >> 63) if v < 0 else (v << 1)
        return 4 + 8 * leb128_len(zig)
    if isinstance(v, float):
        return 4 + 64
    if isinstance(v, str):
        raw = v.encode("utf-8")
        # length varint, then byte-aligned payload
        return 4 + 8 * leb128_len(len(raw)) + 8 * len(raw) + 4
    raw = json.dumps(v).encode("utf-8")
    return 4 + 8 * leb128_len(len(raw)) + 8 * len(raw) + 4


def encode_value(v):
    """The format's own value encoding, as bytes, so a value store can be
    recompressed alongside the key structure rather than merely counted.
    Tags are nibble-aligned in the real format; this pads each value to a
    byte, which costs a few percent and keeps the code readable."""
    def leb(n):
        out = bytearray()
        while True:
            b = n & 0x7F
            n >>= 7
            out.append(b | (0x80 if n else 0))
            if not n:
                return bytes(out)

    if v is None:
        return b"\x00"
    if isinstance(v, bool):
        return b"\x01" + (b"\x01" if v else b"\x00")
    if isinstance(v, int):
        return b"\x02" + leb((v << 1) ^ (v >> 63) if v < 0 else v << 1)
    if isinstance(v, float):
        import struct
        return b"\x05" + struct.pack(">d", v)
    raw = v.encode("utf-8") if isinstance(v, str) else json.dumps(v).encode()
    return b"\x06" + leb(len(raw)) + raw


def value_store_blob(values):
    return b"".join(encode_value(v) for v in values)


def value_store_bytes(values):
    if not values:
        return 0
    bits = sum(value_bits(v) for v in values)
    store = (bits + 7) // 8
    # Sampled offset index, one u64 per 32 values (spec v2 section 9.2).
    return store + 8 * ((len(values) + 31) // 32)


# --------------------------------------------------------------------------
# Per-format sizes
# --------------------------------------------------------------------------

def size_gzip(raw):
    return len(gzip.compress(raw, 9))


def size_huffman(data):
    """Byte-level canonical Huffman: coded payload plus a 256-byte table of
    code lengths. Naive on purpose -- the question is how much redundancy a
    triepack file still carries at byte granularity."""
    from collections import Counter

    freq = Counter(data)
    if not freq:
        return 0
    if len(freq) == 1:
        return (len(data) + 7) // 8 + 256
    heap = list(freq.values())
    heapq.heapify(heap)
    bits = 0
    while len(heap) > 1:
        a = heapq.heappop(heap)
        b = heapq.heappop(heap)
        bits += a + b              # total coded bits = sum of merged weights
        heapq.heappush(heap, a + b)
    return (bits + 7) // 8 + 256


def size_msgpack(obj):
    return len(msgpack.packb(obj)) if msgpack else None


def size_bson(obj):
    if not bson:
        return None
    doc = obj if isinstance(obj, dict) else {"root": obj}
    try:
        return len(bson.encode(doc))
    except Exception:
        return None


def refusal_note(mapping):
    """Why a format declined, when it did -- a refusal is information."""
    if not triepack:
        return None
    try:
        triepack.encode(mapping)
        return None
    except ValueError as e:
        return str(e)


def size_v1(mapping):
    if not triepack:
        return None
    try:
        return len(triepack.encode(mapping))
    except Exception:
        return None


def size_v2(keys, values):
    """Run the prototype over the key list; add the value store separately."""
    if not os.path.exists(PROTOTYPE):
        return None, None
    with tempfile.NamedTemporaryFile("wb", suffix=".txt", delete=False) as f:
        for k in keys:
            f.write((k if isinstance(k, bytes) else k.encode()) + b"\n")
        path = f.name
    path2 = path
    try:
        out = subprocess.run([PROTOTYPE, path], capture_output=True, text=True, timeout=1800).stdout
    except Exception:
        os.unlink(path)
        return None, None, None, None

    keys_only = None
    for line in out.splitlines():
        # Variant D is the specified encoding: alphabet-coded labels *and*
        # alphabet-coded tail pool.
        if line.startswith("| D:"):
            keys_only = int(line.split("|")[4].strip())
    if keys_only is None:
        return None, None, None, None

    # Real bytes, so "then gzip it" is a measurement rather than a guess.
    blob = None
    with tempfile.NamedTemporaryFile("wb", suffix=".trp2", delete=False) as g:
        emit = g.name
    try:
        subprocess.run([PROTOTYPE, path2, emit], capture_output=True, timeout=1800)
        if os.path.exists(emit) and os.path.getsize(emit):
            blob = open(emit, "rb").read()
    finally:
        if os.path.exists(emit):
            os.unlink(emit)

    os.unlink(path)
    # Recompress the whole artifact -- key structure and value store -- so the
    # +gzip and +huffman columns describe the same thing the v2 column does.
    if blob is not None and values:
        blob = blob + value_store_blob(values)
    gz = size_gzip(blob) if blob else None
    hf = size_huffman(blob) if blob else None
    return keys_only, keys_only + value_store_bytes(values), gz, hf


# --------------------------------------------------------------------------
# Corpora
# --------------------------------------------------------------------------

def load(path, kind):
    """Return (native_object, mapping, keys, values)."""
    raw = open(path, "rb").read()
    if kind == "json":
        doc = json.loads(raw)
        flat = {}

        def walk(node, prefix=""):
            if isinstance(node, dict):
                for k, v in node.items():
                    walk(v, f"{prefix}.{k}" if prefix else k)
            elif isinstance(node, list):
                for i, v in enumerate(node):
                    walk(v, f"{prefix}[{i}]")
            else:
                flat[prefix] = node

        walk(doc)
        return raw, doc, flat, list(flat), list(flat.values())

    # Keep list corpora as *bytes*. Decoding with "replace" would fold every
    # non-UTF-8 byte into U+FFFD, shrinking the alphabet and quietly turning
    # the binary corpus into something v1 can encode -- which is exactly the
    # limit the corpus exists to probe.
    keys = [l for l in raw.split(b"\n") if l]
    return raw, keys, {k: None for k in keys}, keys, []


CORPORA = [
    ("tests/data/common_words_10k.txt", "list", "10k English words, one per line"),
    ("tests/data/benchmark_100k.json", "json", "Synthetic product catalog, nested"),
    ("/tmp/paths.txt", "list", "20k URL-like paths, deep shared prefixes"),
    ("/tmp/wide.bin", "list", "20k random binary keys, 253 byte values"),
    ("data/enwik9", "list", "Wikipedia XML (first 16 MB of enwik9)"),
]

LIMIT = {"data/enwik9": 16 * 1024 * 1024}

# The whole gigabyte. Opt-in (--full) because it needs ~60 GB of RAM and
# several minutes -- the encoder's build strategy is a known Phase 3 blocker,
# not a property of the format. The point of the row is that nothing breaks.
FULL = ("data/enwik9", "text list", "Wikipedia XML, the whole 1 GB file")


def stream_gzip_size(path, chunk=1 << 22):
    """gzip a file without holding it in memory twice."""
    import io

    total = 0
    sink = io.BytesIO()
    with gzip.GzipFile(fileobj=sink, mode="wb", compresslevel=9) as gz, open(path, "rb") as f:
        while True:
            b = f.read(chunk)
            if not b:
                break
            gz.write(b)
            total += sink.tell()
            sink.seek(0)
            sink.truncate()
    return total + sink.tell()


def full_corpus_row():
    path = os.path.join(ROOT, FULL[0])
    if not os.path.exists(path) or not os.path.exists(PROTOTYPE):
        return None

    emit = tempfile.NamedTemporaryFile(suffix=".trp2", delete=False).name
    out = subprocess.run([PROTOTYPE, path, emit], capture_output=True, text=True,
                         timeout=7200).stdout
    keys = v2 = None
    for line in out.splitlines():
        if line.startswith("| D:"):
            v2 = int(line.split("|")[4].strip())
        # "keys   10920486" -- not "keys verified  ... / ... OK"
        parts = line.split()
        if len(parts) == 2 and parts[0] == "keys" and parts[1].isdigit():
            keys = int(parts[1])
    verified = "MISMATCH" not in out

    gz = hf = None
    if os.path.exists(emit) and os.path.getsize(emit):
        blob = open(emit, "rb").read()
        gz = size_gzip(blob)
        hf = size_huffman(blob)
        del blob
    if os.path.exists(emit):
        os.unlink(emit)

    return {
        "file": "enwik9 (full)",
        "type": "text list",
        "desc": FULL[2] + (" — all keys verified" if verified else " — VERIFY FAILED"),
        "keys": keys or 0,
        "values": 0,
        "raw": os.path.getsize(path),
        # v1 cannot represent this: the data stream exceeds its 2^32-bit
        # ceiling, so the encoder refuses rather than truncating the offsets.
        "v1": None,
        "v1_note": "data stream exceeds the 512 MB ceiling (TP_ERR_OVERFLOW)",
        "v2_keys": v2,
        "v2": v2,
        "v2_gzip": gz,
        "v2_huff": hf,
        "msgpack": None,
        "bson": None,
        "gzip": stream_gzip_size(path),
    }


def main():
    rows = []
    for rel, kind, desc in CORPORA:
        path = rel if os.path.isabs(rel) else os.path.join(ROOT, rel)
        if not os.path.exists(path):
            continue

        cap = LIMIT.get(rel)
        if cap:
            with open(path, "rb") as f:
                blob = f.read(cap)
            tmp = tempfile.NamedTemporaryFile("wb", suffix=".txt", delete=False)
            tmp.write(blob[: blob.rfind(b"\n") + 1])
            tmp.close()
            path = tmp.name

        raw, native, mapping, keys, values = load(path, kind)
        rows.append(
            {
                "file": os.path.basename(rel),
                "type": "JSON" if kind == "json" else "text list",
                "desc": desc,
                "keys": len(keys),
                "values": len(values),
                "raw": len(raw),
                "v1": size_v1(mapping),
                "v1_note": refusal_note(mapping),
                **dict(zip(("v2_keys", "v2", "v2_gzip", "v2_huff"), size_v2(keys, values))),
                "msgpack": size_msgpack(native) if len(raw) < 64 * 1024 * 1024 else None,
                "bson": size_bson(native) if len(raw) < 64 * 1024 * 1024 else None,
                "gzip": size_gzip(raw),
            }
        )
        if cap:
            os.unlink(path)

    if "--full" in sys.argv:
        row = full_corpus_row()
        if row:
            rows.append(row)

    cols = [("raw", "raw"), ("v1", "triepack v1"), ("v2", "triepack v2"),
            ("v2_gzip", "v2 + gzip"), ("v2_huff", "v2 + huffman"),
            ("msgpack", "MessagePack"), ("bson", "BSON"), ("gzip", "gzip -9")]

    def fmt(n):
        return f"{n:,}" if isinstance(n, int) else "n/a"

    def ratio(raw, n):
        """Size as a fraction of the raw file: 0.32 means it shrank to 32%.
        Anything above 1.0 grew."""
        return f"{n / raw:.3f}" if isinstance(n, int) and raw else "--"

    print("\n# Format comparison\n")
    print("Each cell is bytes, then size as a fraction of the raw file.")
    print("0.32 means it shrank to 32% of the original; above 1.0 it grew.\n")
    header = ["file", "type", "keys", "values"] + [c[1] for c in cols]
    print("| " + " | ".join(header) + " |")
    print("|" + "---|" * len(header))
    for r in rows:
        cells = [r["file"], r["type"], f"{r['keys']:,}", f"{r['values']:,}"]
        for key, _ in cols:
            n = r[key]
            cells.append(fmt(n) if key == "raw" else f"{fmt(n)} ({ratio(r['raw'], n)})")
        print("| " + " | ".join(cells) + " |")

    print("\n## What each corpus is\n")
    for r in rows:
        print(f"- **{r['file']}** — {r['desc']}")

    notes = [(r["file"], r["v1_note"]) for r in rows if r.get("v1_note")]
    if notes:
        print("\n## Refusals\n")
        for f, n in notes:
            print(f"- **{f}** — triepack v1 refused: {n}")

    missing = [n for n, m in (("pymongo/bson", bson), ("msgpack", msgpack),
                              ("triepack (python binding)", triepack)) if not m]
    if missing:
        print("\nnot measured (missing): " + ", ".join(missing))
    if not os.path.exists(PROTOTYPE):
        print("\ntriepack v2 not measured: build it with "
              "`cmake --build build --target v2_prototype`")


if __name__ == "__main__":
    main()
