# Windows patcher

The active installer is a unified cumulative patcher for The Saboteur Enhanced Patch.

Current designation: `v1Pv259`

- patcher generation: **v1P**
- cumulative game payload: **v259**

## Detection

The patcher hashes `Saboteur.exe` before modification.

Accepted states:

- exact untouched retail executable;
- exact validated V258Y executable;
- exact V259 target, detected as already installed.

Unknown or modified executables are refused.

## Reconstruction

The patcher bundles two compressed `SABDP1` direct deltas inside the standalone EXE:

- retail -> V259;
- V258Y -> V259.

Every direct delta verifies source size/SHA-256, every original byte region, target size, and final target SHA-256.

The packaged patcher contains no retail `Saboteur.exe`.

## Safety

A `.Backup` is created only after a supported source hash is recognized. The target is reconstructed and verified before replacement. Replacement uses a rollback file and the installed executable is hashed again before rollback data is removed.

## Building

GitHub Actions uses Python 3.12 + PyInstaller 6.16.0 on `windows-latest`.
