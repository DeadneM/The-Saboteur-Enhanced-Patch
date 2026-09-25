# LOD / draw-distance audit

This document records the renderer-distance map discovered during the V275+ audit.

## SliceQuality

`SliceQuality = 0` is the **High** profile. Numeric index is not a quality ranking.

High table base: `0x01120AD8`

Observed High ranges before V275/V277:

- Slice 0: 0.1 -> 4
- Slice 1: 4 -> 20
- Slice 2: 20 -> 50
- Slice 3: 50 -> 100
- Slice 4: 80 -> 500

Validated cumulative state after V275/V277:

- Slice 3 far bound: **300**
- final outer endpoint: **1500**

The runtime adjustment loop around VA `0x00643030` does not overwrite the static upper bounds patched by V275/V277.

## ClipRange

Native values: 180, 270, 360, 1500. High uses `ClipRange = 3`.

## ObjectQuality / human LOD

Vanilla High: 70/150/300.  
Validated V276 High: **100/300/600**.

Low/Medium remain unchanged.

## ModelInfo

Parser record layout:

- +0x01 RenderSlice
- +0x02 ZPassSlice
- +0x03 ShadowSlice
- +0x04 LODDIST float

V278 changes only the default LODDIST source 1000 -> 1500. Explicit `LODDIST25/30` overrides remain unchanged.

## ShadowSlice

No independent High shadow-distance table was found. ShadowSlice uses the same slice classes, so V277's class-3 extension also affects SHADOWSLICE3.

## TextureQuality

Native mapping: 0 -> 128 px, 1 -> 256 px, 2 -> 512 px, 3 -> 32768 px. No artificial quality 4 is needed.

## WSDetailSystem

A dedicated world-detail/decor distance exists at `WSDetailSystem + 0x218`.

Vanilla logic:

- VA `0x007ECD20`: initialize to 100.0
- VA `0x007ECDC3`: compare against a hard maximum of 100.0
- VA `0x007ECDD0`: if over the ceiling, restore 100.0

This explains why changing only the initial value cannot stick.

Validated V280 redirects those three local accesses to existing native 500.0 constants.

V281A then raises only the same local initial/max/reset values from 500 to 1000 using native constants:
- float 1000.0 at `0x00F7D630`
- double 1000.0 at `0x010A45B8`

The lower clamp remains 10.0. The neighboring +0x21C float remains at its native 10.0 / 0..75 clamp because its semantic role is not yet proven as draw distance.

### Rejected historical V236 attribution

The old V236 label that described offsets +0xC04/+0xC08/+0xC0C as WSDetailSystem was disproven. The real DetailSystem registration allocates only `0x240` bytes and binds constructor `0x00440A90`, so offsets beyond +0x240 cannot belong to that class. The historical 10/10/20 experiment is excluded from the modern lineage.

### VeryFarSceneMonuments audit

The true Monuments registration allocates `0x6898` bytes and reaches its own constructor path. No independent global draw-distance scalar analogous to VeryFarSceneTerrain's distance was proven. Two inline structures contain 256 entries with a hard `0x100` check; because those arrays are embedded in the object layout, increasing the count alone would invalidate layout assumptions. This candidate is rejected as non-surgical.

## Streaming-grid coverage

The streaming manager owns two parallel 3-entry float arrays:

Cell size at VA `0x0104B038`:
- Low 500
- Medium 60
- High 25

Current validated V281 coverage at VA `0x0104B044`:
- Low 8000
- Medium 1600
- High 1250

The runtime scan around VA `0x00A04B7D` divides `coverage[tier] / cell_size[tier]` while iterating tier index 0..2.

Approximate V281 radii:
- Low 16 cells
- Medium 26.67 cells
- High 50 cells

V282A modifies only `coverage[2]` at VA `0x0104B04C` / RAW `0x00C4A24C` from 1250 to 2500, producing a High radius of 100 cells.

This is intentionally narrower than historical V231A, which multiplied all three tiers at once. Runtime impact can still be substantial because doubling a 2D radius may expose roughly four times the area to the High grid.

Validated V282 therefore has:
- Low radius 16
- Medium radius ~26.67
- High radius 100

Validated V283 therefore has:
- Low radius 16
- Medium radius ~53.33
- High radius 100

V284A modifies only `coverage[0]` at VA `0x0104B044` / RAW `0x00C4A244` from 8000 to 16000, producing a Low radius of 32 cells. Medium remains 3200 and High remains 2500.

## HighPalette per-resource priority

The resource-priority classifier around VA `0x009EE2B0` has a six-way candidate-type jump table. Candidate type 4 reaches the path at VA `0x009EE424`.

That path computes:

`metric = resource[+0x1F8] / resource[+0x1F4]`

and compares the result at VA `0x009EE461` against a double threshold. In V284 the source is the shared native 80.0 constant at `0x00FB54B0`.

Historical V232 proved this compare can be isolated, but used a private 320.0 double stored in executable padding. V285A improves the method by redirecting only this one compare to an existing native 160.0 double at `0x00FA4110`.

Patch:
- VA `0x009EE461`
- RAW `0x005ED661`
- operand source `0x00FB54B0 -> 0x00FA4110`
- threshold `80.0 -> 160.0`
- exactly 3 effective bytes

No global 80.0 constant, palette data, streaming-grid radius or fallback logic is modified.

Validated progression:

- V285: 160.0, validated
- V286: 200.0, validated
- V287: 250.0, validated
- V288: 300.0, validated
- V289: 400.0, validated
- V290: 500.0, validated
- V291: 650.0, validated

Each build redirects only the absolute operand of the compare at VA `0x009EE461`; the referenced native constants are not modified.

V292A advances the same compare to the unique engine-native double 800.0 at `0x00FD8CF8`. No native double 700.0 or 750.0 exists, so 800.0 is the next clean native step after 650.0. Exactly three operand bytes change from V291.

The neighboring candidate-type-5 path compares the same resource ratio against 10.0, but semantic ownership remains unproven. It remains untouched.

## Water

Developer tuner exposes Water LOD Dist = 7.0 and Water LOD Scale = 0.02. Untouched.

## VeryFarScene / FarFarScene

Distinct systems observed:

- DetailSystem
- VeryFarScene
- VeryFarSceneTerrain
- VeryFarSceneMonuments
- FarFarScene GeometryDisk

V279 redirects only four VeryFarSceneTerrain-specific loads from 5000 to native 10000. Monuments and DetailSystem remain separate.

### GeometryDisk finding

Historical V233A/V233B was recovered and compared byte-for-byte:

- both archives contain **identical Saboteur.exe files**
- V233A changed only `tuner.txt`: `OuterRadius=25`
- V233B changed only `tuner.txt`: `OuterRadius=400`

Therefore the old visual effect proves the tuner parameter is live, but it does **not** reveal a native EXE patch site.

Loading the full developer tuner would re-inject more than a thousand parameters, so that method is deliberately kept outside the canonical EXE lineage. A native owner/storage path must be proven before GeometryDisk is promoted into a normal cumulative build.

The tuner values remain:

- `FarFarScene.FarFarScene.GeometryDisk.OuterRadius = 100`
- RadialSegments = 8
- CircularSegments = 8

## Distant red-prop issue

The distant red-prop/fallback issue remains separate. Neutralizing the fallback can hide the red proxy by removing the prop, which is not considered a real fix.
