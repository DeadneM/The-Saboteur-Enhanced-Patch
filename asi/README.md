# SaboteurEnhanced ASI

## Current test build: 0.13 — full-resolution PostFX pass

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
