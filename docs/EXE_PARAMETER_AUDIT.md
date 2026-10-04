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
| Spot-shadows 4K factor | 0.5 -> 1.0 | exact WSSpotShadowZBuffer creation owner recovered at RVAs 0x00026054 / 0x0002609B / 0x00026151; projection 0.5 consumer remains untouched |
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


## 0.8A visual-safety finding

In-game screenshot validation of 0.8 showed visible shadow/cascade bands and a
dull/flat image. The regression is attributed to the advanced tonal/shadow/AO
group rather than to the newly added distance controls.

0.8A therefore restores native defaults for ToneMap, CSM selector/lambda/far,
shadow bias multipliers, PCF5x5 and AO resolution/filter scales while retaining
safe resolution/filter and distance improvements.

The advanced controls remain implemented and independently configurable; they
are simply no longer default-on until isolated one by one.


## I. Exact retail second-pass discoveries

The exact retail EXE has now been scanned directly rather than inferred from
historical patch manifests.

### Water render targets

`WaterReflection` is owned by two globals:

- VA `0x011C13D8` / RVA `0x00DC13D8`: width 512
- VA `0x011C13DC` / RVA `0x00DC13DC`: height 128

The same pair feeds creation and backend/surface consumers, so the dimensions
can be changed coherently.

`WaterNormals%d` and `WaterNormalsTemp%d` are both created at 128x128 from
four exact push-immediate sites:

- RVA `0x0053A599`
- RVA `0x0053A5BE`
- RVA `0x0053A629`
- RVA `0x0053A634`

### Rain render targets

`RainCubeRT` is created at 128 from exact push-immediate RVA
`0x00402785`.

`HardwareRainDepthTexture` and `HardwareRainDepthTextureVS` reuse the same
1024x1024 dimension owner as `WSShadowZBuffer`:
RVA `0x00DD61B4/0x00DD61B8`.

Therefore the project deliberately does NOT create a separate rain-depth
resolution key. One shared native owner maps to one INI control:
`Shadows.ShadowMapResolution`.

### Depth blur

The primary `WSDepthBlurFilter` path uses class-owned constants:

- RVA `0x00D3A3C0`: 200
- RVA `0x00D3A3BC`: 50

The path computes approximately
`clamp(max(sourceValue - 200, 0) / 50, 0, 1)`.
The exact semantic name of sourceValue remains under audit, so the INI names are
deliberately conservative:
`DepthBlurAutoStart` and `DepthBlurAutoRange`.

### Still under active second-pass analysis

- MotionBlurDownsampledBackBuffer half-resolution path
- Bloom / GodRays quarter-resolution render targets
- ScaledTexture half-resolution post-process target
- FSAA filter constants
- low-cloud / cloud-shadow statics
- water/reflection shader constants
- post-process intensity/tone constants whose semantic mapping is not yet proven


## J. Exact-retail post-process / WTF / VeryFarScene pass

### ScaledTexture

The generic post-process `ScaledTexture` is created at half backbuffer
resolution by two exact shifts:

- VA `0x007C9EAF` / RVA `0x003C9EAF`: `shr cx,1`
- VA `0x007C9EBE` / RVA `0x003C9EBE`: `shr cx,1`

`ExperimentalPostFX.ScaledTextureFullResolution=1` removes only those two
shifts. Default 0 retains retail.

### DepthBlur mask

`DepthBlurMask2x2` and `DepthBlurMask2x2Temp` share one local dimension
scale load:

- VA `0x007CFE4A` / RVA `0x003CFE4A`
- native source VA `0x00F7AC88`: double 0.5

The ASI redirects only this consumer to an owned double:
`ExperimentalPostFX.DepthBlurMaskResolutionScale=0.5`.

### Will To Fight grid

Retail WSWillToFightGrid is coherently 256x256 across CPU and GPU:

- CPU dimension load VA `0x0097690C` / RVA `0x0057690C`
- source VA `0x01027748`: float 256
- LowResWorldWTF dimensions at RVAs `0x0057603B`, `0x00576040`
- LowResWorldWTFVertex dimensions at RVAs `0x00576085`, `0x0057608A`

The INI exposes one coherent owner:
`WillToFight.GridResolution=256`.

Historical 1024 testing did not solve the distant red-material issue, so native
256 remains the default.

### VeryFarScene profile thresholds

