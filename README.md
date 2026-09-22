# The Saboteur Enhanced Patch

Experimental PC enhancement patch for **The Saboteur**, developed through static binary auditing and in-game validation.

> **Current validated cumulative build: V258Y**  
> HUD/minimap state validated on 2026-09-22.

## Golden rule: every current build is cumulative

**The latest validated build contains every retained validated change from the original executable up to that point.**

V258Y is therefore the full current patch state, not a minimap-only build.

The repository can now reconstruct that full cumulative state directly from the untouched retail executable supplied for this project.

Original EXE SHA-256:

`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

Current V258Y EXE SHA-256:

`032889675706926c60c54ea2ced31cbb6703b5b4ac9872f4da27413cc9993f4e`

## Reproducible cumulative paths

Two verified routes are stored:

### Direct cumulative route

- [`Original -> V258Y`](patches/original_to_v258y.json.zlib.b64)

This is the preferred proof that the latest validated build is cumulative.

### Milestone route

- [`Original -> V200`](patches/original_to_v200.json.zlib.b64)
- [`V200 -> V255A`](patches/v200_to_v255a.json)
- [`V255A -> V257`](patches/v255a_to_v257.json)
- [`V257 -> V258G`](patches/v257_to_v258g.json)
- [`V258G -> V258M`](patches/v258g_to_v258m.json)
- [`V258M -> V258P`](patches/v258m_to_v258p.json)
- [`V258P -> V258W`](patches/v258p_to_v258w.json)
- [`V258W -> V258Y`](patches/v258w_to_v258y.json)

Both routes were verified locally to reproduce the exact V258Y SHA-256 above.

The two large Original-based manifests are stored as zlib-compressed JSON encoded in base64 so the repository stays readable. The patcher supports both plain JSON and `.json.zlib.b64`.

## Current frozen HUD state

The minimap is validated and must not be changed unless explicitly reopened:

- Native/orange layer: **X = 100, Y = 60**
- Scaleform/black layer: **X = 33.333333333333336, Y = 20**

Other current HUD values retained in cumulative V258Y:

- Tutorial: `X=35`, `Y=30`
- ObjectiveTray: local X magnitude `18`, local Y magnitude `49.666666666666664`
- Inventory/ammo: local `X=+91.66666666666667`, `Y=+51.666666666666664`

## What this repository contains

- [`docs/TECHNICAL_NOTEBOOK.md`](docs/TECHNICAL_NOTEBOOK.md): source of truth for technical findings.
- [`docs/BUILD_HISTORY.md`](docs/BUILD_HISTORY.md): validated/rejected lineage.
- [`docs/HASHES.md`](docs/HASHES.md): hash ledger.
- [`tools/apply_patch.py`](tools/apply_patch.py): fail-closed patch applicator.
- [`patches/`](patches/): canonical byte manifests.

## Development rules

1. **Last validated build is always cumulative.**
2. Prefer surgical, isolated binary changes.
3. Warn before any global or intrusive approach.
4. Rebuild from a clean validated base rather than stacking uncontrolled experiments.
5. If rebuilding from an older milestone, re-apply all later validated changes before promotion.
6. Treat the README/notebook as a laboratory log: base hashes, addresses, constants, formulas, test results, failures, regressions, frozen systems, and next hypotheses.
7. A build is not a new base until validated in-game.
8. **For every newly validated build, commit both**:
   - a delta manifest from the previous canonical build to the new build;
   - a direct cumulative manifest from the original retail EXE to the new build.
   This keeps the latest release independently reproducible from vanilla at all times.

## Current next target

The next open HUD item is the **blue floating/off-screen indicator** that can be clipped at the left edge. The intended fix is to identify and slightly reduce its actual screen-edge clamp/radius, without touching the validated V258Y minimap.

## Disclaimer

This is an unofficial community project and is not affiliated with the original developers or publisher. Back up your original executable before testing any binary patch.
