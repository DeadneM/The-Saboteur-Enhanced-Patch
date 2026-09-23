# Technical notebook

## Current canonical cumulative build

**V259**

Original retail EXE SHA-256:
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V259 EXE SHA-256:
`8f9883883abab91ae029b078326e93751664204e75ebb2f83044ae014d13fc0b`

V259 promotes the in-game validated V259E behavior unchanged.

## Cumulative rule

Every future candidate starts from V259, or must reproduce V259 exactly before adding any experiment.

## Frozen minimap

- Native/orange X = `100`
- Native/orange Y = `60`
- Scaleform/black X = `33.333333333333336`
- Scaleform/black Y = `20`

## World-marker system

World and minimap shop/garage assets are separate:

- `om_shop_AB` / `mm_shop_AB`
- `om_garage_AB` / `mm_garage_AB`
- `om_garage2_AB` / `mm_garage2_AB`
- `om_HQ_AB` / `mm_HQ_AB`

### Generic world-marker scale

Validated path:
`0x00797843 -> 0x00790B26 -> 0x00790B34 -> 0x00790B46`

Hook:
`0x00790B46 -> 0x00681000`

Scale constant:
- VA `0x00681020`
- value `0.70f`

### Generic world-marker vertical anchor

Hook:
`0x0079785D -> 0x00681100`

V259 value:
- VA `0x00681130`
- raw `0x00280330`
- `0.25f`

This replaces the former historical `0.50f` anchor.

### Merchant / shop pistol

Asset:
`om_shop_AB`

Loader identifies type 0 as shop.

V259 hook:
`0x009E18A1 -> 0x00681486`

Behavior:
- only type 0 receives extra `Y += 0.25`
- garage types 1 and 2 do not receive this extra shop-only correction

### Resistance HQ / Cross of Lorraine

World asset:
`om_HQ_AB`

Minimap asset:
`mm_HQ_AB`

HQ manager global:
`0x0143CB64`

V259 hook:
`0x007979A8 -> 0x00681400`

Behavior:
`Y += 0.25`

### Shared extra constant

- VA `0x006814A4`
- value `0.25f`

## V259 validation sequence

- V259C fixed the general world-marker height and was reported otherwise perfect.
- Two blue icon families remained: shop/pistol and HQ/Cross.
- V259E added explicit handling for both.
- User validated V259E.
- V259 is identical in behavior to V259E and is now canonical.

## Rejected recent probes

- V259A: world-marker scale 0.70 -> 0.80, rejected.
- V259B: 95.0 -> 80.0 at 0x01138348, rejected.
- V259D: superseded diagnostic.

## Reproducibility

`V258Y -> V259` manifest:
- changed bytes: 69
- regions: 6

Direct `Original -> V259` manifest generated and locally verified:
- changed bytes: 5,754
- regions: 221
- target SHA-256 exactly matches canonical V259

## Engine-cap audit after V259

The first post-HUD engine audit deliberately separates real capacity ceilings from renderer flags, cache thresholds and maintenance budgets.

### OdinDrawlist / Win32Drawlist

The drawlist registration constants near the DetailObjects, Foliage, Decals and Particle labels are **not currently treated as capacities**.

The Win32Drawlist implementation grows vertex/index storage dynamically. Its grow path at `0x00E21180` calls allocator helpers when current storage is insufficient.

The command-vector path also grows dynamically through `0x00E23370`, rounding capacity upward and reallocating/copying entries.

Conclusion: values such as `0x20`, `0x40`, `0x60` and `0x70` seen during drawlist registration are not proven object caps and remain untouched.

### WSSimpleRenderObject

The WSSimpleRenderObject pool is configured with an initial capacity of **9000** objects (`0x2328`) and object size `0x98`.

However the generic pool allocator can fall back to additional individual allocations when the initial free list is exhausted.

Conclusion: 9000 is a preallocation/cache capacity, not a hard visibility cap. It remains untouched.

### WSSceneBox

`WSSceneBox::SetupBox` contains thresholds **15** and **50** around `0x0063E3DF-0x0063E406`.

Exceeding them changes the spatial-tree merge/setup path rather than dropping scene objects.

Conclusion: these are balancing/merge heuristics, not proven render caps. They remain untouched.

### Foliage

Two tempting maintenance thresholds were identified:

- a cache trim check at **20** around `0x00996510`
- a cleanup/drain budget of **120** around `0x00996609`

Neither is a proven active foliage render cap.

Conclusion: no foliage patch yet.

### Proven hard decal cap

A real active-object ceiling was found in the WSDecal path.

Pool initialization:
- VA `0x0098D7C9`
- `push 0x190`
- capacity = **400**
- object size = `0x210` = **528 bytes**
- pool object = `0x0132ADA8`

Active-list enforcement:
- `0x0098E994: mov ecx,[ebp+0x0C]`
- `0x0098E997: cmp ecx,0x190`

When the active count reaches 400, the code removes/destroys the oldest active decal before creating its replacement.

This is a genuine retained-object cap rather than a rendering registration value.

### V260A test candidate

V260A is built directly from canonical V259 and changes only the decal cap and matching pool capacity:

- pool capacity: `400 -> 800`
- active decal cap: `400 -> 800`

Patch sites:
- `0x0098D7C9: push 0x190 -> push 0x320`
- `0x0098E997: cmp ecx,0x190 -> cmp ecx,0x320`

V260A SHA-256:
`a90acc384bab67b7ac54bb973a81852ab2f8bce232d9215cc569af5f5d725440`

CPU-side initial object storage:
- 400 × 528 = 211,200 bytes
- 800 × 528 = 422,400 bytes
- increase = 211,200 bytes (~206.25 KiB)

V260A is **TEST ONLY**. V259 remains canonical until in-game validation.

## Open issue

The separate clipped blue edge indicator remains unresolved and must be audited independently. Do not reuse the shop/garage/HQ marker assumptions for it without proof.