Profile record +0x08 owns two real thresholds:

- profile 0 = 22
- profile 1 = 49

Only VeryFarScene loads are redirected:

22.0:
- RVA `0x004014EE`
- RVA `0x0040153A`
- RVA `0x00401A9B`

49.0:
- RVA `0x004014C2`
- RVA `0x0040151E`
- RVA `0x00401A7B`

The unrelated 49.0 consumer at VA `0x008C04F5` remains untouched.

These are exposed under `[ExperimentalDistances]` at native defaults. The
historical 22/49 -> 88/196 experiment did not move the balcony/facade symptom,
so they are documented as real engine parameters, not as a proven fix.

### HDR luminance chain

The post-process initializer creates an explicit fixed reduction pyramid:

- LuminanceTexture64x64 = 64x64
- LuminanceTexture16x16 = 16x16
- LuminanceTexture4x4 = 4x4
- LuminanceTexture1x1 = 1x1
- adaptive luminance resources remain 1x1

This is not exposed as a resolution option. Increasing only part of the chain
would make the reduction topology incoherent, while increasing the final 1x1
resource would change the semantic endpoint rather than merely quality.

### CloudShadowLowRes

The CPU side requests `CloudShadowLowRes` as a named resource and the VS path
loads `CloudShadowLowRes-PC.dds`. No independent render-target dimension
owner exists in the retail executable. Resolution changes belong to asset
replacement, not to an EXE/INI scalar, so no CloudShadowResolution key is
invented.


## K. DepthBlur color pyramid and SkyDome RT audit

### DepthBlurColor pyramid

The DepthBlur color-resource loop creates four levels and computes its divisor
from a local multiplication by double 0.75.

- consumer VA `0x007CFA3B`
- consumer RVA `0x003CFA3B`
- opcode `DD 05`
- native source VA `0x00F8A368`
- native source RVA `0x00B8A368`
- native value: double 0.75

The resulting retail divisors are approximately:

- level 0: 1.5 => ~2/3 screen
- level 1: 3 => ~1/3
- level 2: 6 => ~1/6
- level 3: 12 => ~1/12

The ASI redirects only this local consumer. The shared 0.75 constant is NOT
modified because it has unrelated consumers, including another DepthBlur
arithmetic path.

INI:
`ExperimentalPostFX.DepthBlurColorPyramidFactor=0.75`

A factor of 0.5 is the clean higher-quality A/B because it produces divisors
1 / 2 / 4 / 8 without altering level count.

### SkyDome render-target family

The SkyDome initializer contains three related downsample families.

Main blend family:

- RVA `0x00615C32`: `shr ax,2`
- RVA `0x00615C37`: `shr bp,2`
- retail: backbuffer /4

3x3 family:

- uses a divide-by-6 magic multiply followed by a final `sar edx,1`
- RVA `0x00615D92`: `D1 FA`
- RVA `0x00615DA8`: `D1 FA`
- retail result: /12

Distortion family:

- RVA `0x00615E2C`: `shr cx,3`
- RVA `0x00615E4E`: `shr cx,3`
- retail: /8

`Sky.ResolutionMultiplier=2` changes all three coherently:

- main /4 -> /2
- 3x3 /12 -> /6
- distortion /8 -> /4

No individual SkyDome subtarget key is exposed because that would make the
family internally inconsistent.


## L. DamageBlur, RainDensity and WSParticleRender audit

### DamageBlur render target

`BackBufferLDRPostFiltersDamageBlur` derives width and height from one local
double 0.5 consumer:

- VA `0x007D72D3`
- RVA `0x003D72D3`
- opcode `DD 05`
- expected source RVA `0x00B7AC88`
- retail scale: 0.5

The ASI redirects only that local consumer. The shared 0.5 constant remains
untouched because it is used by unrelated systems.

INI:
`ExperimentalPostFX.DamageBlurResolutionScale=0.5`

### Hidden RainDensity setting

Retail registers a hidden integer graphics setting named `RainDensity` with
default 100.

The renderer consumes it at:

- VA `0x00801EFC`
- RVA `0x00401EFC`
- exact CALL bytes `E8 1F AB FB FF`

The following native code divides the integer by 100 and clamps the normalized
value to 0.25..2.0, corresponding to 25..200 percent.

INI:
`Rain.DensityPercentOverride=0`

