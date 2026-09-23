# The Saboteur Enhanced Patch

Experimental PC enhancement patch for **The Saboteur**, developed through static binary auditing and in-game validation.

> **Current validated cumulative build: V259**  
> **Current Windows patcher: v1Pv259**

## Windows patcher

The project now uses the same distribution model as the Call of Juarez: Gunslinger patch:

`The_Saboteur_Enhanced_Patcher_v1Pv259.exe`

Patcher generation:

- **v1P**

Cumulative game payload:

- **V259**

Supported exact source states:

- untouched retail `Saboteur.exe`
- validated V258Y
- V259 itself, detected as already installed

The patcher refuses unknown or modified executables.

It bundles compact direct `SABDP1` deltas, not a retail game executable. Before installation it verifies the source SHA-256 and every original byte region, reconstructs V259, verifies the exact V259 SHA-256, creates a backup, stages replacement with rollback, then verifies the installed file again.

Expected V259 target SHA-256:

`8f9883883abab91ae029b078326e93751664204e75ebb2f83044ae014d13fc0b`

The Windows build is produced by GitHub Actions using Python 3.12 and PyInstaller 6.16.0. The first verified CI build completed successfully.

Patcher CI-build SHA-256:

`bd6a74fc911829e99df7a43c7a70d54eecc561933e3b3bceceb8b0b81b8b3733`

See [`installer/README.md`](installer/README.md) for installer architecture and [`.github/workflows/build-patcher.yml`](.github/workflows/build-patcher.yml) for the reproducible Windows build.

## Golden rule: every current build is cumulative

**The latest validated build contains every retained validated change from the original executable up to that point.**

V259 is the full current patch state.

Original EXE SHA-256:

`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

Current V259 EXE SHA-256:

`8f9883883abab91ae029b078326e93751664204e75ebb2f83044ae014d13fc0b`

## V259 validation

V259 promotes the in-game validated V259E behavior unchanged.

Validated world-marker behavior:

- generic world `om_*` marker vertical anchor: `0.50 -> 0.25`
- world `om_*` scale remains `0.70`
- merchant/shop pistol `om_shop_AB`: dedicated +0.25 Y correction
- Resistance HQ / Cross of Lorraine `om_HQ_AB`: dedicated +0.25 Y correction
- garage markers remain on the validated generic path
- minimap remains frozen and separate

## Reproducible cumulative paths

### Latest delta

- [`V258Y -> V259`](patches/v258y_to_v259.json)

### Historical chain

- [`Original -> V200`](patches/original_to_v200.json.zlib.b64)
- [`V200 -> V255A`](patches/v200_to_v255a.json)
- [`V255A -> V257`](patches/v255a_to_v257.json)
- [`V257 -> V258G`](patches/v257_to_v258g.json)
- [`V258G -> V258M`](patches/v258g_to_v258m.json)
- [`V258M -> V258P`](patches/v258m_to_v258p.json)
- [`V258P -> V258W`](patches/v258p_to_v258w.json)
- [`V258W -> V258Y`](patches/v258w_to_v258y.json)
- [`V258Y -> V259`](patches/v258y_to_v259.json)

## Frozen minimap

Do not change unless explicitly reopened:

- Native/orange: **X = 100, Y = 60**
- Scaleform/black: **X = 33.333333333333336, Y = 20**

## Development rules

1. **Last validated build is always cumulative.**
2. Prefer surgical, isolated binary changes.
3. Warn before any global or intrusive approach.
4. Rejected experiments never become part of the canonical base.
5. Every future candidate starts from V259 or reproduces V259 exactly before adding a new change.
6. Every validated build updates the patcher payload, technical notebook, build history and hashes.

## Current open work

The separate clipped blue edge indicator remains unresolved and must be audited independently from the validated merchant/garage/HQ world-marker work.

## Disclaimer

Unofficial community project. Back up the original executable before testing.
