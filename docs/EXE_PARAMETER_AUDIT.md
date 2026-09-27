# Complete EXE parameter audit for INI migration

Status: **active audit / Core 1 + ASI 0.8**  
Retail EXE: **14,834,176 bytes**  
Retail SHA-256: `e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

## Audit rule

The cleaned Core stays limited to bootstrap/system/UI/correctness work.

Engine/render constants are migrated to `SaboteurEnhanced.ini` only when the
owner is identified and the ASI can fail closed by verifying the native
Core1/retail bytes or source operand before writing.

A historical value is not enough by itself. Old V200/V257 code-cave constants
must be traced back to their native/runtime owner before they become a real INI
control.

## A. Implemented and independently configurable

### Graphics

| INI key | Native | Project value | Owner/status |
|---|---:|---:|---|
| `Graphics.EnvironmentMapResolution` | 128 | 2048 | clean native owner, migrated |
| `Graphics.AnisotropicFiltering` | 4 | 16 | sampler state family, migrated |
| `Graphics.MipLODBias` | 0.0 | -0.25 | sampler state family, migrated |
| `Graphics.ToneMap` | 0.25 | 0.15 | clean scalar owner, migrated |

### Shadows

| INI key | Native | Project value | Owner/status |
|---|---:|---:|---|
| `Shadows.ShadowMapResolution` | 1024 | 4096 | main shadow map dimensions |
| `Shadows.ShadowPCF5x5` | 0 | 1 | exact 3x3 -> recovered 5x5 parameter-family redirects |
| `Shadows.CSMQuality` | 2 | 5 | internal quality selector, not a literal cascade count |
| `Shadows.CSMLambda` | 0.50 | 0.60 | cascade split scalar |
| `Shadows.CSMFarDistance` | ~100 | 150 | private far-minus-0.1 constant |
| `Shadows.DepthBiasScale` | 1.00 | 0.75 | five-entry depth-bias table |
| `Shadows.SlopeBiasScale` | 1.00 | 0.90 | five-entry slope-bias table |

Historical note: the engine layout recovered during the project contains four
CSM cascades. `CSMQuality=5` is therefore deliberately documented only as an
internal quality selector.

### Ambient occlusion

| INI key | Native | Project value | Owner/status |
|---|---:|---:|---|
| `AmbientOcclusion.FullResolution` | 0 | 1 | half-res -> full-res buffer/sample path |
| `AmbientOcclusion.BlurScale` | 2.0 | 1.25 | AO filter scalar |
| `AmbientOcclusion.ErodeScale` | 2.0 | 1.25 | AO filter scalar |

### Streaming and broad distance

| INI key | Native | Project value | Owner/status |
|---|---:|---:|---|
| `Streaming.CoverageLow` | 1500 | 16000 | streaming-grid coverage table |
| `Streaming.CoverageMedium` | 300 | 3200 | streaming-grid coverage table |
| `Streaming.CoverageHigh` | 250 | 2500 | streaming-grid coverage table |
| `Distances.FarScene` | 200 | 320 | three-value FarScene table |
| `Distances.DecalVisibility` | 120 | 160 | stored internally as squared distance |

## B. New clean native owners migrated in ASI 0.8

| INI key | Native | Default/project value | Evidence |
|---|---:|---:|---|
| `Distances.RenderSlice3HighFar` | 100 | 300 | V277 validated; ShadowSlice shares the same slice classes |
| `Distances.RenderSliceHighOuter` | 500 | 1500 | V275 validated |
| `Distances.ModelInfoDefaultLODDistance` | 1000 | 1500 | V278 validated; explicit LODDIST25/30 still override |
| `Distances.VeryFarSceneTerrain` | 5000 | 10000 | V279 validated; four terrain-only loads |
| `Experimental.HighPaletteThreshold` | 80 | 80 | exact SS_HighPalette compare owner; historical stable progression reached 1600 |

### 0.8 implementation details

`RenderSlice3HighFar` writes the High table field at RVA `0x00D20B00`.

`RenderSliceHighOuter` writes the final High endpoint at RVA
`0x00D20B0C`.

`ModelInfoDefaultLODDistance` redirects only the ModelInfo initializer
instruction at RVA `0x00238F2D` from the native shared 1000.0 source to an
ASI-owned float. The global 1000 constant is not modified.

`VeryFarSceneTerrain` redirects only the four terrain-specific `fld`
operands at RVAs:

- `0x004014CB`
- `0x004014F7`
- `0x00401526`
- `0x00401A8B`

The shared native 5000.0 constant remains untouched.

`HighPaletteThreshold` redirects only the SS_HighPalette compare at RVA
`0x005EE461` to ASI-owned double storage. The default stays native 80 because
the historical 80 -> 160 -> 200 -> 250 -> 300 -> 400 -> 500 -> 650 -> 800 ->
900 -> 1000 -> 1500 -> 1600 lineage proved stability, not a unique visual gain.

## C. Validated findings that still need a clean native/runtime owner

These settings existed in the mature branch but were built from V200/V257
injected tables or cave redirections. They must not be represented by fake INI
controls.

| Historical finding | Historical/native-to-project value | Status |
|---|---|---|
| Object LOD family | 20/40/56/80/120/160/240 -> 25/50/70/100/150/200/300 | old local cave table; native consumers must be recovered |
| Foliage distance | 200 -> 250 | old V257 cave family; owner recovery required |
| Shadow-caster distance | 180 -> 240 | two historical sites; clean owner not yet reconstructed |
| Particle LOD distance | 100 -> 150 | old V257 cave family; clean owner not yet reconstructed |
| Spot-shadows 4K factor | 0.5 -> 1.0 | historical result retained, exact Core1 owner not yet proven |
| WSDetailSystem | 100 -> 500 -> 1000 | real object field at +0x218 proven; three instruction owners known, exact Core1 bytes still need reconstruction before generic INI support |
| VeryFarScene profile thresholds | 22 / 49 | real per-profile thresholds; V312 x4 did not affect balcony symptom |
| WSSphereActivator max radius | approx. 2.06 | real clamp in VA 0x0068EF80; affects a broader activation/query system and needs focused ownership proof |
| Water LOD | 7.0 / 0.02 | developer-tuner values only; native executable owner not proven |
| FarFarScene GeometryDisk | historical 25 / 400 tuner A/B | tuner-only evidence; no native owner proven |

## D. Capacity / pool values worth exposing later

These are exact engine limits discovered and historically validated. They are
good INI candidates, but the new architecture requires telemetry or paired
allocation/cap verification before enabling them by default.

| System | Native | Historical validated | Important ownership notes |
|---|---:|---:|---|
| WSDecal | 400 | 800 | allocation/active ceiling pair |
| WSPhysicsParticle | 1000 | 2000 | pool plus runtime ceiling |
| Havok TOI queue | 512 | 1024 | `SizeOfToiEventQueue` |
| WSParticleRender arena A | 4500 | 9000 | allocation plus all matching caps |
| WSParticleRender arena B | 1000 | 2000 | allocation plus matching cap |
| WSParticleRender arena C | 500 | 1000 | allocation plus 499/500 class checks |
| WSParticleRender sort scratch A | 4500 | 9000 | scratch size follows arena capacity |
| WSParticleRender sort scratch B | 1000 | 2000 | scratch size follows arena capacity |
| WSPhGridObject | 1000 | 2000 | fixed pool plus matching active-count gates |
| WSActivateSphere pool | 256 | 512 | pool capacity only; separate from 2.06 radius clamp |
| WSParticleInfoData | 1400 | 2800 | fixed pool |
| WSParkingSpace | 32 | 64 | fixed pool |
| WallPoint | 50 | 100 | must remain paired with WallSegment |
| WallSegment | 50 | 100 | must remain paired with WallPoint |
| WSLuaCall | 20 | 40 | fixed pool, object size 0x110 |
| WSDamageSphere | 512 | 1024 | fixed pool |
| WSReadJob | 1200 | 2400 | shared source with WSUncompressJob |
| WSUncompressJob | 1200 | 2400 | shared source with WSReadJob |
| WSInventoryStateStow | 32 | 64 | fixed pool |
| PblCRCTreeNode | 40000 | 60000 | WORD links + 0xFFFF sentinel; never cross 65534 |

### Pool settings deliberately not generalized blindly

- WSAICorpse has an independent inline 20-pointer manager array. Raising only
  the pool would corrupt assumptions.
- VeryFarSceneMonuments contains inline 256-entry structures. A count-only
  expansion is structurally unsafe.
- Spill-enabled reserves are not useful INI knobs unless runtime pressure proves
  the reserve itself is the bottleneck.

## E. Streaming performance knobs worth instrumenting before exposure

Historical V200 work identified several possible controls:

- async read count (historical 16 -> 32)
- streaming buffer size (historical 32 MiB increase)
- merged/coalesced read sizes
- WSReadJob / WSUncompressJob capacities
- Low/Medium/High grid coverage

Only coverage is currently exposed. The ASI should record queue depth, read-job
peaks, full-queue events and stalls before bringing the remaining throughput
knobs back.

The native 64-entry FIFO itself is not treated as a quality knob. Core 1 keeps
the validated full-queue drop-oldest correctness fix.

## F. Real renderer/culling parameters found but not promoted

### WSModel small-object policy

Automatic render-mask thresholds:

- metric < 0.3 -> mask 0x03
- metric < 0.6 -> mask 0x07
- metric < 4.0 -> mask 0x0F
- metric >= 4.0 -> mask 0x1F

Camera-depth fields start at 10000, then native code rewrites:

- A8 = 20 + 60 * metric for metric < 1.5
- AC = 15 + 20 * metric for metric < 5.0

V309 bypassing the A8 rewrite produced no visible improvement and is rejected.
V310/V311 are the retained corrections instead.

### WSFarSceneObject

The render path compares depth against two class-local FarScene values. The
historical V313 320 -> 1280 test did not solve the balcony/facade symptom. The
general `Distances.FarScene` control already owns the underlying three-value
table, so no duplicate WSFarSceneObject INI key is created.

### WTF / distant red-material pipeline

Recovered shader/material owners include:

- `g_vWTFGreyscale` / `g_vWTFGreyscale2`
- `g_vWTFAmbientHigh` / `g_vWTFAmbientLow`
- `g_vWTFDiffuseHigh` / `g_vWTFDiffuseLow`
- `vWTFColor`
- `fWTFIntensity` / `smpWTFIntensity`
- `WSWillToFightFilter.hlsl`
- `WSWillToFightZone.hlsl`

The PC path also owns a 256x256 CPU influence grid plus
`LowResWorldWTF` and `LowResWorldWTFVertex` 256x256 resources.

The V299 256 -> 1024 diagnostic did not establish a fix and is not enabled by
default. A future INI control for WTF grid resolution should remain
experimental and should patch CPU/GPU dimensions coherently.

## G. Findings deliberately not exposed

- `TextureQuality=3` already maps to a practical 32768-pixel ceiling.
  A fictional quality 4 adds no useful quality.
- V303-V308 SliceQuality diagnostics hid the red symptom in some cases while
  breaking scenery. Those combinations must not be presented as enhancement
  presets.
- V314 ModelInfo ZCULL did not affect the balcony/facade symptom.
- Odin child visibility and WSDamageable selector experiments were negative and
  remain disabled diagnostics.
- `Sleep(1) -> Sleep(0)` is not part of Core 1; the validated frame-pacing
  component is `timeBeginPeriod(1)`.
- tuner.txt is not part of the architecture.

## H. Remaining true full-EXE audit pass

The repository evidence is sufficient to reconstruct every historical parameter
listed above and to migrate clean owners. It is **not** sufficient to discover
previously untouched constants that were never part of any historical patch.

A true second-pass sweep of the exact retail executable should cover:

1. all float/double constant xrefs from renderer, shadow, AO, particle, foliage,
   water, reflection, weather and post-process RTTI/string anchors;
2. allocation/count immediates and every matching cap/check;
3. D3D9 sampler/render-state creation and reset paths;
4. all named `*Quality`, `*Distance`, `*Range`, `*LOD`, `*Resolution`,
   `*Radius`, `*Bias`, `*Threshold`, `*Count`, `*Size` consumers;
5. owner isolation: reject constants shared by unrelated systems unless the
   consuming instruction itself can be redirected;
6. Core1 byte verification before adding any new INI key.

The exact retail bytes are required for that untouched-constant sweep.
