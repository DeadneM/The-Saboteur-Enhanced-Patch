# The Saboteur Enhanced Patch

Experimental PC enhancement patch for **The Saboteur**, developed through static binary auditing and in-game validation.

> **Current validated cumulative build: V279**  
> **Current Windows patcher source/CI target: v1Pv260**  
> **Current development axis: far-scene / LOD / draw-distance audit**

## Current canonical build

V279 is the complete validated cumulative game state.

Original retail EXE SHA-256:

`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

Current V279 EXE SHA-256:

`db2ac0f79b2fcace02ac32d77bdea32c5d9591f6bee56715e9e1976f81999b0a`

The latest validated build contains every retained validated change from the original executable up to that point. Rejected experiments are excluded.

## V261 -> V274 engine-cap lineage

After V260, the project completed a long hard-cap / pool audit:

- **V261** — WSPhysicsParticle: 1000 -> 2000, with matching runtime ceiling.
- **V262** — Havok TOI event queue: 512 -> 1024.
- **V263** — WSParticleRender arenas: 4500 -> 9000, 1000 -> 2000, 500 -> 1000.
- **V264** — WSPhGridObject: 1000 -> 2000 with matching active-count gates.
- **V265** — WSParticleRender sort scratch: 4500 -> 9000 and 1000 -> 2000.
- **V266** — WSActivateSphere fixed pool: 256 -> 512.
- **V267** — WSParticleInfoData fixed pool: 1400 -> 2800.
- **V268** — WSParkingSpace fixed pool: 32 -> 64.
- **V269** — WallPoint / WallSegment matched pools: 50 -> 100.
- **V270** — WSLuaCall fixed pool: 20 -> 40.
- **V271** — WSDamageSphere fixed pool: 512 -> 1024.
- **V272** — WSReadJob / WSUncompressJob: 1200 -> 2400.
- **V273** — WSInventoryStateStow: 32 -> 64.
- **V274** — PblCRCTreeNode: 40000 -> 60000, deliberately kept below the 16-bit index/sentinel boundary.

The generic fixed-pool sweep is now effectively exhausted. Spill-enabled reserves and structurally unsafe inline arrays are not increased just because a constant looks small.

## V275 -> V279 LOD / draw-distance lineage

The current visual-distance work is deliberately split into isolated builds:

- **V275** — SliceQuality High outer range: 500 -> 1500.
- **V276** — ObjectQuality High human ranges: 70/150/300 -> 100/300/600.
- **V277** — High RenderSlice3 far bound: 100 -> 300.
- **V278** — ModelInfo default LODDIST: 1000 -> 1500, using a local source redirection rather than modifying the shared global 1000 constant.
- **V279** — VeryFarSceneTerrain dedicated distance: 5000 -> 10000, by redirecting only four terrain-specific loads to the engine's native 10000 constant.

Important findings:

- `SliceQuality = 0` is the **High** profile. Higher numeric indices are not higher quality.
- `TextureQuality = 3` already maps to a practical no-downscale ceiling (32768 px), so no fake "TextureQuality 4" patch is used.
- ShadowSlice uses the same slice-class distance system; V277 already extends class-3 shadow reach.
- ModelInfo per-model `LODDIST25/30` overrides remain untouched by V278.
- VeryFarSceneTerrain is patched independently from VeryFarSceneMonuments and DetailSystem.

See [docs/LOD_DISTANCE_AUDIT.md](docs/LOD_DISTANCE_AUDIT.md) for the detailed map.

## Frozen minimap

Do not change unless explicitly reopened:

- Native/orange: **X = 100, Y = 60**
- Scaleform/black: **X = 33.333333333333336, Y = 20**

## Windows patcher

The repository patcher is still intentionally pinned to **V260 / v1Pv260**.

Reason: the public patcher is fail-closed. It will not be retargeted to V279 until direct upgrade payloads are regenerated and verified against exact supported source executables. Documentation may advance ahead of the public patcher; safety verification may not.

Current patcher designation:

`The_Saboteur_Enhanced_Patcher_v1Pv260.exe`

It recognizes exact known hashes, verifies every patch region and final SHA-256, stages replacement with rollback, and never bundles the retail game executable.

## Development rules

1. **Last validated build is always cumulative.**
2. Prefer surgical, isolated binary changes.
3. Warn before any global or intrusive approach.
4. Rejected experiments never enter the canonical base.
5. Future candidates start from V279 or reproduce V279 exactly first.
6. Patch proven hard caps, queues, LOD gates or subsystem values only after control-flow / ownership proof.
7. Do not modify shared constants globally when a local instruction/source can be redirected instead.
8. Keep the cumulative README / technical notebook sufficiently detailed to reconstruct the work without relying on chat history.

## Current open work

- Continue VeryFarScene / DetailSystem / FarFarScene coverage audit.
- Audit the FarFarScene GeometryDisk radius before changing it.
- Keep the distant red-prop fallback investigation separate from general draw-distance work.
- Regenerate verified cumulative patcher payloads after the next public patcher sync point.

## Disclaimer

Unofficial community project. Back up the original executable before testing.
