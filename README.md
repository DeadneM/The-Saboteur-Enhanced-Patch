# The Saboteur Enhanced Patch

Experimental PC enhancement patch for **The Saboteur**, developed through static binary auditing and in-game validation.

> **Current validated cumulative build: V258Y**  
> HUD/minimap state validated on 2026-09-22.

## Golden rule: every current build is cumulative

**The latest validated build must contain every validated change from the original executable up to that point.**

That means:

- V258Y is not "just the minimap build".
- V258Y is the full cumulative patch state: all validated engine, streaming, FOV/windowing, HUD, reticle, scope, objective, inventory/ammo, minimap and other retained fixes from the earlier validated lineage.
- A new build must start from the latest validated cumulative base unless there is a specific reason to rebuild from an older clean milestone.
- If we rebuild from an older milestone, every later validated change must be re-applied before the new build can become the next canonical base.
- Experimental/rejected changes are never carried forward.
- The newest validated build supersedes older validated builds as the practical install/test target.

So the project model is:

`Original EXE -> ...validated cumulative work... -> V200 -> V255A -> V257 -> V258G -> V258M -> V258P -> V258W -> V258Y -> next build`

The repository currently has exact reproducible manifests from **V200 through V258Y**. V200 is the surviving re-audited cumulative baseline that already contains the validated work that preceded it. To make the repository independently reconstruct the full chain starting from an untouched retail EXE, we still need to add an exact **Original EXE -> V200** manifest when that untouched executable is available for byte comparison.

## Current frozen HUD state

The minimap is validated and must not be changed unless explicitly reopened:

- Native/orange layer: **X = 100, Y = 60**
- Scaleform/black layer: **X = 33.333333333333336, Y = 20**

Other current HUD values inherited in the cumulative V258Y build:

- Tutorial: `X=35`, `Y=30`
- ObjectiveTray: local X magnitude `18`, local Y magnitude `49.666666666666664`
- Inventory/ammo: local `X=+91.66666666666667`, `Y=+51.666666666666664`

## What this repository contains

This repository tracks **reproducible patch data, source tooling, hashes, and the technical notebook** rather than committing the copyrighted game executable itself.

- [`docs/TECHNICAL_NOTEBOOK.md`](docs/TECHNICAL_NOTEBOOK.md): current source of truth, addresses, formulas, engine/HUD findings and next work.
- [`docs/BUILD_HISTORY.md`](docs/BUILD_HISTORY.md): validated lineage and rejected experiments.
- [`docs/HASHES.md`](docs/HASHES.md): archive and executable hash ledger.
- [`tools/apply_patch.py`](tools/apply_patch.py): fail-closed byte-patch applicator.
- [`patches/`](patches/): exact byte-diff manifests between canonical milestones.

## Canonical patch chain represented in this repo

- [`V200 -> V255A`](patches/v200_to_v255a.json)
- [`V255A -> V257`](patches/v255a_to_v257.json)
- [`V257 -> V258G`](patches/v257_to_v258g.json)
- [`V258G -> V258M`](patches/v258g_to_v258m.json)
- [`V258M -> V258P`](patches/v258m_to_v258p.json)
- [`V258P -> V258W`](patches/v258p_to_v258w.json)
- [`V258W -> V258Y`](patches/v258w_to_v258y.json)

Each manifest records:

- exact source EXE SHA-256
- exact target EXE SHA-256
- source bytes and replacement bytes
- file offsets
- changed-byte count

The patcher refuses to modify an executable if the source hash or expected bytes do not match.

## Engine baseline

**V257 Max Engine** is the validated engine baseline carried inside the cumulative V258Y build. It retains the proven streaming architecture while increasing selected visibility/LOD values conservatively for a DX9 32-bit LAA title. See the technical notebook for the exact values and locations.

## Development rules

1. **Last validated build is always cumulative.**
2. Prefer surgical, isolated binary changes.
3. Warn before any global or intrusive approach.
4. Rebuild from a clean validated base rather than stacking uncontrolled experiments.
5. If rebuilding from an older milestone, re-apply all later validated changes before promoting it.
6. Treat the README/notebook as a laboratory log: record base hashes, addresses, constants, formulas, test results, failures, regressions, frozen systems, and next hypotheses.
7. A build is not a new base until it has been validated in-game.

## Current next target

The next open HUD item is the **blue floating/off-screen indicator** that can be clipped at the left edge. The intended fix is to identify and slightly reduce its actual screen-edge clamp/radius, without touching the validated V258Y minimap.

## Disclaimer

This is an unofficial community project and is not affiliated with the original developers or publisher. Back up your original executable before testing any binary patch.
