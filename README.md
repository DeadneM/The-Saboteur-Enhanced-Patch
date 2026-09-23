# The Saboteur Enhanced Patch

Experimental PC enhancement patch for **The Saboteur**, developed through static binary auditing and in-game validation.

> **Current validated cumulative build: V260**  
> **Current Windows patcher source/CI target: v1Pv260**  
> **Current test candidate: V261A**

## Windows patcher

The cumulative patcher has been advanced to target **V260**.

Designation:

`The_Saboteur_Enhanced_Patcher_v1Pv260.exe`

Supported exact source states:

- untouched retail `Saboteur.exe`
- validated V258Y
- validated V259
- V260 itself, detected as already installed

The patcher refuses unknown or modified executables.

It bundles compact direct `SABDP1` deltas, not a retail game executable. It verifies source SHA-256, every source byte region in the delta, reconstructed target SHA-256, stages replacement with rollback, creates a backup only after recognition, and verifies the installed executable again.

Expected V260 target SHA-256:

`a90acc384bab67b7ac54bb973a81852ab2f8bce232d9215cc569af5f5d725440`

Public release publication remains a manual GitHub Actions step through **Publish Patcher Release**.

## Golden rule: every current build is cumulative

**The latest validated build contains every retained validated change from the original executable up to that point.**

V260 is the full current patch state.

Original EXE SHA-256:

`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

Current V260 EXE SHA-256:

`a90acc384bab67b7ac54bb973a81852ab2f8bce232d9215cc569af5f5d725440`

## V260 validation

V260 promotes the validated V260A decal-cap candidate.

Validated change:

- WSDecal active ceiling: **400 -> 800**
- matching WSDecal pool capacity: **400 -> 800**
- object size: `0x210` = 528 bytes
- additional initial object storage: ~206.25 KiB

Patch sites:

- `0x0098D7C9: push 0x190 -> push 0x320`
- `0x0098E997: cmp ecx,0x190 -> cmp ecx,0x320`

User validation:
**"cest valider on continu"**

## Current V261A test

The next proven hard ceiling was found in the physics-particle manager.

V261A tests:

- `WSPhysicsParticle` pool: **1000 -> 2000**
- matching runtime hard ceiling: **1000 -> 2000**
- object size: `0x90` = 144 bytes
- additional initial pool storage: ~140.62 KiB

Patch sites:

- `0x009DB5D2: push 0x3E8 -> push 0x7D0`
- `0x009DB66B: cmp [ebx+0xA0],0x3E8 -> 0x7D0`

V261A remains test-only until in-game validation.

## Reproducible cumulative paths

Latest validated delta:

- [`V259 -> V260`](patches/v259_to_v260.json)

Direct cumulative:

- [`Original -> V260`](patches/original_to_v260.json.zlib.b64)

Historical chain remains in `patches/`.

## Frozen minimap

Do not change unless explicitly reopened:

- Native/orange: **X = 100, Y = 60**
- Scaleform/black: **X = 33.333333333333336, Y = 20**

## Development rules

1. **Last validated build is always cumulative.**
2. Prefer surgical, isolated binary changes.
3. Warn before any global or intrusive approach.
4. Rejected experiments never become part of the canonical base.
5. Every future candidate starts from V260 or reproduces V260 exactly before adding a new change.
6. Patch only proven hard caps/pools/queues, not suspicious constants without control-flow proof.
7. Every validated build updates manifests, documentation and the patcher target.

## Current open work

- Validate V261A physics-particle ceiling.
- Continue audit of real scene/streaming/pool/queue limits.
- Keep the separate clipped blue edge indicator as an independent HUD task.

## Disclaimer

Unofficial community project. Back up the original executable before testing.
