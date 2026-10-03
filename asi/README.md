# SaboteurEnhanced ASI

## Current test build: 0.14 — CorrectUV / texel compensation

Validated baseline retained:
- Core 1
- V310 WSModel automatic RenderSlice full-mask correction
- V311 explicit ModelInfo RenderSlice full-mask correction
- x86 dinput8 proxy loader

## Purpose

ASI 0.8 continues restoring proven graphics/render findings and adds clean native owners found during the EXE-parameter audit. It
intentionally removed from the cleaned Core executable.

The rule is now simple: one proven setting, one independent INI entry.
Nothing is silently hard-baked into Core 1.

Every migrated patch verifies the original Core1/retail value or byte sequence
before writing. A mismatch is skipped and logged.

## Migrated graphics families

### Graphics
- Environment maps: 128 -> 2048
- anisotropic filtering: 4x -> 16x
- MIP LOD bias: 0.0 -> -0.25
- ToneMap: 0.25 -> 0.15

### Shadows
- main shadow maps: 1024 -> 4096
- PCF 3x3 parameter family -> PCF 5x5
- internal shadow/CSM quality selector: 2 -> 5
- CSM lambda: 0.50 -> 0.60
- recovered private CSM far target: ~100 -> 150
- shadow depth-bias table scale: 1.00 -> 0.75
- shadow slope-bias table scale: 1.00 -> 0.90

The internal quality value 5 is deliberately described as a selector, not as a
literal count of cascades. Historical analysis separately identified a four
cascade CSM layout.

### Ambient occlusion
- full-resolution AO buffer/sample path
- AO blur scale: 2.0 -> 1.25
- AO erode scale: 2.0 -> 1.25

### Streaming / distances
- streaming coverage Low/Medium/High: 1500/300/250 -> 16000/3200/2500
- FarScene x3: 200 -> 320
- decal visibility distance: 120 -> 160, stored internally as 14400 -> 25600

## Existing fixes

V310 and V311 remain independent default-on entries under [Fixes].

The WSDynamicPart priority-radius A/B remains available as
WSDynamicPartPriorityRadius. Rejected Odin/WSDamageable experiments remain
disabled.

## New 0.8 audited native owners

### Distances / LOD
- RenderSlice/ShadowSlice High class-3 far: 100 -> 300
- RenderSlice/ShadowSlice High outer endpoint: 500 -> 1500
- ModelInfo default LODDIST: 1000 -> 1500
- VeryFarSceneTerrain: 5000 -> 10000

### Experimental resource priority
- SS_HighPalette threshold has its own INI entry.
- Native/default remains 80 because the historical progression through 1600 was stable but did not prove an isolated visual benefit.

Each instruction-operand redirection verifies the original opcode and the original Core1/retail target before replacing it with ASI-owned storage.

## Still being migrated

Some historical findings depended on V200/V257 injected code caves rather than
clean native owners. They are NOT blindly copied into Core1/ASI 0.7. These
include the old ObjectLOD/foliage/shadow-caster/particle-LOD cave family and
other legacy distance hooks. They require clean native/runtime owners before
activation.

The historical Spot Shadows 4K 0.5 -> 1.0 factor is also preserved in the
research history, but its exact owner is not yet proven from the retained
manifest, so 0.7 does not fake an INI switch for it.

No tuner.txt modification is used.


## 0.8A visual regression correction

A user screenshot of 0.8 showed two regressions:
- visible shadow/cascade banding/lines across the scene
- an overly dull/flat image

To isolate the owner, 0.8A keeps the safe resolution/filter/distance gains but
returns advanced tonal/shadow/AO tuning to native defaults:

- MipLODBias: -0.25 -> 0.0
- ToneMap: 0.15 -> 0.25
- ShadowPCF5x5: 1 -> 0
- CSMQuality: 5 -> 2
- CSMLambda: 0.60 -> 0.50
- CSMFarDistance: 150 -> 100
- DepthBiasScale: 0.75 -> 1.00
- SlopeBiasScale: 0.90 -> 1.00
- AO FullResolution: 1 -> 0
- AO Blur/Erode: 1.25 -> 2.0

ShadowMapResolution=4096, EnvironmentMapResolution=2048, AF16 and validated
distance controls remain enabled.

This is an A/B isolation baseline, not a deletion of the recovered settings.


## 0.10 retail-EXE discoveries

