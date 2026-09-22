#!/usr/bin/env python3
"""Apply a The Saboteur byte-patch manifest.

The manifest verifies both the source SHA-256 and every original byte region
before writing. This makes accidental rebasing on the wrong executable fail
closed rather than silently corrupting the file.
"""
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('input_exe', type=Path)
    ap.add_argument('manifest', type=Path)
    ap.add_argument('output_exe', type=Path)
    args=ap.parse_args()

    data=bytearray(args.input_exe.read_bytes())
    m=json.loads(args.manifest.read_text(encoding='utf-8'))

    if len(data) != m['file_size']:
        raise SystemExit(f"size mismatch: {len(data)} != {m['file_size']}")
    got=sha256(data)
    if got.lower() != m['base_exe_sha256'].lower():
        raise SystemExit(f"base SHA-256 mismatch:\n  got      {got}\n  expected {m['base_exe_sha256']}")

    for r in m['regions']:
        off=int(r['offset'],16)
        before=bytes.fromhex(r['before'])
        after=bytes.fromhex(r['after'])
        if len(before)!=len(after) or len(before)!=r['length']:
            raise SystemExit(f"invalid manifest region at {r['offset']}")
        current=bytes(data[off:off+len(before)])
        if current != before:
            raise SystemExit(
                f"byte verification failed at {r['offset']}:\n"
                f"  got      {current.hex()}\n  expected {before.hex()}"
            )
        data[off:off+len(after)] = after

    target=sha256(data)
    if target.lower()!=m['target_exe_sha256'].lower():
        raise SystemExit(f"target SHA-256 mismatch after patch: {target}")
    args.output_exe.write_bytes(data)
    print(f"OK: {m['from_version']} -> {m['to_version']}")
    print(f"SHA-256: {target}")
    return 0

if __name__=='__main__':
    raise SystemExit(main())
