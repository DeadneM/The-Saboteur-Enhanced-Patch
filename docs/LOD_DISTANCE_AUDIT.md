# LOD / draw-distance audit

This document records the current renderer-distance map discovered during the V275+ audit.

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

The runtime adjustment loop around VA `0x00643030` updates the first four slice records but does not overwrite the static upper bounds patched by V275/V277.

## ClipRange

Native values:

- index 0 = 180.0
- index 1 = 270.0
- index 2 = 360.0
- index 3 = 1500.0

High uses `ClipRange = 3`.

V275 chose 1500 for the High outer endpoint specifically because it is already the engine's native High ClipRange value.

## ObjectQuality / human LOD

Vanilla High:

- 70 -> 150
- 150 -> 300

Validated V276 High:

- 100 -> 300
- 300 -> 600

Runtime thresholds are written into `0x0111772C/30/34/38` and consumed by the human camera/distance update path.

Low/Medium remain unchanged.

## ModelInfo

Parser record layout:

- +0x01 RenderSlice
- +0x02 ZPassSlice
- +0x03 ShadowSlice
- +0x04 LODDIST float

ModelInfo supports tags including:

- `RENDERSLICE0..4`
- `ZPASSSLICE0..4`
- `SHADOWSLICE0..4`
- `LODDISTxx`
- `ZCULL`
- `FOLIAGE`
- `ALWAYSRENDER`

The default LODDIST was 1000.0. V278 changes only the ModelInfo default source to native 1500.0. Explicit `LODDIST25/30` overrides remain unchanged.

## ShadowSlice

No independent High shadow-distance table was found.

ShadowSlice is a model classification/mask that uses the same slice-class distance system. Therefore V277's class-3 extension from 100 to 300 also affects models classified as SHADOWSLICE3.

A separate "ShadowDistance" build was intentionally rejected as duplicate/non-independent.

## TextureQuality

Native mapping:

- 0 -> max 128 px
- 1 -> max 256 px
- 2 -> max 512 px
- 3 -> max 32768 px

Therefore quality 3 already behaves as a practical original-resolution/no-downscale mode for retail assets. No artificial quality 4 is used.

## Water

Developer tuner exposes:

- Water LOD Dist = 7.0
- Water LOD Scale = 0.02

These remain untouched.

## VeryFarScene / FarFarScene

Distinct systems/labels observed:

- DetailSystem
- VeryFarScene
- VeryFarSceneTerrain
- VeryFarSceneMonuments
- FarFarScene GeometryDisk

VeryFarSceneTerrain uses two internal profiles with different parameters but a shared 5000.0 distance source. V279 redirects only the four terrain-specific loads to the engine's native 10000.0 constant.

V279 does **not** alter:

- VeryFarSceneMonuments
- DetailSystem
- shared global 5000.0
- FarFarScene GeometryDisk

The developer tuner exposes:

- `FarFarScene.FarFarScene.GeometryDisk.OuterRadius = 100`
- RadialSegments = 8
- CircularSegments = 8

The GeometryDisk value is the next audit target, but it must not be assumed to be world draw distance until its consumers are proven.

## Distant red-prop issue

The historical red distant-prop/fallback issue remains a separate investigation.

Current working hypothesis is compatible with a representation/streaming boundary, but V275-V279 are not described as a guaranteed fix for it. The fallback path should not be globally disabled merely to hide red proxies, because that can remove the prop instead of fixing its far representation.