The exact retail executable has now been reintroduced into the audit
(14,834,176 bytes, SHA-256
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`).

New clean owners exposed at native defaults:

- WaterReflection RT: 512x128
- WaterNormals / WaterNormalsTemp: 128x128
- RainCubeRT: 128
- WSDepthBlurFilter automatic transition: start 200 / range 50

Important shared-owner correction:

- WSShadowZBuffer and HardwareRainDepthTexture use the same native 1024x1024
  dimension globals. They remain one `ShadowMapResolution` control; a separate
  rain-depth key would be incorrect and could fight the shadow setting.

These new entries are native by default and therefore do not alter visuals
until explicitly changed.


## 0.10 second retail pass

Additional exact owners are now exposed at native defaults:

- `ScaledTextureFullResolution=0`: retail ScaledTexture is backbuffer/2.
- `DepthBlurMaskResolutionScale=0.5`: local owner for DepthBlurMask2x2 and its temp.
- `WillToFight.GridResolution=256`: CPU influence grid plus both WTF low-res render targets are kept coherent.
- `VeryFarSceneProfile0Threshold=22` and `VeryFarSceneProfile1Threshold=49`: only the six VeryFarScene consumers are redirected; an unrelated 49.0 consumer remains untouched.

All four are native-default controls. They do not change the visual baseline
unless explicitly edited.

The retail HDR luminance chain was also reconstructed as
64 -> 16 -> 4 -> 1 followed by adaptive 1x1 textures. It is intentionally not
presented as a generic resolution knob because changing one level alone would
make the reduction pyramid incoherent.

CloudShadowLowRes is loaded from a named DDS resource rather than created from
an EXE-owned dimension. No fake CloudShadowResolution control is added.


## 0.10 third retail pass

Two additional render-target owners are now independently configurable at
their exact retail defaults:

- `ExperimentalPostFX.DepthBlurColorPyramidFactor=0.75`
  controls only the local divisor used to build the four DepthBlurColor levels.
  Retail produces approximate screen fractions 2/3, 1/3, 1/6 and 1/12.
  A factor 0.5 would instead produce 1, 1/2, 1/4 and 1/8.
- `Sky.ResolutionMultiplier=1`
  controls the complete SkyDome RT family coherently. A value of 2 changes
  main blend /4 -> /2, 3x3 /12 -> /6 and distortion /8 -> /4.

Both remain retail-native by default.


## 0.10 fourth retail pass

Additional exact-retail owners are now exposed at native defaults:

- `ExperimentalPostFX.DamageBlurResolutionScale=0.5`
  controls only the BackBufferLDRPostFiltersDamageBlur render target. Retail is
  half-resolution; 1.0 is the clean full-resolution A/B.
- `Rain.DensityPercentOverride=0`
  leaves the hidden retail RainDensity setting untouched. A nonzero override
  uses the engine's own effective 25..200 percent range and preserves the
  native normalization/clamp logic.
- `Particles.RenderTargetResolutionMultiplier=1`
  keeps the WSParticleRender hierarchy native. Value 2 changes the shared main
  particle post-FX family from /2 to full resolution and ParticleBB3 from /16
  to /8, preserving the hierarchy coherently.

These controls are native/default-neutral until explicitly changed.


## 0.11 clean visual baseline

0.11 is the first cleanup candidate produced from the exact-retail EXE
re-audit. It is intentionally conservative and targets the two regressions
seen in the 0.8 screenshot: visible shadow/cascade lines and an overly dull
image.

### Critical corrections

- Removed the old `CSMQuality` runtime path completely. The audited target is
  an embedded `WSAmbientOcclusionFilter / PsDepthConv` shader instruction,
  not a CSM quality integer.
- ToneMap fallback is now retail `0.25`. The old `0.15` value directly
  reduced the recovered shader RGB contribution and is not treated as a
  quality enhancement.
- Full-resolution AO has been rebuilt as one coherent verified feature:
  - removes the two CPU half-resolution shifts used to create
    `AmbientOcclusionBB`;
  - changes the matched `PsAmbientOcclusion` scale 2.0 -> 1.0;
  - applies all three linked `PsDepthConv` instruction/operand changes.
  All retail signatures are verified before the first write.
- The two old addresses accidentally taken from `PsBloomFinal` are no longer
  part of the AO feature.
- Shadow-map resolution now owns its 24 embedded PCF texel offsets. The offsets
  are derived as `nativeOffset * 1024 / ShadowMapResolution`, so 2048, 4096
  and 8192 preserve the retail sampling radius.
- `ShadowPCF5x5` is now the real shader selector. It redirects the two verified
  retail PCF3x3 consumers to the recovered PCF5x5 families instead of modifying
  texel-offset constants.

### 0.11 shipped visual baseline

Kept enabled:
- EnvironmentMapResolution 2048
- AnisotropicFiltering 16
- ShadowMapResolution 4096, now with coherent texel offsets
- validated non-shadow distance/LOD improvements
- V310/V311 model render-mask fixes

Returned to retail for isolation:
- MipLODBias 0.0
- ToneMap 0.25
- PCF selector 3x3
- CSM lambda 0.50
- CSM far ~100
- shadow depth/slope bias scales 1.0 / 1.0
- SpotShadowResolutionScale 0.5
- AO half-resolution
- AO blur/erode 2.0 / 2.0
- ShadowSlice/CSM High bounds 100 / 500

The old `CSMQuality` INI key is removed.

This is a **test candidate**, not yet a validated release. The first test should
compare directly against the 0.8 screenshot area and check:

1. whether the visible horizontal/cascade lines are gone;
2. whether the image has recovered normal brightness/contrast;
3. whether 4096 shadows remain clean without oversized sampling;
4. whether AO produces any halo or edge artifact.


## 0.12 validated quality baseline

Status: **VALIDATED — current canonical quality baseline, built strictly from the validated 0.11 architecture**.

No new hook or binary owner is introduced in 0.12. The corrected 0.11 code is
unchanged. This candidate only enables two independently configurable,
retail-audited quality paths that were deliberately left OFF during the 0.11
visual-baseline validation:

- `Shadows.ShadowPCF5x5=1`
  - selects the verified SampleShadowMapPCF5x5 shader families;
  - does not modify shadow-map texel offsets;
  - ShadowMapResolution remains 4096 and continues to derive its own 24
    sampling offsets coherently.
- `AmbientOcclusion.FullResolution=1`
  - removes both CPU half-resolution shifts for AmbientOcclusionBB;
  - applies the matched PsAmbientOcclusion 2.0 -> 1.0 scale;
  - applies the complete linked PsDepthConv rewrite;
  - does not touch PsBloomFinal.

All other 0.11 visual-isolation values remain unchanged.

A/B isolation without rebuilding:
- to test PCF only: set `FullResolution=0`;
- to test full-resolution AO only: set `ShadowPCF5x5=0`;
- to return to the validated 0.11 visual profile: set both to 0.

Validation result (2026-10-01): **validated by the user**.

Canonical 0.12 profile:
- true PCF 5x5 enabled;
- coherent full-resolution AO enabled;
- ShadowMapResolution remains 4096 with derived sampling offsets;
- all other 0.11 cleanup/isolation decisions remain unchanged.

0.11 is retained as the immediate rollback/reference profile by setting both
`ShadowPCF5x5=0` and `FullResolution=0`.


## 0.13 full-resolution PostFX pass

Status: **TEST CANDIDATE — built strictly from canonical 0.12**.

0.13 does not add or change any C++ hook. It uses only render-target owners
already audited from the exact retail executable and enables one coherent
screen-space quality pass:

- `ExperimentalPostFX.MotionBlurFullResolution=1`
  - MotionBlurDownsampledBackBuffer: /2 -> full resolution.
- `ExperimentalPostFX.BloomResolutionMultiplier=2`
  - Bloom/GodRays pyramid: /4,/8,/16,/32 -> /2,/4,/8,/16.
- `ExperimentalPostFX.ScaledTextureFullResolution=1`
  - generic ScaledTexture source: /2 -> full resolution.
- `ExperimentalPostFX.DepthBlurMaskResolutionScale=1.0`
  - DepthBlurMask2x2/Temp: 0.5 -> 1.0.
- `ExperimentalPostFX.DepthBlurColorPyramidFactor=0.5`
  - DepthBlurColor divisors: 1.5/3/6/12 -> 1/2/4/8.
- `ExperimentalPostFX.DamageBlurResolutionScale=1.0`
  - damage-blur target: 0.5 -> 1.0.
- `Lighting.LightVolumeResolutionMultiplier=2`
  - LightVolumeRT: /2 -> full resolution.
- `Particles.RenderTargetResolutionMultiplier=2`
  - main particle post-FX family: /2 -> full resolution;
  - ParticleBB3 family: /16 -> /8.

Everything validated in 0.12 remains unchanged:
- PCF 5x5 stays enabled;
- coherent full-resolution AO stays enabled;
- ShadowMapResolution remains 4096 with derived texel offsets;
- tone-map, shadow bias, CSM distances and other 0.11 cleanup decisions remain frozen.

The water, sky and rain render-target families are deliberately left native in
0.13 so this test isolates screen-space/post-FX resolution quality before the
next world-render-target pass.

Rollback to exact 0.12 profile without rebuilding:
- MotionBlurFullResolution=0
- BloomResolutionMultiplier=1
- ScaledTextureFullResolution=0
- DepthBlurMaskResolutionScale=0.5
- DepthBlurColorPyramidFactor=0.75
- DamageBlurResolutionScale=0.5
- LightVolumeResolutionMultiplier=1
- Particles.RenderTargetResolutionMultiplier=1

Validation target:
1. image stability and brightness identical to 0.12;
2. cleaner motion/damage blur edges;
3. cleaner bloom/god-rays and depth-blur masks;
4. denser particle/light-volume edges without halos or broken compositing;
5. note any meaningful GPU cost or scene-specific artifact.


## 0.14 CorrectUV / texel-compensation candidate

Status: **TEST CANDIDATE — 0.12 remains canonical**.

User requirement for this branch: do not remove any of the 0.13 quality
increases. 0.14 therefore restores the complete original 0.13 profile:

- MotionBlurFullResolution=1
- BloomResolutionMultiplier=2
- ScaledTextureFullResolution=1
- DepthBlurMaskResolutionScale=1.0
- DepthBlurColorPyramidFactor=0.5
- DamageBlurResolutionScale=1.0
- LightVolumeResolutionMultiplier=2
- Particles.RenderTargetResolutionMultiplier=2

The 0.13A and 0.13B tests showed that returning Bloom/GodRays and
ScaledTexture individually to retail resolution did not remove the bright
fragmented visual artifact. They are therefore no longer removed in this
candidate.

0.14 follows the same principle that fixed historical full-resolution AO:
increasing a render-target resolution must be paired with the shader-side
sampling geometry that still assumes the retail target scale.

Two new independently configurable shader controls are added:

- `DepthBlurMaskTapOffsetScale`
  - native = 1.0;
  - 0.14 test = 0.5;
  - scales only the literal spatial tap distances in the two verified
    DepthBlur mask shaders;
  - normalization constants such as 1/9 remain untouched;
  - because the mask target changes 0.5x -> 1.0x, 0.5 preserves its retail
    screen-space sampling radius.

- `DepthBlurColorTexelOffsetScale`
  - native = 1.0;
  - 0.14 test = 0.6666667;
  - scales only the verified -1/+1 texel offsets in the two DepthBlur color
    shaders;
  - the unrelated 0.2 shader threshold remains untouched;
  - the color pyramid changes from divisors 1.5/3/6/12 to 1/2/4/8, making
    every level 1.5x larger per dimension, so 2/3 preserves the retail
    screen-space texel radius.

Every embedded float is verified against its exact retail value before the
group is written. A mismatch fails closed.

This candidate deliberately does not invent compensation controls for
MotionBlur, DamageBlur, LightVolume or particle buffers until their shader-side
owner is proven with the same level of confidence.


## 0.15 coherent LightVolume full-resolution candidate

Status: **TEST CANDIDATE — 0.12 remains canonical**.

0.14 result: the bright fragmented/polygonal artifact remained unchanged, so
the DepthBlur shader compensation is not the owner of that defect.

The exact retail executable was re-audited. A second half-resolution owner was
found inside the WSLightVolumeManager vtable refresh path:

- method entry: VA `0x007FE560`
- helper: VA `0x007FE060`
- local scale load: VA `0x007FE0A5`, RVA `0x003FE0A5`
- retail operand: VA `0x00F7AC88` = double `0.5`
- the same loaded factor feeds both backbuffer width and height
- all reciprocal/coordinate values are then derived from those scaled dimensions

This owner is independent from the two `shr 1` instructions that create
`LightVolumeRT` at half resolution. Therefore 0.13 made the target full
resolution while this manager-side coordinate profile still behaved as
half-resolution.

New INI control:

`Lighting.LightVolumeCoordinateResolutionScale`

- retail/native: `0.5`
- 0.15 test: `1.0`

With `LightVolumeResolutionMultiplier=2`, value 1.0 makes both the render
target and the manager's coordinate/reciprocal profile full-resolution.

Only the local FLD operand is redirected to ASI-owned storage. The shared 0.5
constant is not modified.

All 0.13 quality increases and the coherent 0.14 DepthBlur compensation remain
enabled. Nothing is removed for this test.


## 0.16 particle RestoreDepthBuffer CorrectUV candidate

Status: **TEST CANDIDATE — 0.12 remains canonical**.

0.15 result: the LightVolume manager coordinate profile was made coherent with
the full-resolution LightVolumeRT, but the user still sees the same bright
white fragmented/polygonal artifact around the window. LightVolume therefore
is not sufficient to explain the defect.

The next exact shader-side dependency was recovered from the embedded
`WildStar/Particles/ApplyPS.hlsl` `RestoreDepthBuffer` variant.

Retail particle topology:
- ParticleBB0 / AfterParticleLightVolume family: backbuffer /2;
- RestoreDepthBuffer reconstructs that half-resolution source through a packed
  alternating-column mapping;
- the mapping contains four fixed embedded constants:
  - RVA `0x00D40DDC`: `0.0125` = 1/80;
  - RVA `0x00D40DE8`: `2.0`;
  - RVA `0x00D40DF8`: `-80.0`;
  - RVA `0x00D40DFC`: `+80.0`.

0.13 raises the ParticleBB0 family from /2 to full resolution, but until 0.16
the RestoreDepthBuffer shader still performed the old half-column
reconstruction.

New INI control:

`Particles.FullResolutionDepthRestore`

- native/retail = 0;
- 0.16 candidate = 1;
- valid with `RenderTargetResolutionMultiplier=2`;
- all four retail constants are verified before the particle RT resize is
  allowed to proceed.

Full-resolution mapping:
- 1/80 -> 0;
- 2.0 -> 1.0;
- -80 -> 0;
- +80 -> 0.

This collapses the packed half-column reconstruction to the normal
full-resolution screen-coordinate mapping while preserving the higher
resolution particle targets.

Nothing from 0.13, 0.14 or 0.15 is removed. The candidate keeps Bloom,
ScaledTexture, MotionBlur, DepthBlur, DamageBlur, LightVolume and particles at
their enhanced settings and adds only this missing particle-side coordinate
correction.


## 0.16 result -> 0.17 bloom-energy compensation candidate

0.16 result: **visual geometry artifact fixed**.

The user confirms that the bright fragmented/polygonal defect disappeared
after enabling the coherent full-resolution particle RestoreDepthBuffer mapping.

Therefore the following 0.16 correction is retained:
- `Particles.RenderTargetResolutionMultiplier=2`
- `Particles.FullResolutionDepthRestore=1`

Remaining issue:
- image is now stable, but bright lamps/mirrors/reflections show excessive
  highlight energy;
- ToneMap remains the validated retail-safe 0.25 and is deliberately not used
  to solve a local bloom problem.

0.17 exposes one exact PsBloomFinal scalar:

`ExperimentalPostFX.BloomFinalContribution`

- retail = 4.0;
- 0.17 candidate = 2.0;
- exact RVA = `0x00D69360`;
- historical V200 also changed a sampler route, but 0.17 does **not** repeat
  that artistic sampler change;
- BloomResolutionMultiplier remains 2, so the higher-resolution bloom pyramid
  is preserved;
- ToneMap/exposure/adaptive luminance remain unchanged.

The goal is to halve bloom energy while preserving all 0.13-0.16 quality and
CorrectUV improvements.


## 0.21 validated canonical quality baseline

Status: **VALIDATED — current canonical ASI baseline**.

User validation confirms that the 0.21 PsBloom prefilter normalization resolves
the excessive highlight brightness while preserving the image quality and all
previous CorrectUV/full-resolution improvements.

Canonical 0.21 profile retains:
- PCF 5x5
- full-resolution AO
- ShadowMapResolution 4096 with coherent offsets
- full-resolution MotionBlur
- 2x Bloom/GodRays pyramid
- full-resolution ScaledTexture
- coherent high-resolution DepthBlur
- full-resolution DamageBlur
- full-resolution LightVolume with corrected coordinate scale
- 2x particle RT hierarchy
- particle RestoreDepthBuffer full-resolution coordinate fix
- retail ToneMap/BloomFinal/final samplers
- PsBloom prefilter gain normalized to 1.0

Validated brightness path:
- `Graphics.ToneMap=0.25`
- `ExperimentalPostFX.BloomFinalContribution=4.0`
- `ExperimentalPostFX.BloomFinalBackBufferSampler=0`
- `ExperimentalPostFX.BloomFinalDownsampledBackBufferSampler=0`
- `ExperimentalPostFX.BloomPrefilterGain=1.0`

0.21 supersedes 0.12 as the canonical ASI baseline.


## 0.22 world render-target quality candidate

Status: **TEST CANDIDATE — built strictly from canonical 0.21**.

0.22 leaves the validated 0.21 post-processing, brightness, AO, shadows and
CorrectUV chain untouched. It only raises world-facing render targets whose
dimension owners are already audited and self-consistent:

- WaterReflection: 512x128 -> 2048x512
  - exact 4:1 aspect ratio preserved;
  - shared width/height owners feed both creation and backend/surface paths.
- WaterNormals / WaterNormalsTemp: 128x128 -> 512x512
  - all four fixed dimension immediates changed coherently.
- SkyDome RT family: multiplier 1 -> 2
  - main blend /4 -> /2;
  - 3x3 family /12 -> /6;
  - distortion /8 -> /4.
- RainCubeRT: 128 -> 512
  - cubemap face resolution only;
  - HardwareRainDepthTexture remains owned by ShadowMapResolution and is not
    modified here.
- Rain density remains native.

No change to:
- ToneMap 0.25;
- Bloom prefilter normalization 1.0;
- BloomFinal/samplers;
- PCF5x5 / AO full-res / ShadowMap 4096;
- LightVolume/particle CorrectUV;
- any distance/LOD or engine-limit setting.

Validation target:
1. water reflections and normal detail;
2. sky gradients/distortion clarity;
3. rain cubemap sharpness;
4. no seams, UV shifts, brightness changes or crashes.


## 0.22A validated canonical world-render baseline

Status: **VALIDATED — current canonical ASI baseline**.

0.22A is functionally identical to 0.22 and differs only in packaging:
- packaged documentation restored to `README.txt`.

Validated world-render upgrades:
- WaterReflection 512x128 -> 2048x512;
- WaterNormals / WaterNormalsTemp 128x128 -> 512x512;
- SkyDome family multiplier 1 -> 2;
- RainCubeRT 128 -> 512;
- rain density remains native.

All 0.21 canonical post-processing, brightness, AO, shadows, CorrectUV and
particle fixes remain unchanged.

0.22A supersedes 0.21 as the canonical ASI baseline.


## 0.23 extreme world render-target candidate

Status: **TEST CANDIDATE — built strictly from canonical 0.22A**.

0.23 pushes only the three already-validated world render-target families
further, leaving SkyDome at its validated coherent x2 profile:

- WaterReflection: 2048x512 -> 4096x1024
  - native 4:1 aspect ratio preserved;
  - same shared width/height owners as validated 0.22A.
- WaterNormals / WaterNormalsTemp: 512x512 -> 1024x1024
  - same matched four-immediate family as validated 0.22A.
- RainCubeRT: 512 -> 1024
  - cubemap face resolution only.
- SkyDome remains x2:
  - main blend /2;
  - 3x3 family /6;
  - distortion /4.
- Rain density remains native.

Everything else is frozen from 0.22A, including:
- ShadowMap 4096 + PCF5x5;
- full-resolution AO;
- all validated 0.13-0.16 PostFX/CorrectUV fixes;
- particle RestoreDepthBuffer fix;
- LightVolume full-resolution coherent path;
- ToneMap 0.25;
- BloomPrefilterGain 1.0;
- retail BloomFinal/samplers;
- README packaged as `README.txt`.

Validation target:
1. visible gain in water reflection/normal sharpness and rain cubemap;
2. no seams, UV shifts, shimmer, brightness change or crash;
3. no meaningful regression in GPU cost severe enough to justify backing off.


## 0.23 validated canonical extreme world-render baseline

Status: **VALIDATED — current canonical ASI baseline**.

User validation confirms that the 0.23 world render-target increases are stable.

Validated 0.23 world-render values:
- WaterReflection 4096x1024;
- WaterNormals / WaterNormalsTemp 1024x1024;
- SkyDome family x2;
- RainCubeRT 1024;
- rain density native.

All 0.22A/0.21 validated post-processing, brightness, AO, shadows, CorrectUV,
particle and LightVolume fixes remain unchanged.

0.23 supersedes 0.22A as the canonical ASI baseline.


## 0.24 maximum world render-target candidate

Status: **TEST CANDIDATE — built strictly from canonical 0.23**.

0.24 takes the already-validated world render-target families to the current
audited ASI limits:

- WaterReflection: 4096x1024 -> 8192x2048
  - 4:1 aspect ratio preserved;
  - still uses the shared coherent width/height owners.
- WaterNormals / WaterNormalsTemp: 1024x1024 -> 2048x2048
  - reaches the current ASI limit for this matched family.
- RainCubeRT: 1024 -> 2048
  - reaches the current ASI limit for this isolated cubemap owner.
- SkyDome remains x2
  - x2 is the highest coherent implementation currently audited.
- Rain density remains native.

Everything else remains frozen from canonical 0.23, including the validated
0.21 brightness normalization and all PostFX/CorrectUV/shadow/AO fixes.

This candidate is intended as the endpoint for simple world-RT scaling. If it is
validated, future work should move to a different engine/render owner instead
of increasing these dimensions further.


## 0.24 abandoned

The 0.24 maximum-RT experiment is abandoned by user direction.

Reason:
- no more blind render-target escalation;
- 0.23 already established a stable high-quality world-RT baseline;
- further work must target a proven engine/render owner with a concrete
  gameplay or visual problem.

The repository is restored to the 0.23 canonical values:
- WaterReflection 4096x1024
- WaterNormals 1024
- SkyDome x2
- RainCubeRT 1024

Future candidates must not continue simple RT-size inflation.


## 0.25 streaming-pressure telemetry diagnostic

Status: **DIAGNOSTIC ONLY — canonical gameplay/render baseline remains 0.23**.

0.25 does not increase a streaming capacity and does not change scheduling,
coverage, merged-read size or coalescing thresholds.

The generic pool manager was re-audited before building this diagnostic. The
field semantics are now proven from the allocator/free paths:

- descriptor +0x2C = object size;
- descriptor +0x34 = current allocated capacity;
- descriptor +0x38 = active/in-use count;
- descriptor +0x3C = backing allocation;
- descriptor +0x44 = free-list head.

The allocator increments +0x38 after a successful allocation; the release path
decrements it. The invariant checker explicitly verifies:

`free-list node count == capacity(+0x34) - active(+0x38)`.

Streaming pool descriptors:
- WSReadJob: VA 0x0132B9D8 / RVA 0x00F2B9D8, object size 0x2C;
- WSUncompressJob: VA 0x0132BA40 / RVA 0x00F2BA40, object size 0x28.

0.25 samples both active counts every 50 ms and reports every 5 seconds.

It also instruments the already-validated Core 1 streaming FIFO full-queue
correction. Core 1 keeps the native 64-slot ring and, when full, drops the
oldest entry before enqueueing the new request. 0.25 counts those exact
full-queue events while reproducing the existing Core 1 code path unchanged.

New diagnostic INI controls:
- `Diagnostics.StreamingTelemetry=1`
- `Diagnostics.StreamingTelemetryIntervalMs=50`
- `Diagnostics.StreamingTelemetryReportMs=5000`

The resulting `SaboteurEnhanced.log` reports:
- current/peak WSReadJob occupancy;
- current/peak WSUncompressJob occupancy;
- capacity-hit transitions;
- native FIFO full-queue event count.

Decision rule:
- if either 1200-entry pool approaches/exhausts capacity, the historical
  validated 1200 -> 2400 V272 change earns a focused functional candidate;
- if pool pressure stays low but FIFO-full events accumulate, investigate
  producer/consumer scheduling and async throughput instead;
- if neither is pressured, do not increase either capacity.


## 0.25 runtime result

0.25 completed its diagnostic purpose.

Observed in the user runtime log:
- WSReadJob peak: 94 / 1200 (~7.8%);
- WSUncompressJob peak: 492 / 1200 (41.0%);
- capacity-hit transitions: 0 / 0;
- exact Core1 64-slot FIFO full events: 0.

Decision:
- do not restore historical V272 1200 -> 2400;
- do not enlarge the 64-slot FIFO;
- move the investigation to the retail I/O scheduler itself.

## 0.26 retail single-submit scheduler telemetry diagnostic

Status: **DIAGNOSTIC ONLY — canonical gameplay/render baseline remains 0.23**.

A second-pass audit reconstructed the retail scheduler precisely rather than
treating historical Async32 as a scalar.

Retail/Core1 path:
- scheduler tick RVA 0x009B6750;
- one current I/O pointer at scheduler +0x218;
- queue object at scheduler +0x10C;
- submit routine RVA 0x009B5540;
- completion callback RVA 0x009B56C0.

The retail tick submits only when +0x218 is null. It removes one queued request,
stores it as the single current request, and calls the submit routine. On
completion, the callback clears +0x218 and immediately submits at most one next
queued request.

Historical V200 Async32 is now confirmed as a real multi-submit scheduler:
- the V200 scheduler hook enters a loop while its in-flight counter is below 32;
- it repeatedly peeks/pops queued requests;
- it increments an in-flight counter before each submit;
- its completion hook decrements the counter;
- this is why Async32 must never be represented as a fake 16 -> 32 constant.

0.26 does **not** enable that historical scheduler. It instruments the untouched
retail policy only.

Exact diagnostic hooks:
- retail tick submit CALL: RVA 0x009B679E;
- completion-chain submit CALL: RVA 0x009B572A;
- completion callback entry: RVA 0x009B56C0.

All three sites are verified against exact retail/Core1 bytes before any write.

0.26 measures:
- submit and completion counts;
- submit->completion service latency, average/max and >=2/5/10/20/50 ms buckets;
- queue-depth peak;
- submits/completions that occur with additional backlog;
- 5 ms scheduler-state sampling:
  - current-I/O busy percentage;
  - queued percentage;
  - busy+queued percentage;
  - idle-while-queued samples and longest streak;
- actual per-read byte size reconstructed from the same job fields consumed by
  the native submit routine;
- read-size buckets <=64/256/512/1024/>1024 KiB;
- overlap/unmatched/job-pointer consistency checks.

0.25 pool/FIFO telemetry remains enabled alongside 0.26 so scheduler data can
be correlated with the already-proven pool headroom.

New INI controls:
- `Diagnostics.StreamingSchedulerTelemetry=1`
- `Diagnostics.StreamingSchedulerSampleMs=5`

No scheduling, pool, FIFO, coverage, coalescing or read-size limit is changed.


## 0.26 runtime result

The user runtime log proves the 0.26 hooks installed successfully, but the
historical V200 Async32 scheduler path remained completely dormant during the
observed workload:

- scheduler submit count: 0;
- completion count: 0;
- queue peak: 0;
- busy percentage: 0%;
- all latency/read-size buckets: 0.

At the same time, the active streaming pools were busy:
- WSReadJob peak: 121 / 1200;
- WSUncompressJob peak: 542 / 1200;
- no capacity hits;
- no Core1 FIFO-full events.

Conclusion:
- 0.26 does **not** justify restoring Async32;
- the historical Async32 path is not the active path for this observed streaming
  workload;
- follow the actual WSReadJob/WSUncompressJob producer/consumer call sites next.

## 0.27 streaming-pool provenance diagnostic

Status: **DIAGNOSTIC ONLY — canonical gameplay/render baseline remains 0.23**.

0.27 disables the dormant 0.26 scheduler probe by default and keeps 0.25 pool
pressure telemetry.

It hooks the exact generic pool manager functions:
- allocate RVA 0x009C1940;
- release RVA 0x009C1A20.

The first 9 bytes of both routines are verified before either hook is installed.

Hooks immediately filter on:
- WSReadJob descriptor RVA 0x00F2B9D8;
- WSUncompressJob descriptor RVA 0x00F2BA40.

No other pool is logged.

0.27 records:
- total allocation/free counts for both streaming pools;
- distinct caller return RVAs;
- approximate direct CALL RVA (= return RVA - 5);
- per-caller invocation counts and first/last observation times.

No pool capacity, queue policy, scheduler behavior, coalescing threshold or
distance setting is changed.

Decision gate:
- use the real caller RVAs to identify the active read/decompress pipeline;
- only instrument that proven path in the next diagnostic;
- keep historical Async32 disabled unless the active path itself later proves
  serialized and backlogged.


## 0.27 rejected after runtime crash

0.27 pool-provenance instrumentation crashed at runtime.

Decision:
- reject 0.27 completely;
- stop allocator/scheduler hook telemetry as a development direction;
- restore the repository to the clean 0.23 canonical ASI baseline;
- keep 0.25/0.26 only as historical diagnostics;
- future work must prefer static audit and narrowly justified functional fixes
  over invasive runtime instrumentation.

0.23 remains the canonical gameplay/render baseline.


## 0.28 DepthBlur RAW-to-RVA CorrectUV fix

Status: **FUNCTIONAL TEST CANDIDATE — canonical baseline remains 0.23**.

A static audit of the two persistent DepthBlur `[SKIP]` messages found a
concrete address-mapping error introduced in ASI 0.14.

The 0.14 shader audit recovered literal constants from the executable file and
documented their file offsets as if they were already runtime RVAs.

This embedded shader PE section has a verified mapping:

`runtime RVA = raw file offset + 0x1600`

The same mapping is already used correctly by the independently verified
PsBloom/PsBloomFinal owners, for example:
- RAW 0x00D67E3C -> RVA 0x00D6943C;
- RAW 0x00D68568 -> RVA 0x00D69B68.

Therefore the DepthBlur owners are corrected as follows.

Mask spatial taps:
- RAW 0x00D66AE0 -> RVA 0x00D680E0;
- RAW 0x00D66AF8/FC/B00/B04 -> RVA 0x00D680F8/FC/100/104;
- RAW 0x00D66DB8/DBC -> RVA 0x00D683B8/BC;
- RAW 0x00D66DD0/D4/D8/DC -> RVA 0x00D683D0/D4/D8/DC.

Color texel offsets:
- RAW 0x00D664A0/A4 -> RVA 0x00D67AA0/A4;
- RAW 0x00D666B0/B4 -> RVA 0x00D67CB0/B4.

The values themselves are unchanged from the original 0.14 design:
- `DepthBlurMaskTapOffsetScale=0.5`;
- `DepthBlurColorTexelOffsetScale=0.6666667`.

No render-target dimension, bloom, ToneMap, AO, shadow, streaming, LOD or
engine-capacity setting changes in 0.28.

Expected runtime result:
- the two previous DepthBlur `[SKIP]` lines disappear;
- both groups report `[OK]`;
- full-resolution DepthBlur keeps the intended retail screen-space sampling
  radius rather than silently running without its compensation.

0.23 remains canonical until explicit visual validation.


## 0.28 validated canonical baseline

Status: **VALIDATED — current canonical ASI baseline**.

User validation confirms the DepthBlur RAW-to-RVA correction is good in game.

Canonical 0.28 preserves the full validated 0.23 profile and permanently fixes
the two DepthBlur shader compensation groups by applying the correct embedded
shader section mapping:

- mask family RAW offsets -> runtime RVA +0x1600;
- color family RAW offsets -> runtime RVA +0x1600.

Frozen validated values:
- DepthBlurMaskTapOffsetScale = 0.5;
- DepthBlurColorTexelOffsetScale = 0.6666667.

0.28 supersedes 0.23 as the canonical ASI baseline.


## 0.29 Scaleform high-resolution UI cache candidate

Status: **FUNCTIONAL TEST CANDIDATE — canonical baseline remains 0.28**.

0.29 adds one coherent UI/Scaleform resource family recovered exactly from the
retail executable and the historical V200 image. It does not alter rendering,
streaming, LOD, shadows, AO, bloom, particles or world render targets.

Audited owners:

1. Scaleform `_Mesh_Cache`
   - constructor RVA 0x007B4667
   - retail 8 MiB
   - historical V200 16 MiB
   - 0.29 = 16 MiB

2. Vector glyph cache
   - constructor RVA 0x007E6AD7
   - field [esi+0x9C0]
   - retail 512
   - historical V200 1024
   - 0.29 = 1024

3. Font cache texture count
   - constructor RVAs 0x007E7D3E and 0x007E7E19
   - shared field [esi+0x1C]
   - retail 1
   - historical V200 2
   - 0.29 = 2

The font-cache pair is atomic: both retail signatures are verified before
either is written.

New INI:
```
[UI]
MeshCacheMiB=16
VectorGlyphCache=1024
FontCacheTextures=2
```

0.29 deliberately supports only the exact audited retail/V200 pairs. There is
no arbitrary x2/x4 extrapolation beyond those historical values.

Validation target:
- normal menus/HUD/text rendering;
- no missing/corrupted glyphs;
- no UI crash;
- no visual regression at current resolution.

0.28 remains canonical until explicit validation.


## 0.29 validated canonical baseline

Status: **VALIDATED — current canonical ASI baseline**.

User validation confirms the audited Scaleform UI-cache profile is stable.

Canonical UI values:
- MeshCacheMiB = 16;
- VectorGlyphCache = 1024;
- FontCacheTextures = 2.

All validated 0.28 rendering, DepthBlur, AO, shadow, distance, streaming and
post-processing settings remain unchanged.

0.29 supersedes 0.28 as the canonical ASI baseline.


## 0.30 WSModel small-object hard-cull candidate

Status: **FUNCTIONAL TEST CANDIDATE — canonical baseline remains 0.29**.

This candidate targets the strongest remaining renderer-specific explanation for
very-near small-prop pop-in.

Retail WSModel behavior:
- constructor initializes WSModel+0xA8 = 10000;
- for unlisted models with size metric < 1.5, setup rewrites A8 to:
  `20 + 60 * metric`;
- the visibility path later hard-culls the model when camera-forward depth
  exceeds A8.

Exact branch:
- VA 0x0063954E
- runtime RVA 0x0023954E
- retail bytes: `7A 1A` (JP)
- 0.30: `EB 1A` (JMP)

The branch change skips only the A8 rewrite. It continues into the existing
AC/shadow-distance calculation, so the shadow cutoff path is preserved.

New INI:
`Fixes.WSModelSmallObjectHardCullBypass=1`

No slice table, LOD scalar, streaming value, render target, pool or scheduler
setting changes in 0.30.

0.29 remains canonical until explicit validation.


## 0.30 validated canonical baseline

Status: **VALIDATED — current canonical ASI baseline**.

User validation confirms the WSModel small-object hard-cull correction is good
in game.

Validated 0.30 delta over 0.29:
- exact branch at RVA 0x0023954E;
- retail `7A 1A` -> canonical `EB 1A`;
- skips only the size-derived WSModel+0xA8 hard-cull rewrite;
- preserves the independent +0xAC shadow-distance calculation;
- keeps the constructor default +0xA8 = 10000.

Canonical INI:
- `Fixes.WSModelSmallObjectHardCullBypass=1`.

All validated 0.29 Scaleform UI-cache changes and all earlier graphics,
DepthBlur, AO, shadow, distance, streaming and post-processing fixes remain
unchanged.

0.30 supersedes 0.29 as the canonical ASI baseline.