Zero means no override. For 25..200 the ASI replaces only the getter CALL with
`mov eax,imm32`; the native store, divide and clamp remain intact.

### WSParticleRender render-target hierarchy

The main ParticleBB0 / particle-light-volume / distortion family reuses one
half-resolution width/height pair:

- RVA `0x002E7DFA`: `66 D1 E9` = `shr cx,1`
- RVA `0x002E7DFE`: `66 D1 ED` = `shr bp,1`

The ParticleBB3 family uses /16:

- RVA `0x002E7FAF`: `66 C1 E9 04`
- RVA `0x002E7FB4`: `66 C1 ED 04`

INI:
`Particles.RenderTargetResolutionMultiplier=1`

Supported 2x quality keeps the relative hierarchy coherent:

- main family /2 -> full
- BB3 family /16 -> /8

No individual particle target is exposed separately because doing so would
break the renderer's shared dimension assumptions.


## Scaleform / high-resolution UI cache owners — exact retail confirmation

These owners were re-audited read-only against the exact retail executable and
the byte-perfect reconstructed V200 image. They are now sufficiently proven to
be future independent `[UI]` controls. They are **not enabled by ASI 0.12**.

### Mesh cache

The constructor is directly associated with the adjacent retail
`_Mesh_Cache` name at VA `0x01062064`.

Retail constructor instruction:

- instruction VA: `0x00BB4667`
- instruction RVA: `0x007B4667`
- instruction RAW: `0x007B3867`
- immediate VA/RVA/RAW: `0x00BB466A / 0x007B466A / 0x007B386A`
- retail: `mov dword ptr [esi+14h], 00800000h` = **8 MiB**
- V200: `01000000h` = **16 MiB**

The decoded V200 manifest records the changed high bytes at RAW
`0x007B386C`.

Future INI owner: `[UI] MeshCacheMiB`.

### Vector glyph cache capacity

The constructor is in the retail `_Font_Cache` family. Nearby retail data
contains the explicit warning:

`Warning: Increase vector glyph cache capacity - SetMaxVectorCacheSize().`

Owner:

- instruction VA: `0x00BE6AD7`
- instruction RVA: `0x007E6AD7`
- immediate VA/RVA/RAW: `0x00BE6ADD / 0x007E6ADD / 0x007E5CDD`
- field: `[esi+0x9C0]`
- retail: **512**
- V200: **1024**
- manifest changed byte: RAW `0x007E5CDE`, `02 -> 04`

Future INI owner: `[UI] VectorGlyphCache`.

### Font cache texture count

Two separate `_Font_Cache` constructors initialize the same configuration
field `[esi+0x1C]`. V200 raises both coherently.

Constructor A:

- instruction VA/RVA/RAW:
  `0x00BE7D3E / 0x007E7D3E / 0x007E6F3E`
- immediate VA/RVA/RAW:
  `0x00BE7D41 / 0x007E7D41 / 0x007E6F41`
- retail **1** -> V200 **2**

Constructor B:

- instruction VA/RVA/RAW:
  `0x00BE7E19 / 0x007E7E19 / 0x007E7019`
- immediate VA/RVA/RAW:
  `0x00BE7E1C / 0x007E7E1C / 0x007E701C`
- retail **1** -> V200 **2**

The two sites must be treated as one coherent owner. A future
`[UI] FontCacheTextures` setting must verify both retail instructions before
writing either one.

### Safety decision

These UI cache controls are structurally much cleaner than the old broad
graphics experiments because they are constructor-owned capacities with exact
retail/V200 values and nearby Scaleform cache identifiers.

They remain deferred until the 0.12 graphics A/B is validated so UI resource
changes cannot contaminate that visual test.


### WSSpotShadowZBuffer exact owner

The dedicated spot-shadow Z-buffer creation family is now recovered cleanly
from Core1/retail.

Three creation instructions multiply target dimensions by the same native
double 0.5:

- RVA 0x00026054
- RVA 0x0002609B
- RVA 0x00026151

All three are `DC 0D <absolute double>` and target the retail 0.5 owner at
RVA 0x00B7AC88.

A fourth 0.5 consumer at VA 0x004268B7 is projection/midpoint math and must not
be redirected as part of the render-target quality owner.

Therefore `Shadows.SpotShadowResolutionScale` is now a proven clean runtime
control. The 0.33 candidate tests the historical 0.5 -> 1.0 quality step.
