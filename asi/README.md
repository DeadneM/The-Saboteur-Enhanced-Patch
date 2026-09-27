# SaboteurEnhanced ASI

## Current test build: 0.8A — safe visual baseline

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
