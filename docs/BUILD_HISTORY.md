# Build history and validation state

## Cumulative-build rule

**The newest validated build is always the complete cumulative patch.**

Current retained cumulative build: **V311**

Original retail EXE SHA-256:  
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V302 EXE SHA-256:  
`db96ff3f6f8e38acdd262a87b0bb0ded7c604abca82e86e2501a12347f512459`

Current work: **architectural / VeryFarScene distance expansion from validated V311**

V292A EXE SHA-256:  
`aad83b503c6a8ae9159a9b960e602c0eade4417bd76d5de82c66c2cc05b5bc28`

## Earlier validated lineage

- **V137**: dual-layer minimap transform validated.
- **V138**: world `om_*` and minimap `mm_*` assets proven separate.
- **V140/V147**: world marker scale 70%.
- **V200**: cumulative streaming/HUD baseline.
- **V255A**: native 100% sniper-scope exception.
- **V257**: Max Engine baseline.
- **V258G/M/P/W/Y**: ObjectiveTray/minimap/final HUD lineage.
- **V259**: final world-marker geometry.
- **V260**: WSDecal active ceiling + pool 400 -> 800.

## Validated engine-cap lineage

| Build | Validated change |
|---|---|
| V261 | WSPhysicsParticle 1000 -> 2000 + matching runtime cap |
| V262 | Havok TOI queue 512 -> 1024 |
| V263 | WSParticleRender arenas 4500/1000/500 -> 9000/2000/1000 |
| V264 | WSPhGridObject 1000 -> 2000 + matching active-count gates |
| V265 | WSParticleRender sort scratch 4500/1000 -> 9000/2000 |
| V266 | WSActivateSphere 256 -> 512 |
| V267 | WSParticleInfoData 1400 -> 2800 |
| V268 | WSParkingSpace 32 -> 64 |
| V269 | WallPoint / WallSegment 50/50 -> 100/100 |
| V270 | WSLuaCall 20 -> 40 |
| V271 | WSDamageSphere 512 -> 1024 |
| V272 | WSReadJob / WSUncompressJob 1200/1200 -> 2400/2400 |
| V273 | WSInventoryStateStow 32 -> 64 |
| V274 | PblCRCTreeNode 40000 -> 60000 |

## Validated LOD / distance / resource-priority lineage

| Build | Validated change |
|---|---|
| V275 | SliceQuality High final outer range 500 -> 1500 |
| V276 | ObjectQuality High human ranges 70/150/300 -> 100/300/600 |
| V277 | RenderSlice3 High far bound 100 -> 300 |
| V278 | ModelInfo default LODDIST 1000 -> 1500 |
| V279 | VeryFarSceneTerrain dedicated range 5000 -> 10000 |
| V280 | WSDetailSystem maximum detail distance 100 -> 500 |
| V281 | WSDetailSystem maximum detail distance 500 -> 1000 |
| V282 | Streaming High coverage 1250 -> 2500 |
| V283 | Streaming Medium coverage 1600 -> 3200 |
| V284 | Streaming Low coverage 8000 -> 16000 |
| V285 | SS_HighPalette threshold 80 -> 160 |
| V286 | SS_HighPalette threshold 160 -> 200 |
| V287 | SS_HighPalette threshold 200 -> 250 |
| V288 | SS_HighPalette threshold 250 -> 300 |
| V289 | SS_HighPalette threshold 300 -> 400 |
| V290 | SS_HighPalette threshold 400 -> 500 |
| V291 | SS_HighPalette threshold 500 -> 650 |\n| V292 | SS_HighPalette threshold 650 -> 800 |\n| V293 | SS_HighPalette threshold 800 -> 900 |\n| V294 | SS_HighPalette threshold 900 -> 1000 |\n| V295 | SS_HighPalette threshold 1000 -> 1500 |
| V296 | SS_HighPalette threshold 1500 -> 1600 |

## Validated V295

V295 changes only the already-proven SS_HighPalette local comparison:

- compare instruction: VA `0x009EE461`, RAW `0x005ED661`
- V294 source: native double 1000.0 at VA `0x010A45B8`
- V295A source: native double 1500.0 at VA `0x010207E0`
- exactly 3 EXE bytes change
- no code cave
- no injected data
- no global constant modified

## Current V296A candidate

V296A starts from canonical V295 and changes only the already-proven SS_HighPalette local comparison:

- compare instruction: VA `0x009EE461`, RAW `0x005ED661`
- V295 source: native double 1500.0 at VA `0x010207E0`
- V296A source: native double 1600.0 at VA `0x010140A8`
- exactly 3 EXE bytes change
- no code cave
- no injected data
- no global constant modified

## Current V297A EXTREME candidate

V297A starts from canonical V296 and changes only the already-proven SS_HighPalette local comparison:

- compare instruction: VA `0x009EE461`, RAW `0x005ED661`
- V296 source: native double 1600.0 at VA `0x010140A8`
- V297A source: native double 1,000,000,000.0 at VA `0x00F86DE0`
- exactly 4 EXE bytes change
- no branch forcing
- no code cave
- no injected data
- no global constant modified

Fallback ladder if unstable: 100000 -> 10000 -> 5000 -> 2000.

## Important audit conclusions

