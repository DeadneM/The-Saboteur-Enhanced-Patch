# SaboteurEnhanced ASI

## Current test build: 0.7 — modular graphics/engine INI migration

Validated baseline retained:
- Core 1
- V310 WSModel automatic RenderSlice full-mask correction
- V311 explicit ModelInfo RenderSlice full-mask correction
- x86 dinput8 proxy loader

## Purpose

ASI 0.7 begins restoring the proven graphics/render findings that were
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
