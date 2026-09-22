#!/usr/bin/env python3
"""Apply a The Saboteur byte-patch manifest.

Supported manifest encodings:
- plain JSON (.json)
- zlib-compressed JSON, base64 encoded (.json.zlib.b64)

Every patch verifies:
- source file size
- source SHA-256
- every original byte region
- final target SHA-256

This makes accidental rebasing on the wrong executable fail closed.
"""
from __future__ import annotations
import argparse, base64, hashlib, json, zlib
from pathlib import Path

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def load_manifest(path: Path) -> dict:
    raw = path.read_bytes()
    if path.name.endswith(".zlib.b64"):
        raw = zlib.decompress(base64.b64decode(raw))
    return json.loads(raw.decode("utf-8"))

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("input_exe", type=Path)
    ap.add_argument("manifest", type=Path)
    ap.add_argument("output_exe", type=Path)
    args = ap.parse_args()

    data = bytearray(args.input_exe.read_bytes())
    m = load_manifest(args.manifest)

    if len(data) != m["file_size"]:
        raise SystemExit(f"size mismatch: {len(data)} != {m['file_size']}")

    got = sha256(data)
    if got.lower() != m["base_exe_sha256"].lower():
        raise SystemExit(
            f"base SHA-256 mismatch:\n"
            f"  got      {got}\n"
            f"  expected {m['base_exe_sha256']}"
        )

    for r in m["regions"]:
        off = int(r["offset"], 16)
        before = bytes.fromhex(r["before"])
        after = bytes.fromhex(r["after"])
        if len(before) != len(after) or len(before) != r["length"]:
            raise SystemExit(f"invalid manifest region at {r['offset']}")

        current = bytes(data[off:off + len(before)])
        if current != before:
            raise SystemExit(
                f"byte verification failed at {r['offset']}:\n"
                f"  got      {current.hex()}\n"
                f"  expected {before.hex()}"
            )
        data[off:off + len(after)] = after

    target = sha256(data)
    if target.lower() != m["target_exe_sha256"].lower():
        raise SystemExit(f"target SHA-256 mismatch after patch: {target}")

    args.output_exe.write_bytes(data)
    print(f"OK: {m['from_version']} -> {m['to_version']}")
    print(f"SHA-256: {target}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
