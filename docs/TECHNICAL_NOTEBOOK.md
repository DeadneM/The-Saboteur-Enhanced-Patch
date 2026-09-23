# Technical notebook

## Current canonical cumulative build

**V260**

Original retail EXE SHA-256:
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V260 EXE SHA-256:
`a90acc384bab67b7ac54bb973a81852ab2f8bce232d9215cc569af5f5d725440`

## Cumulative rule

Every future candidate starts from V260, or reproduces V260 exactly before adding an experiment.

## Frozen minimap

- Native/orange X = `100`
- Native/orange Y = `60`
- Scaleform/black X = `33.333333333333336`
- Scaleform/black Y = `20`

## Retained world-marker state

- world `om_*` scale = 0.70
- generic world vertical anchor = 0.25
- shop/pistol `om_shop_AB` extra Y correction = +0.25
- HQ/Cross `om_HQ_AB` extra Y correction = +0.25

## Validated V260: WSDecal hard cap

The first post-HUD capacity audit found a genuine retained-object ceiling.

### Pool

`0x0098D7C9`

V259:
`push 0x190` = 400 WSDecal objects

V260:
`push 0x320` = 800

Object size:
`0x210` = 528 bytes

### Runtime enforcement

`0x0098E997`

V259:
`cmp ecx,0x190`

When the active count reaches 400, the code removes/destroys the oldest active decal before creating a replacement.

V260:
`cmp ecx,0x320`

### Memory impact

- 400 × 528 = 211,200 bytes
- 800 × 528 = 422,400 bytes
- increase = 211,200 bytes (~206.25 KiB)

### Validation

V260A was validated in-game and promoted to canonical V260.

## Continued cap audit: WSPhysicsParticle

A second paired pool/runtime ceiling has been identified.

### Class identification

At the allocator setup near `0x009DB5A0`:

First pool:
- type: `WSPhysicsParticleEffect`
- object size: `0x348`
- initial capacity: `0x40` = 64

Second pool:
- type: `WSPhysicsParticle`
- object size: `0x90` = 144 bytes
- initial capacity: `0x3E8` = 1000

### Runtime total

Function around `0x009DB5F0` recomputes a total into:

`[manager + 0xA0]`

It traverses active effect entries, calls the per-effect count/update routine, and accumulates the returned quantity into this field.

### Proven runtime ceiling

At:

`0x009DB66B`

V260 code:

`cmp dword ptr [ebx+0xA0],0x3E8`

followed by:

`jae 0x009DB73C`

Therefore once the current total reaches 1000, the pending particle-processing loop stops creating additional WSPhysicsParticle instances.

The branch target calls:

`0x009DB490`

which removes/drains the remaining pending entries.

This is behavioral suppression, not just preallocation.

### V261A candidate

Built directly from canonical V260.

Changes:

1. WSPhysicsParticle pool:
   - VA `0x009DB5D2`
   - 1000 -> 2000

2. Runtime hard ceiling:
   - VA `0x009DB66B`
   - 1000 -> 2000

Object-size impact:

- extra 1000 × 144 bytes
- +144,000 bytes
- ~140.62 KiB

V261A SHA-256:
`01a048563d10464e54a844bc9be10e12daa33363d9ceee80d3e761728832ddac`

### Deliberately untouched

The nearby `WSPhysicsParticleEffect` pool of 64 objects is not changed. No matching hard active-effect ceiling of 64 has yet been proven.

## Rejected/non-cap interpretations retained

### OdinDrawlist / Win32Drawlist

Vertex/index and command storage grow dynamically. Drawlist registration values are not proven scene-object capacities.

### WSSimpleRenderObject

Initial pool 9000, but allocator can spill to additional allocations. Not a hard visibility cap.

### WSSceneBox

15/50 thresholds choose spatial-tree setup/merge behavior rather than dropping objects.

### Foliage

20-entry cache trimming and 120-item cleanup budgets are maintenance behavior, not proven render caps.

## Reproducibility

V259 -> V260:
- 2 changed regions
- 4 changed bytes

Original -> V260:
- 223 changed regions
- 5,758 changed bytes
- exact V260 target hash verified locally

## Current open work

1. Validate V261A.
2. Continue auditing streaming queues/pending-delete limits and other true pools.
3. Do not alter the frozen minimap.
4. Keep the clipped blue edge indicator separate from engine-cap work.
