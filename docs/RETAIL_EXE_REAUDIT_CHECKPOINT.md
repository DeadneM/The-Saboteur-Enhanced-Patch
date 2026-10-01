# Retail EXE re-audit checkpoint — ASI 0.10 recovery

Status: **WIP audit checkpoint — not a validated release**

This document is the authoritative restart point for the post-Core1 ASI work.
It exists specifically to prevent old cumulative EXE assumptions from being
reintroduced without proving their retail owner.

## Baseline

Exact retail executable used for the re-audit:

- file: `Saboteur.exe`
- size: 14,834,176 bytes
- SHA-256: `e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

Validated architecture remains:

- **Core 1** executable:
  `83995ab6e04fce264309a37a243744d4f5929c5abb87c3f195fb50fef7f5f2a8`
- **ASI 0.1** is the validated runtime architecture baseline.
- Later 0.7–0.10 work is research/WIP until the mappings below are corrected
  and retested.

No tuner.txt architecture is used.

## Critical corrections discovered by the retail re-audit

### 1. `CSMQuality` is incorrectly identified

The current ASI key named `CSMQuality` does **not** represent a CSM quality
selector.

The targeted byte belongs to the embedded
`WSAmbientOcclusionFilter.hlsl / PsDepthConv` shader instruction stream.

The historical V200 change was a coherent shader instruction rewrite. Treating
one changed byte as an integer quality level creates an incomplete instruction
rewrite and is invalid.

**Decision:** remove/deprecate `Shadows.CSMQuality`. Never interpret values
such as 2 or 5 as CSM quality levels.

This is a strong candidate for the visible line/banding regression seen in the
ASI 0.8 screenshot.

### 2. Full-resolution AO must be reconstructed as one coherent feature

Historical V200 full-resolution AO is not one byte.

The recovered family contains:

1. two CPU-side half-resolution dimension shifts for `AmbientOcclusionBB`
   that are removed for full resolution;
2. a `PsAmbientOcclusion` scale change 2.0 -> 1.0;
3. the matching `PsDepthConv` instruction/operand rewrite so coordinate
   conversion remains coherent at full resolution.

The current ASI implementation is incomplete and also overlaps bytes belonging
to `PsBloomFinal`.

**Decision:** replace the current AO patch with a verified atomic AO feature.
All required original bytes must match before any member of the group is
written.

### 3. Shadow-map resolution and PCF selection are separate systems

The historical 24 PCF-related constant changes do **not** select PCF 5x5.
They scale sampling offsets to compensate for the shadow-map resolution change
1024 -> 4096.

Recovered relation:

- 1024: native texel offsets
- 2048: offset scale 0.5
- 4096: offset scale 0.25

The real historical PCF 3x3 -> 5x5 switch is a separate pair of shader
selection changes:

- shader selection `+0xF4 -> +0x10C`
- shader selection `+0xE8 -> +0x100`

The target blobs identify as `SampleShadowMapPCF3x3` and
`SampleShadowMapPCF5x5`.

**Decision:**

- `ShadowMapResolution` owns shadow-map dimensions and automatically derived
  texel/sample offsets.
- a separately verified PCF kernel option may select 3x3 or 5x5.
- retire the misleading `ShadowPCF5x5` meaning currently attached to the
  offset constants.

### 4. ToneMap 0.15 is not a quality improvement

The embedded `PsToneMap` shader uses the recovered 0.25 constant directly in
the RGB output multiplication.

Changing 0.25 -> 0.15 therefore reduces that contribution by 40%.

**Decision:** retail 0.25 is the safe baseline. The old 0.15 value must not be
treated as an image-quality improvement and is a plausible contributor to the
reported dull/flat image.

### 5. `PsBloomFinal` historical changes are artistic, not AO quality

V200 contains two linked `PsBloomFinal` changes:

- a constant 4.0 -> 2.0 in a MAD-related contribution;
- sampler selection `SkyBloomTextureSampler (s4) -> BackBufferSampler (s0)`.

These materially change the final bloom composition.

**Decision:** keep them out of AO and out of defaults. If retained at all, they
must be separate experimental artistic controls after exact shader semantics
are documented.

### 6. RenderSlice / ShadowSlice classification

The table historically called RenderSlice High is consumed by the
SliceQuality/ShadowSlice/CSM path. It must not be described as a generic object
draw-distance table.

Recovered High profile:

| slice | start | far |
|---|---:|---:|
| 0 | 0.1 | 4 |
| 1 | 4 | 20 |
| 2 | 20 | 50 |
| 3 | 50 | 100 |
| 4 | 80 | 500 |

The CSM path also uses lambda 0.5 and an approximately 99.9 far value while
building split boundaries.

**Decision:** move the existing `RenderSlice3HighFar` and
`RenderSliceHighOuter` concepts out of generic `[Distances]`. They require
shadow/slice-specific naming and isolated A/B testing because aggressive
values can produce visible transition lines.

## Streaming findings corrected from retail

### Coalesced read byte limit

Retail code compares the cumulative coalesced read size against:

- `0x0007D000` = **512,000 bytes**

Historical V200 changed that threshold directly to:

- `0x08000000` = **128 MiB**

This was not merely "retail +32 MiB".

### Coalescing sector thresholds

Recovered retail -> V200 changes:

- contiguous span threshold: 127 -> 4096 sectors
- merge gap threshold: 40 -> 192 sectors
- merged total span threshold: 127 -> 4096 sectors

Sector size in this path is 2048 bytes.

### Async 32

The old Async32 feature is **not** a one-immediate 16 -> 32 patch.

V200 introduced a scheduler/hook that maintained an in-flight I/O counter and
submitted multiple jobs. Retail's corresponding routine keeps a single current
job pointer and submits one at a time.

**Decision:** do not expose a fake `AsyncReads=32`. A future implementation
must be a real ASI hook with matched submit/completion accounting and telemetry.

## Havok owners confirmed

Retail:

- TOI event queue: **250**
- broad-phase query size: **1024**

Historical V200:

- TOI event queue: 250 -> 512
- broad-phase query size: 1024 -> 2048

Later cumulative work raised TOI further.

Both are now identified well enough to be future independent INI controls,
but retail defaults remain the safe baseline.

## Scaleform / high-resolution UI resource limits recovered

Historical V200 also changed:

- `_Mesh_Cache`: 8 MiB -> 16 MiB
- vector glyph cache: 512 -> 1024
- two font-cache constructors: texture/count field 1 -> 2

These are promising high-resolution UI controls. They belong in a future
`[UI]` section, not in graphics/LOD tuning.

## Previously recovered engine-limit owners

The following retail owners have been reconstructed and should remain
native-default until isolated pressure/telemetry justifies increases:

- WSDecal 400, with pool and active ceiling
- WSPhysicsParticle 1000, allocation plus runtime ceiling
- WSParticleRender 4500 / 1000 / 500, with arenas, scratch and runtime caps
- WSLuaCall 20
- WSParkingSpace 32
- WSParticleInfoData 1400
- WSActivateSphere 256
- WallPoint / WallSegment 50 / 50
- WSDamageSphere 512
- WSInventoryStateStow 32
- WSReadJob / WSUncompressJob 1200 / 1200
- PblCRCTreeNode 40000, structural ceiling 65534 because 0xFFFF is sentinel

## Known-good visual/runtime findings that remain useful

These are not invalidated by the corrections above, although aggressive
defaults still require isolated testing:

- environment-map resolution owner
- anisotropic filtering owner
- MIP LOD bias owner
- shadow-map resolution owner
- CSM lambda owner
- CSM far-distance owner
- shadow depth-bias scale owner
- shadow slope-bias scale owner
- AO blur/erode scale owners
- stream coverage table
- FarScene owner
- decal visibility owner
- ModelInfo default LOD-distance owner
- VeryFarScene terrain-specific owner
- HighPalette owner, experimental/native-default
- WSDynamicPart priority radius, experimental
- V310/V311 model render-mask fixes

## Current safety classification

### KEEP / preserve architecture

- Core 1
- ASI loader/runtime architecture
- exact retail signature/original-byte verification
- V310/V311
- one finding = one independently documented INI owner
- fail closed on byte/signature mismatch

### RECONSTRUCT before next test build

- full-resolution AO
- shadow-map resolution + automatically derived PCF offsets
- true PCF 3x3/5x5 selection
- shadow/slice table naming and ownership

### RESET TO RETAIL DEFAULT

- ToneMap: 0.25
- AO full-resolution until reconstructed
- shadow slice / RenderSlice aggressive distances until isolated
- any old CSMQuality value

### EXPERIMENTAL ONLY

- PsBloomFinal artistic changes
- HighPalette
- broad engine-pool increases without telemetry
- aggressive streaming coalescing
- Async32 scheduler
- WTF resource-resolution experiments

### REMOVE / DEPRECATE

- current `Shadows.CSMQuality` interpretation
- current misleading `ShadowPCF5x5` interpretation
- any AO patch that also writes `PsBloomFinal` bytes

## Resume order

1. Correct/deprecate invalid INI keys and compiled fallbacks.
2. Rebuild AO as one atomic verified patch.
3. Rebuild shadow-map resolution so sampling offsets are derived automatically.
4. Add a real independently verified PCF 3x3/5x5 selector.
5. Restore retail ToneMap 0.25 as default.
6. Reclassify ShadowSlice/RenderSlice controls and test them independently.
7. Only then add new UI/Havok/streaming controls.
8. Build one clean candidate from Core1 + corrected ASI.
9. User test specifically for:
   - line/banding regression;
   - image brightness/contrast;
   - shadow transitions;
   - AO halos;
   - distant pop-in.
10. Promote only after the test result is recorded.

## Release rule

Do **not** call the current 0.10 research state validated.

The public patcher remains independently pinned to its previously documented
fail-closed baseline until a corrected ASI candidate is built and validated.
