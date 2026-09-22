# Windows patcher design

## Goal

Public distribution should use a single Windows patcher executable rather than shipping a growing chain of patched game executables.

The patcher is a front-end for the repository's cumulative byte-patch data. The repository remains the auditable source of truth.

## Canonical model

For every validated release:

1. The latest validated game state is cumulative.
2. A direct manifest exists from the untouched retail executable to the latest build.
3. A delta manifest exists from the previous validated build to the latest build.
4. The Windows patcher embeds only patch data and patching logic, never the copyrighted game executable.
5. The patcher verifies all known source states by SHA-256 before modification.
6. The patcher creates a backup before replacing the game executable.
7. The generated executable is verified against the expected final SHA-256.
8. Replacement occurs only after successful verification.

## Supported source states

At minimum, the patcher should recognize:

- untouched retail executable
- previous public validated release

Optionally it can recognize older canonical milestones such as V200, V257, V258W and V258Y if maintaining those upgrade paths remains useful.

Unknown executables must be rejected with a clear message rather than patched heuristically.

## User-facing flow

1. Locate or select `Saboteur.exe`.
2. Compute SHA-256.
3. Identify the exact known source build.
4. Create `Saboteur.exe.backup` or a timestamped backup.
5. Apply the cumulative patch in memory or to a temporary file.
6. Verify target SHA-256.
7. Atomically replace the executable.
8. Report source build, target build, backup path and resulting SHA-256.

## Safety rules

- Never patch an unknown SHA-256.
- Never overwrite the only copy of the source executable.
- Never write directly over the source while patching.
- Verify all expected original bytes before writing replacements.
- Verify the final target SHA-256 before replacement.
- Preserve a recovery path if anything fails.
- Do not silently downgrade or remove previously validated fixes.

## Versioning

The public patcher version should follow the current validated cumulative game-patch build.

Example:

`The_Saboteur_Enhanced_Patcher_V259.exe`

If V259 is later validated, that patcher must generate the exact V259 cumulative executable from every supported known source state.

## Repository role

GitHub stores:

- source code for the patcher
- manifests / patch data
- hashes
- technical notebook
- build history
- release notes

The public patcher is the convenient delivery mechanism. The manifests remain the forensic/audit layer.

## Current state

Current canonical cumulative build:

**V258Y**

Current next technical target before the next validated release:

**blue floating/off-screen HUD indicator clamp**

Once the next build is validated, the patcher should be generated or updated to target that new cumulative build.
