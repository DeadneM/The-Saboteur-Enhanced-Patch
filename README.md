# The Saboteur Enhanced Patch

Experimental PC enhancement patch for **The Saboteur**, developed through static binary auditing and in-game validation.

> **Current validated cumulative build: V285**  
> **Current Windows patcher source/CI target: v1Pv260**  
> **Current test candidate: V286A — HighPalette priority threshold 160 -> 200**

## Current canonical build

V285 is the complete validated cumulative game state.

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

- **V275** — SliceQuality High outer range: 500 -> 1500.
- **V276** — ObjectQuality High human ranges: 70/150/300 -> 100/300/600.
- **V277** — High RenderSlice3 far bound: 100 -> 300.
- **V278** — ModelInfo default LODDIST: 1000 -> 1500, using a local source redirection rather than modifying the shared global 1000 constant.
- **V279** — VeryFarSceneTerrain dedicated distance: 5000 -> 10000, redirecting only four terrain-specific loads to the engine's native 10000 constant.

Important findings:

- `SliceQuality = 0` is the **High** profile.
- `TextureQuality = 3` already maps to a practical no-downscale ceiling (32768 px).
- ShadowSlice uses the same slice-class distance system; V277 already extends class-3 shadow reach.
- ModelInfo per-model `LODDIST25/30` overrides remain untouched by V278.
- The historical V233 `FarFarScene.GeometryDisk.OuterRadius` 25/400 A/B changed only `tuner.txt`, not the EXE. It is therefore kept out of the canonical EXE lineage until a native owner/storage path is proven.

## Validated V280

V280 revisits the previously isolated **WSDetailSystem** distance correctly on the modern V279 base.

`WSDetailSystem + 0x218` is initialized to 100.0 and independently clamped to a maximum of 100.0. V280A redirects all three local accesses to existing native 500.0 constants:

- VA `0x007ECD20`: initial value 100 -> 500
- VA `0x007ECDC3`: maximum comparison 100 -> 500
- VA `0x007ECDD0`: clamp replacement 100 -> 500

No shared constant is modified and no code cave is used.

V280 EXE SHA-256:

`d9d08f6c5aaa50435dd26909f0e969428bb880c3a30df14acc011e97167a6978`

V280 was validated in-game and is now canonical.

## Validated V281

V281 raises only the same proven WSDetailSystem distance field and matching clamp:

- `WSDetailSystem + 0x218`: 500 -> 1000
- VA `0x007ECD20`: initial value -> native float 1000.0 at `0x00F7D630`
- VA `0x007ECDC3`: maximum comparison -> native double 1000.0 at `0x010A45B8`
- VA `0x007ECDD0`: clamp replacement -> native float 1000.0

No global constant and no code cave is modified.

V281 EXE SHA-256:

`2125cf72e5a0743e468f50f3c2e33651d292173e81d8e92b471a83925dc76549`

Audit corrections made before V281A:
- the historical V236 +0xC04/+0xC08/+0xC0C fields are **not** WSDetailSystem; the real DetailSystem object is only 0x240 bytes.
- VeryFarSceneMonuments has inline 256-entry structures and no proven independent global distance scalar; a pool-only expansion is structurally unsafe and rejected.

## Validated V282

V282 changes only the HIGH-quality streaming-grid coverage:

- cell-size table: 500 / 60 / 25, unchanged
- coverage table in V281: 8000 / 1600 / 1250
- V282A: High coverage only 1250 -> 2500
- resulting High cell radius: 50 -> 100

Patch:
- VA `0x0104B04C`
- RAW `0x00C4A24C`
- float `1250.0 -> 2500.0`
- exactly 2 effective EXE bytes change

The historical V231 stress test multiplied all three tiers. V282A deliberately avoids that global method and isolates only the High tier.

V282 EXE SHA-256:

`f9188b4c7e30a513ae40106ff46c3da7640664314c96677ffe8f0b96a75990bd`

## Validated V283

