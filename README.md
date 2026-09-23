# The Saboteur Enhanced Patch

Experimental PC enhancement patch for **The Saboteur**, developed through static binary auditing and in-game validation.

> **Current validated cumulative build: V259**  
> V259 promotes the validated V259E behavior unchanged.

## Golden rule: every current build is cumulative

**The latest validated build contains every retained validated change from the original executable up to that point.**

V259 is the full current patch state.

Original EXE SHA-256:

`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

Current V259 EXE SHA-256:

`8f9883883abab91ae029b078326e93751664204e75ebb2f83044ae014d13fc0b`

## V259 validation

V259 is based on the V259E candidate that was validated in-game.

Validated world-marker behavior:

- generic world `om_*` marker vertical anchor: `0.50 -> 0.25`
- world `om_*` scale remains `0.70`
- merchant/shop pistol `om_shop_AB`: dedicated +0.25 Y correction
- Resistance HQ / Cross of Lorraine `om_HQ_AB`: dedicated +0.25 Y correction
- garage markers remain on the validated generic path
- minimap remains frozen and separate

The user validated V259E with: **"oui tres bien on valide"**.

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

A direct Original -> V259 cumulative manifest was generated and verified locally against the final V259 SHA-256.

## Frozen minimap

Do not change unless explicitly reopened:

- Native/orange: **X = 100, Y = 60**
- Scaleform/black: **X = 33.333333333333336, Y = 20**

## Other retained HUD values

- Tutorial: `X=35`, `Y=30`
- ObjectiveTray: X magnitude `18`, Y magnitude `49.666666666666664`
- Inventory/ammo: `X=+91.66666666666667`, `Y=+51.666666666666664`

## Development rules

1. **Last validated build is always cumulative.**
2. Prefer surgical, isolated binary changes.
3. Warn before any global or intrusive approach.
4. Rejected experiments never become part of the canonical base.
5. Every future candidate starts from V259 or reproduces V259 exactly before adding a new change.
6. Every validated build updates the technical notebook, build history and hashes.

## Current open work

The separate clipped blue edge indicator remains unresolved and must be audited independently from the now-validated merchant/garage/HQ world-marker work.

## Disclaimer

Unofficial community project. Back up the original executable before testing.