- `SliceQuality=0` is the High table.
- `TextureQuality=3` already maps to a practical 32768-pixel ceiling.
- ShadowSlice shares the slice-distance system.
- Explicit ModelInfo `LODDIST25/30` overrides remain intact.
- VeryFarSceneMonuments contains inline 256-entry structures; increasing that structural count alone is rejected.
- Historical V236 offsets +0xC04/+0xC08/+0xC0C were misattributed and are not WSDetailSystem.
- Historical V233 GeometryDisk tests changed tuner data rather than the EXE.
- The neighboring HighPalette-classifier type-5 threshold 10.0 remains untouched because semantic ownership is not proven.
- The historical `Sleep(1) -> Sleep(0)` change is already present in the modern lineage.

## Frozen minimap

- Native/orange: X = 100, Y = 60
- Scaleform/black: X = 33.333333333333336, Y = 20

## Next rule

Future candidates start from **V311** or reproduce it exactly first.


## Current V297A x4 candidate

Final HighPalette stress step:

- 1600 -> 6400 (x4)
- compare VA `0x009EE461`
- source VA `0x010140A8 -> 0x01011D00`
- exactly 2 effective EXE bytes
- 6400.0 exists exactly once as a native double
- 16000.0 is absent as a native double
- no cave / no injected data / no forced branch

The abandoned 1,000,000,000 experiment is rejected as excessive and is excluded from the canonical lineage.

## Retained V298

V298 starts from V296 and retains a user-requested all-High RenderSlice increase. Corrected semantics: ModelInfo RENDERSLICE n is converted to (1 << n) - 1, so the old V277 label overstated what its record-3 change did for RENDERSLICE3 objects.

Effective High distance targets in V298:
- Slice0 far 16
- Slice1 far 80
- Slice2 far 200
- Slice3 far 1200
- outer/final far 6000

V298 EXE SHA-256: `57878890c4b9ba3bb9705109b4b216623d226b28598b3a187cf4b2795574a09a`

The user reported no obvious visual improvement or regression, but explicitly requested that these Slice increases be kept in all future builds. V298 is therefore the retained cumulative base.

## Current V299A candidate

V299A is built directly from V298 and preserves its RenderSlice table. It targets only WSWillToFightGrid low-resolution occupation data:

- CPU influence-grid dimension 256 -> 1024 via local source redirection
- LowResWorldWTF 256x256 -> 1024x1024
- LowResWorldWTFVertex 256x256 -> 1024x1024
- no WTF palette/color/intensity constants changed
- 8 effective EXE bytes changed versus V298

V299A EXE SHA-256: `46ddd1d3b146166b0220d7f0337db3c726500988863d4eb5d13554d3ee4a1abf`

The previous post-V296 HighPalette x4 and 1e9 stress experiments are not retained.


## Retained V302 and rejected V303-V308

V302 is now the retained cumulative baseline.

V302 starts directly from V298. V299-V301 are excluded. It scales the retained High SliceQuality distance structure coherently by 1.56103515625:

- Slice0 far: 24.9765625
- Slice1 far: 124.8828125
- Slice2 far: 312.20703125
- Slice3 far: 1873.2421875
- Slice4 start: 124.8828125
- outer/final far: 9366.2109375

V302 EXE SHA-256:
`db96ff3f6f8e38acdd262a87b0bb0ded7c604abca82e86e2501a12347f512459`

User validation:
- scenery: OK
- distant red issue: still visible

Rejected after V302:
- V303: all-profile Slice4.first historical test; red hidden, scenery broken.
- V304A: Q1+Q2+Q3 Slice4.first; red hidden, scenery broken.
- V304B: Q4 only; red visible, scenery degraded.
- V305A: Q1 only; red visible, scenery OK.
- V305B: Q2 only; red hidden, scenery broken.
- V306A: Q2 threshold 124.8828125; scenery broken.
- V307A: coherent Q2 x1.5625 table; scenery broken.
- V308A: Q2 RENDERSLICE3 far 50 -> 200; no visible improvement to the very-near prop pop.

Conclusion: V303-V308 are diagnostic branches only and must not be inherited.


## Validated V310 auto-classifier bypass

Base: V302.

Patch:
- VA 0x00639622 / RAW 0x00238822
- automatic unlisted-model classifier entry `D9 47 -> EB 4F`
- jumps directly to the existing mask pack/store path
- explicit ModelInfo models are untouched

Effect:
- unlisted WSModels retain full 0x1F automatic render masks instead of being
  reduced to 0x03/0x07/0x0F according to model size.

User result:
- previously very-near small-object pop visibly improved.

V310 EXE SHA-256:
`f209b6a96249b7a84b13efe5ef3c44d31aaddad38c51309ea24c9aabb8c1c993`

V310 is the current retained baseline.


## Validated V311 explicit ModelInfo full RenderSlice

Base: V310.

Patch:
- VA 0x006395AF / RAW 0x002387AF
- `0F B6 4A 02 -> B1 05 90 90`
- explicit ModelInfo RenderSlice is forced to 5
- existing conversion produces `(1<<5)-1 = 0x1F`

Untouched:
- explicit ShadowSlice
- explicit ZPassSlice
- flags / ZCULL
- explicit LODDIST

User result:
- garage/workshop pop corrected as well.

V311 EXE SHA-256:
`d87283851502782c1a9d566eeacbd33f40a48f10b31d0983237522386826284e`

V311 is the current retained baseline.