V283 changes only the MEDIUM-quality streaming-grid coverage:

- cell sizes remain 500 / 60 / 25
- Low coverage remains 8000
- Medium coverage: 1600 -> 3200
- High coverage remains 2500
- Medium radius: ~26.67 -> ~53.33 cells

Patch:
- VA `0x0104B048`
- RAW `0x00C4A248`
- float `1600.0 -> 3200.0`
- exactly 2 effective EXE bytes change

V283 EXE SHA-256:

`6c1c582045219bc1d5d7a4c5124f30f69fd65504136ff488f89a30c64adcde72`

## Validated V284

V284 changes only the LOW-quality streaming-grid coverage:

- cell sizes remain 500 / 60 / 25
- Low coverage: 8000 -> 16000
- Medium coverage remains 3200
- High coverage remains 2500
- Low radius: 16 -> 32 cells

Patch:
- VA `0x0104B044`
- RAW `0x00C4A244`
- float `8000.0 -> 16000.0`
- exactly 2 effective EXE bytes change

V284 EXE SHA-256:

`6ee4a938ac322f7d7e8d327811d823220bde34dd044852f41b7f306c8b63d518`

## Validated V285

V285 changes only the per-resource priority threshold in the SS_HighPalette candidate path:

- runtime metric: resource[+0x1F8] / resource[+0x1F4]
- HighPalette compare site: VA `0x009EE461`
- V284 threshold source: native double 80.0 at `0x00FB54B0`
- V285A threshold source: native double 160.0 at `0x00FA4110`
- no shared constant is modified
- no private padding / code cave is used
- exactly 3 effective EXE bytes change

V285 EXE SHA-256:

`f2967a6863e07354311e3f84f070212031935f51b1714f6dc9ab5933433a15bc`

This replaces the historical V232 technique for future work. V232 tested 320.0 by storing a private double in executable padding; V285A instead reuses an existing engine-native 160.0 value for a cleaner x2 test.

## Current V286A test

V286A starts from validated V285 and advances only the same proven SS_HighPalette comparison threshold:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V285 source: native double 160.0 at `0x00FA4110`
- V286A source: native double 200.0 at `0x00F7B778`
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 3 effective EXE bytes change

V286A EXE SHA-256:

`40fd97c7dc5ca5352983cc2073e9e851e665b8afa188d29040b3603047b1df38`

The neighboring type-5 threshold remains untouched because its ownership cannot yet be proven cleanly as SS_LowPalette.

See [docs/LOD_DISTANCE_AUDIT.md](docs/LOD_DISTANCE_AUDIT.md) for the detailed map.

## Frozen minimap

Do not change unless explicitly reopened:

- Native/orange: **X = 100, Y = 60**
- Scaleform/black: **X = 33.333333333333336, Y = 20**

## Windows patcher

The repository patcher is still intentionally pinned to **V260 / v1Pv260**.

Reason: the public patcher is fail-closed. It will not be retargeted until direct upgrade payloads are regenerated and verified against exact supported source executables. Documentation may advance ahead of the public patcher; safety verification may not.

## Development rules

1. **Last validated build is always cumulative.**
2. Prefer surgical, isolated binary changes.
3. Warn before any global or intrusive approach.
4. Rejected experiments never enter the canonical base.
5. Future candidates start from the latest validated build or reproduce it exactly first.
6. Patch proven hard caps, queues, LOD gates or subsystem values only after control-flow / ownership proof.
7. Do not modify shared constants globally when a local instruction/source can be redirected instead.
8. Keep the cumulative README / technical notebook sufficiently detailed to reconstruct the work without relying on chat history.

## Current open work

- Validate V286A HighPalette priority threshold 200.
- Continue VeryFarSceneMonuments / DetailSystem / FarFarScene ownership audit.
- Keep the distant red-prop fallback investigation separate from general draw-distance work.
- Regenerate verified cumulative patcher payloads after the next public patcher sync point.

## Disclaimer

Unofficial community project. Back up the original executable before testing.
