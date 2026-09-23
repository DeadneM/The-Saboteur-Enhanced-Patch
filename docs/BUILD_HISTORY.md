# Build history and validation state

## Cumulative-build rule

**The newest validated build is always the complete cumulative patch.**

Current canonical cumulative build: **V260**

Original retail EXE SHA-256:
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V260 EXE SHA-256:
`a90acc384bab67b7ac54bb973a81852ab2f8bce232d9215cc569af5f5d725440`

## Validated lineage

- **V137**: dual-layer minimap transform validated.
- **V138**: proved world `om_*` and minimap `mm_*` shop/garage assets are separate.
- **V140/V147**: world `om_*` scale 70%.
- **V146D/V147**: historical world vertical anchor 0.50.
- **V200**: cumulative streaming/HUD baseline.
- **V255A**: native 100% sniper-scope exception.
- **V257**: Max Engine baseline.
- **V258G/M/P/W/Y**: ObjectiveTray/minimap/final HUD lineage.
- **V259**: final world-marker geometry, shop/pistol and HQ/Cross corrections.
- **V260**: validated active decal-cap improvement:
  - active WSDecal ceiling 400 -> 800
  - matching WSDecal pool 400 -> 800

## V260 candidate history

- **V260A**: built directly from V259.
- Proven hard cap at `0x0098E997`: active decal count 400.
- Matching WSDecal pool at `0x0098D7C9`: 400 objects.
- Candidate raised both to 800.
- User verdict: **"cest valider on continu"**.
- V260A behavior is promoted unchanged as canonical **V260**.

## V261 candidate history

- **V261A TEST**: current test candidate.
- Identified class: `WSPhysicsParticle`.
- Pool initialization:
  - `0x009DB5D2`
  - 1000 -> 2000
- Matching runtime ceiling:
  - `0x009DB66B`
  - `cmp [ebx+0xA0],1000`
  - jumps out when total is >=1000
- The exit path calls `0x009DB490`, which drains/removes pending entries instead of continuing particle creation.
- The nearby 64-object `WSPhysicsParticleEffect` pool remains untouched because no matching hard 64-effect ceiling has been proven.
- V261A is **not canonical** until in-game validation.

## Frozen minimap

- Native/orange: X = 100, Y = 60
- Scaleform/black: X = 33.333333333333336, Y = 20

## Audit guardrails

Not currently treated as hard visibility/render caps:

- OdinDrawlist/Win32Drawlist registration values
- WSSimpleRenderObject 9000 initial pool
- WSSceneBox 15/50 spatial thresholds
- Foliage 20-entry trim threshold
- Foliage 120-item cleanup budget
- WSPhysicsParticleEffect 64-object initial pool

Future validated work starts from **V260**.
