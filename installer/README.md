# Windows patcher

The active installer source is the unified cumulative patcher for The Saboteur Enhanced Patch.

Current source designation: `v1Pv260`

- patcher generation: **v1P**
- cumulative game payload: **V260**
- Windows artifact: **The_Saboteur_Enhanced_Patcher_v1Pv260.exe**

## Detection

The patcher hashes `Saboteur.exe` before modification.

Accepted states:

- exact untouched retail executable;
- exact validated V258Y executable;
- exact validated V259 executable;
- exact V260 target, detected as already installed.

Unknown or modified executables are refused.

## Reconstruction

The V260 patcher bundles three compressed `SABDP1` direct deltas:

- retail -> V260;
- V258Y -> V260;
- V259 -> V260.

Every direct delta verifies source size/SHA-256, every original byte region, target size, and final target SHA-256.

The packaged patcher contains no retail `Saboteur.exe`.

## Safety

A `.Backup` is created only after a supported source hash is recognized. The target is reconstructed and verified before replacement. Replacement uses a rollback file and the installed executable is hashed again before rollback data is removed.

## Current validated payload

V260 adds the validated WSDecal improvement:

- active decal ceiling: 400 -> 800;
- matching WSDecal pool capacity: 400 -> 800.

Target V260 SHA-256:

`a90acc384bab67b7ac54bb973a81852ab2f8bce232d9215cc569af5f5d725440`

## Building

GitHub Actions uses Python 3.12 + PyInstaller 6.16.0 on `windows-latest`.

`.github/workflows/build-patcher.yml` builds the development artifact automatically after installer changes.

`.github/workflows/publish-release.yml` publishes the public GitHub Release when manually dispatched.
