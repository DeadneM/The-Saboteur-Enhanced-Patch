# The Saboteur Enhanced Patch

Experimental PC enhancement patch for **The Saboteur**, developed through static binary auditing and in-game validation.

> **Current retained cumulative build: V310**  
> **Current Windows patcher source/CI target: v1Pv260**  
> **Current work: continue object draw-distance expansion from validated V310**

## Current canonical build

V302 is the current retained cumulative game state. It keeps the validated engine/LOD lineage through V296, retains the V298 RenderSlice expansion, and applies the later coherent High SliceQuality expansion that was validated with scenery intact.

Original retail EXE SHA-256:

`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

Current V302 EXE SHA-256:

`db96ff3f6f8e38acdd262a87b0bb0ded7c604abca82e86e2501a12347f512459`

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

## Validated V286

V286 advances only the same proven SS_HighPalette comparison threshold:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V285 source: native double 160.0 at `0x00FA4110`
- V286A source: native double 200.0 at `0x00F7B778`
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 3 effective EXE bytes change

V286 EXE SHA-256:

`40fd97c7dc5ca5352983cc2073e9e851e665b8afa188d29040b3603047b1df38`

The neighboring type-5 threshold remains untouched because its ownership cannot yet be proven cleanly as SS_LowPalette.

## Validated V287

V287 advances only the same SS_HighPalette compare:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V286 source: native double 200.0 at `0x00F7B778`
- V287A source: native double 250.0 at `0x00F97DD8`
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 3 effective EXE bytes change

V287 EXE SHA-256:

`ee446ee76b6356371d4ad76f9125331c939d09b6da32890ebf02d0c5ffc93590`

The neighboring type-5 threshold remains untouched because its semantic ownership is still not proven.

## Validated V288

V288 advances only the same SS_HighPalette compare:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V287 source: native double 250.0 at `0x00F97DD8`
- V288A source: native double 300.0 at `0x00F94648`
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 2 effective EXE bytes change

V288 EXE SHA-256:

`88ccee9b4eb11115a4cfe2981f4f46e59766c97d0c170dcbb67321118d315213`

The neighboring type-5 threshold remains untouched because its semantic ownership is still not proven.

## Validated V289

V289 advances only the same proven SS_HighPalette compare:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V288 source: native double 300.0 at `0x00F94648`
- V289A source: native double 400.0 at `0x00FAA2D0`
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 3 effective EXE bytes change

V289 EXE SHA-256:

`ad1874548a8c4c6381ad982b1d6ed48fe653d97a43314a518de57963e964774c`

V289A ZIP SHA-256:

`5f4f0d3263607228d98bd9be3d517ab0f3fdc36588eea5ba35b7899eefa18d0f`

There is no native double 350.0 in the executable, so 400.0 is the next clean native step after validated 300.0.

## Validated V290

V290 advances only the same proven SS_HighPalette compare:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V289 source: native double 400.0 at `0x00FAA2D0`
- V290A source: native double 500.0 at `0x00F97DE0`
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 3 effective EXE bytes change

V290 EXE SHA-256:

`f27a94661eb446508ccbbbed6057fd36261ca877f5e54d4d5ccfae0dfb9fa5a8`

V290A ZIP SHA-256:

`72a3b8916ba48d7a643f8eabcc9a9d5cf7241abede0ef9a9018a3cae213d342f`

## Validated V291

V291 advances only the same proven SS_HighPalette compare:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V290 source: native double 500.0 at `0x00F97DE0`
- V291A source: native double 650.0 at `0x00FEF000`
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 3 effective EXE bytes change

V291 EXE SHA-256:

`05555941326fdf66253f166037eb0a51b9aa04aaa45cf1de61721fdb152bc9b2`

V291A ZIP SHA-256:

`83636b031ecdb8933217b2ccbf9cb87b2d4879e21b48e7bec0dc9ed14e49d7b2`

No native double 600.0 exists in the executable. The next clean native threshold above 500.0 is 650.0.

## Validated V292

V292 advances only the same proven SS_HighPalette compare:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V291 source: native double 650.0 at `0x00FEF000`
- V292A source: native double 800.0 at `0x00FD8CF8`
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 3 effective EXE bytes change
- native doubles 700.0 and 750.0 are absent from the executable

V292 EXE SHA-256:

`aad83b503c6a8ae9159a9b960e602c0eade4417bd76d5de82c66c2cc05b5bc28`

V292A ZIP SHA-256:

`12e24f9b19222016a5a6a42b49a556e9e64a824add994b9e688e5f7d36ad01ee`

## Validated V293

V293 advances only the same proven SS_HighPalette compare:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V292 source: native double 800.0 at `0x00FD8CF8`
- V293A source: native double 900.0 at `0x00F82660`
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 3 effective EXE bytes change

V293 EXE SHA-256:

`e8f4177caab596f2aaca8a0c7da1bc43f1601bbc15efab325045974b8aabbb92`

V293A ZIP SHA-256:

`9069d70bdfcdc7e3e71642cb58af0e2be0955a384ce45ed70549d3a7dedc106d`

## Validated V294

V294 advances only the same proven SS_HighPalette compare:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V293 source: native double 900.0 at `0x00F82660`
- V294A source: unique native double 1000.0 at `0x010A45B8`
- RAW source for 1000.0: `0x00CA37B8`
- PE section mapping was explicitly verified
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 4 effective EXE bytes change

V294 EXE SHA-256:

`888cae2157ff38570f56e8eb6ee974cc45892b2cc84f78f3cf3c465ee4c741f8`

V294A ZIP SHA-256:

`2c745ca8066f1bdf4f9b1f5a4aa13f3707bd01928871d28c340afe51c8246ed3`

## Validated V295

V295 advances only the same proven SS_HighPalette compare:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V294 source: native double 1000.0 at `0x010A45B8`
- V295A source: read-only native double 1500.0 at `0x010207E0`
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 3 effective EXE bytes change

V295 EXE SHA-256:

`5ed95c8d052e130b0593c28b1f85e306f846784bd012a7614cb586dddcb918a9`

V295A ZIP SHA-256:

`305d16c12b01c0c5f17f375822a64443c341fa15634183f1d7a06a9beae5713f`

There are two native double 1500.0 values. V295A deliberately references the copy in `.rdata` at VA `0x010207E0`, not the copy embedded in `.text`.

## Validated V296

V296 advances only the same proven SS_HighPalette compare:

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V295 source: native double 1500.0 at `0x010207E0`
- V296A source: unique native double 1600.0 at `0x010140A8`
- source RAW for 1600.0: `0x00C132A8`
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 3 effective EXE bytes change

V296 EXE SHA-256:

`2c40dfe0953b300d2da65ea6cdf1f7d02df50cb013dabe7101a51199b4208b5f`

V296A ZIP SHA-256:

`6376d2bae8479885e33ba56cb75a1da4162db77b9b908eb0e5006c33c4e1db46`

## Rejected V297 HighPalette stress tests

The post-V296 HighPalette stress experiments are not retained in the cumulative lineage. The 1e9 experiment was rejected as excessive; the later x4/6400 candidate was superseded when work moved to RenderSlice and WTF rendering.

- metric remains `resource[+0x1F8] / resource[+0x1F4]`
- compare site remains VA `0x009EE461`
- V296 source: native double 1600.0 at `0x010140A8`
- V297A source: native double 1,000,000,000.0 at `0x00F86DE0`
- no branch forcing
- no shared constant is modified
- no code cave, padding data, or injected constant
- exactly 4 effective EXE bytes change

V297A EXE SHA-256:

`990cb7d7fad5b78ed272675f0a50c522c7509c1ab03b496f7fc2014a746ab94e`

V297A ZIP SHA-256:

`b50a7878b3c34fff450fc1d1c10ddca8a392094ff84b148dd64491c8982ec916`

If the extreme threshold causes crashes, VRAM pressure, or frame-time regressions, fallback candidates are 100000, 10000, 5000, then 2000 using the same isolated local operand.

See [docs/LOD_DISTANCE_AUDIT.md](docs/LOD_DISTANCE_AUDIT.md) for the detailed map.


## Retained V298 — all High RenderSlice distances

V298 is retained for all future builds at the user's request. A corrected audit proved that ModelInfo RENDERSLICE n is converted to (1 << n) - 1; therefore a model marked RENDERSLICE3 is bounded by slices 0+1+2, which explained the observed ~50-unit pop-in wall.

V298 raises the effective High RenderSlice distance chain to approximately:

- Slice0 far: 4 -> 16
- Slice1 far: 20 -> 80
- Slice2 far: 50 -> 200
- Slice3 far: 300 -> 1200
- outer/final far: 1500 -> 6000

Only five effective EXE bytes differ from V296.

V298 EXE SHA-256:

`57878890c4b9ba3bb9705109b4b216623d226b28598b3a187cf4b2795574a09a`

V298 ZIP SHA-256:

`ce05ce809f549b6710316e89246679f4e09edde6a92e7f9d1d78d46a45adf445`

## Current V299A test — WSWillToFightGrid low-res WTF 1024

The distant red-material investigation is no longer treated as a Will-to-Fight root-cause problem after negative V299-V301 diagnostics. The retained branch has returned to renderer/LOD ownership analysis. Static audit proved that the live PC path uses WSWillToFightGrid resources LowResWorldWTF and LowResWorldWTFVertex, both created at 256x256, plus a matching 8-bit CPU influence buffer sampled bilinearly and normalized by 255.

V299A keeps all V298 RenderSlice changes and increases only this WTF spatial representation:

- CPU/grid dimension source: 256 -> 1024
- LowResWorldWTF: 256x256 -> 1024x1024
- LowResWorldWTFVertex: 256x256 -> 1024x1024
- WTF colors, greyscale, ambient/diffuse values and intensity range remain untouched
- exactly 8 effective EXE bytes change versus V298

V299A EXE SHA-256:

`46ddd1d3b146166b0220d7f0337db3c726500988863d4eb5d13554d3ee4a1abf`

V299A ZIP SHA-256:

`93203c84582697bfa1ce2bde1419474ed4ffb180ad428e857b90118e99686e50`

## Frozen minimap

Do not change unless explicitly reopened:

- Native/orange: **X = 100, Y = 60**
- Scaleform/black: **X = 33.333333333333336, Y = 20**

## Windows patcher

The repository patcher is still intentionally pinned to **V260 / v1Pv260**. The retained executable research state is now V302.

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

- Validate V299A WTF low-resolution grid 1024 using the same occupied-zone / metallic-surface test locations.
- Continue VeryFarSceneMonuments / DetailSystem / FarFarScene ownership audit.
- Continue the distant red-material investigation through WSWillToFightGrid / WTF material calculations if V299A does not change the bug.
- Regenerate verified cumulative patcher payloads after the next public patcher sync point.

## Disclaimer

Unofficial community project. Back up the original executable before testing.


## Retained V302

V302 is the current retained cumulative research build and supersedes V298 as the working baseline.

It starts directly from V298 and excludes the rejected V299-V301 WTF diagnostics. The High SliceQuality table is expanded coherently by the historical factor 1.56103515625:

- effective Slice0 far: 16 -> 24.9765625
- effective Slice1 far: 80 -> 124.8828125
- effective Slice2 far: 200 -> 312.20703125
- Slice3 far: 1200 -> 1873.2421875
- Slice4 start: 80 -> 124.8828125
- outer/final far: 6000 -> 9366.2109375

User validation: scenery remained correct. The distant red issue remained visible, proving that the coherent High-table expansion is safe but does not solve that separate issue.

V302 EXE SHA-256:

`db96ff3f6f8e38acdd262a87b0bb0ded7c604abca82e86e2501a12347f512459`

### Rejected diagnostics after V302

- V303-V307: cross-profile / Q2 Slice4 diagnostics. Some hid the red symptom but damaged scenery; none are retained.
- V308: Q2 RENDERSLICE3 far 50 -> 200. No visible improvement to the very-near object pop; rejected.

Future distance work starts from V302 and must identify the actual owner of the remaining short-range object pop before changing another scalar.


## Validated V310

V310 starts directly from retained V302 and changes only the automatic WSModel
size-to-RenderSlice classifier used when no explicit ModelInfo record exists.

Native automatic masks:
- size < 0.3 -> 0x03
- 0.3 <= size < 0.6 -> 0x07
- 0.6 <= size < 4.0 -> 0x0F
- size >= 4.0 -> 0x1F

V310 bypasses that classifier and preserves the already initialized full masks:
- Render mask = 0x1F
- ZPass mask = 0x1F
- shadow-side auto mask = 0x1F

Patch:
- VA 0x00639622
- RAW 0x00238822
- `D9 47 -> EB 4F`

User validation: the very-near small-object pop shown in the garage/workshop route
was visibly improved. V310 is therefore the new retained cumulative baseline.

V310 EXE SHA-256:
`f209b6a96249b7a84b13efe5ef3c44d31aaddad38c51309ea24c9aabb8c1c993`

V309 remains rejected because bypassing the WSModel A8 hard-cull rewrite did not
change the observed pop.
