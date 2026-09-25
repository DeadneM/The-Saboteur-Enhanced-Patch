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

Parser record layout (corrected from the consumer path):

- +0x00 flags, including ZCULL
- +0x01 ShadowSlice
- +0x02 RenderSlice
- +0x03 ZPassSlice
- +0x04 LODDIST float
- +0x08 foliage-related flag

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
- V292: 800.0, validated
- V293: 900.0, validated
- V294: 1000.0, validated
- V295: 1500.0, validated
- V296: 1600.0, validated

Each build redirects only the absolute operand of the compare at VA `0x009EE461`; the referenced native constants are not modified.

V292 advances the same compare to the unique engine-native double 800.0 at `0x00FD8CF8` and is validated. No native double 700.0 or 750.0 exists.

V293 advances that same local compare to the unique native double 900.0 at `0x00F82660` and is validated.

V294 advances the same compare to the unique native double 1000.0 at `0x010A45B8` / RAW `0x00CA37B8` and is validated.

V295 is validated at 1500.0 using the read-only .rdata copy at VA `0x010207E0`.

V296A advances the same local compare to the unique native double 1600.0 at VA `0x010140A8` / RAW `0x00C132A8`. The compare remains at VA `0x009EE461`; exactly three operand bytes change from V295. The 1600.0 constant is referenced only, not edited.

V297A EXTREME replaces incremental tuning with an upper-bound stress test. The same compare is redirected to the engine-native double 1,000,000,000.0 at VA `0x00F86DE0` / RAW `0x00B85FE0`. This is intended to make the HighPalette threshold effectively non-limiting for ordinary metric values while preserving the original compare and control flow.

If the extreme value causes instability or excessive memory/frame-time cost, planned fallback values using the same isolated operand are 100000, 10000, 5000, and 2000.

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

The distant red-prop/fallback issue is now being re-audited through the native Will to Fight rendering path. Neutralizing the old fallback remains rejected because it can hide the prop rather than fix its material.


### HighPalette branch closure candidate

V296 validates 1600.0.

V297A closes the HighPalette branch with a deliberate x4 stress step:
- 1600 -> 6400
- native 6400.0 at VA `0x01011D00`
- same compare VA `0x009EE461`
- 2 effective operand bytes changed

A x10 target of 16000.0 was considered but rejected because no native double 16000.0 exists. Injecting a custom constant would violate the current surgical rule when a clean x4 native step exists.

The 1e9 threshold experiment is rejected as excessive and excluded.

## V298 RenderSlice semantic correction and retained expansion

A later control-flow audit corrected the earlier direct mapping assumption.

ModelInfo RENDERSLICE n is converted to:

    mask = (1 << n) - 1

This means RENDERSLICE3 covers slices 0+1+2 and is bounded by the class-2 far edge. Consequently, the historical V277 record-3 100 -> 300 change did not by itself move the many RENDERSLICE3 props from ~50 to 300.

V298 therefore expands all relevant High render-slice boundaries while retaining the runtime table mechanics:

- effective Slice0 far: 4 -> 16
- effective Slice1 far: 20 -> 80
- effective Slice2 far: 50 -> 200
- Slice3 far: 300 -> 1200
- outer/final far: 1500 -> 6000

Patched fields:
- record1 start VA 0x01120AE4 / RAW 0x00D1F4E4: 4 -> 16
- record2 start VA 0x01120AF0 / RAW 0x00D1F4F0: 20 -> 80
- record3 start VA 0x01120AFC / RAW 0x00D1F4FC: 50 -> 200
- record3 far VA 0x01120B00 / RAW 0x00D1F500: 300 -> 1200
- record4 far VA 0x01120B0C / RAW 0x00D1F50C: 1500 -> 6000

User reported no obvious visible improvement or regression in V298, but explicitly requested that these increases remain in all later builds. V298 became the retained distance baseline at that point. V302 now supersedes it as the retained cumulative baseline.

## WTF / distant red-material branch

The user observed that Nazi-occupied zones intentionally transform the world to black/white/red and that some grey metallic materials become red under this state. Static evidence now links this to the native Will to Fight pipeline.

WSWillToFightGrid owns the PC-default low-resolution WTF influence representation:
- LowResWorldWTF 256x256
- LowResWorldWTFVertex 256x256
- matching 8-bit CPU influence buffer
- bilinear influence sampling normalized by 255

V299A is a surgical resolution test only:
- all three WTF spatial dimensions 256 -> 1024
- all V298 RenderSlice changes retained
- all WTF palette/color and intensity semantics untouched

If V299A changes the red-material bug, low-resolution WTF spatial interpolation becomes strongly implicated. If it is identical, the next work should target the WTF material/color calculation rather than the influence-grid resolution.


## V302 retained High table and near-object-pop closure tests

V302 is the current retained baseline.

It starts from V298 and scales the retained High/Q0 distance structure coherently:
- effective Slice0 far: 24.9765625
- effective Slice1 far: 124.8828125
- effective Slice2 far: 312.20703125
- Slice3 far: 1873.2421875
- Slice4 start: 124.8828125
- outer/final far: 9366.2109375

V302 EXE SHA-256:
`db96ff3f6f8e38acdd262a87b0bb0ded7c604abca82e86e2501a12347f512459`

Validation:
- scenery OK
- distant red issue still visible

### Q2 diagnostics

Subsequent tests proved that Q2 is a live SliceQuality profile in the problematic scene:
- Q1 Slice4.first only: no red change, scenery OK
- Q2 Slice4.first 80 -> 125: red hidden, scenery broken
- Q2 Slice4.first 80 -> 124.8828125: scenery broken
- entire Q2 table x1.5625: scenery broken
- Q2 RENDERSLICE3 terminal far 50 -> 200: no visible improvement to the very-near object pop

Conclusion:
1. Q2 Slice4 is coupled to scene visibility strongly enough that using it to hide the red symptom is not viable.
2. The newly reported ~3 m object pop is not controlled by Q2 record2 far / RENDERSLICE3.
3. Future audit must look beyond the already-pushed global SliceQuality bands and identify object-family-specific visibility gates, per-instance/detail culling, model bounds/size classifiers, activation spheres, or other short-range systems.


## WSModel size-derived hard visibility cull

This is the strongest remaining explanation for the very-near small-prop pop captured after V302.

### Automatic model-size classifier

During WSModel setup around VA `0x006394D0`, models without a matching explicit ModelInfo entry are classified using the float at `WSModel+0x58`, which behaves as a model size/radius metric.

Dedicated thresholds:
- VA `0x0112084C` / RAW `0x00D1F24C`: 0.3
- VA `0x01120850` / RAW `0x00D1F250`: 0.6
- VA `0x01120854` / RAW `0x00D1F254`: 4.0

Automatic render masks:
- metric < 0.3 -> mask 0x03 -> slices 0+1
- 0.3 <= metric < 0.6 -> mask 0x07 -> slices 0+1+2
- 0.6 <= metric < 4.0 -> mask 0x0F -> slices 0+1+2+3
- metric >= 4.0 -> mask 0x1F -> all five slices

This explains why tiny unlisted clutter can remain short-ranged even when explicit ModelInfo RENDERSLICE3 entries and global slice tables are expanded.

### Per-model hard camera-depth cutoffs

WSModel constructor initializes:
- `WSModel+0xA8 = 10000.0`
- `WSModel+0xAC = 10000.0`

Setup then derives shorter values from the size metric.

For metric < 1.5:
`A8 = 20 + (metric / 1.5) * 90 = 20 + 60*metric`

For metric < 5.0:
`AC = 15 + (metric / 5.0) * 100 = 15 + 20*metric`

The metric is first clamped to at least 0.1.

Relevant constants:
- 1.5 at VA `0x00F7D648`
- 90 at VA `0x00FC0E68`
- 20 at VA `0x00F8CCE8`
- 5 at VA `0x00F81DE8`
- 100 at VA `0x00F7BF80`
- 15 at VA `0x00F82618`

At VA `0x00638710`, the engine computes camera-forward depth of the model and compares it against these fields.

If depth exceeds `WSModel+0xA8`, the output render mask is set to zero. The model is therefore hard-culled, not merely switched to a lower LOD.

If depth exceeds `WSModel+0xAC`, the low nibble of the packed mask is cleared. Based on the ModelInfo packing path, this affects the shadow portion rather than the main render mask.

The hard-cull routine is called from:
- VA `0x0063922F`
- VA `0x0048C755`

Both call sites are gated by byte global `0x01210FC4`. The semantic setting name for this gate remains unproven.

### Surgical next diagnostic

The narrowest diagnostic is to leave every SliceQuality table untouched and prevent only the small-model `A8` rewrite, allowing constructor default 10000.0 to survive.

At setup:
- VA `0x0063954E`: `jp 0x0063956A`
- RAW `0x0023874E`
- bytes `7A 1A`

Changing only the conditional jump to an unconditional short jump:
- `7A 1A -> EB 1A`

would skip only the A8 formula/store while preserving x87 stack behavior and continuing into the existing AC/shadow-distance calculation.

This is a one-byte diagnostic candidate. It is not yet part of the retained build.

## WSSphereActivator real radius cap

V266 increased only the fixed WSActivateSphere pool capacity from 256 to 512. It did not alter activation radius.

The actual sphere-create routine is VA `0x0068EF80`.

It computes:
`effective_radius = min(requested_radius * 1.05, 2.06)`

Constants:
- 2.06 at VA `0x00FCD834`
- 1.05 at VA `0x00FCD838`

Callers include VA `0x0066CD74`, `0x009FADCA`, `0x00A0111E`, and `0x00A029F0`. Several callers derive the requested radius from a model/object `+0x58` size metric.

The sphere objects are transient and lifetime-driven, and the system performs spatial queries/activation work. The 2.06 cap is therefore a real short-range engine limit, but it is not yet proven to own static scenery rendering. A global radius increase could affect gameplay, physics, AI or trigger behavior and must not be applied before ownership is proven.

## Near-pop audit conclusion after V308

The supplied garage/workshop capture is not explained by:
- Q2 RENDERSLICE3 terminal far (V308 50 -> 200 had no visible effect)
- streaming-grid coverage
- WSDetailSystem +0x218
- VeryFarSceneTerrain
- ModelInfo default LODDIST
- HighPalette priority

The strongest remaining renderer-specific candidate is the WSModel size-derived A8 hard visibility cutoff. WSSphereActivator and global occlusion remain secondary candidates.
