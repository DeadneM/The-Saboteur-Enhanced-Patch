# Build history and validation state

## Cumulative-build rule

**The newest validated build is always the complete cumulative patch.**

Current canonical cumulative build: **V288**

Original retail EXE SHA-256:  
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V288 EXE SHA-256:  
`88ccee9b4eb11115a4cfe2981f4f46e59766c97d0c170dcbb67321118d315213`

Current candidate: **V289A**

V289A EXE SHA-256:  
`ad1874548a8c4c6381ad982b1d6ed48fe653d97a43314a518de57963e964774c`

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

## Current V289A candidate

V289A starts from canonical V288 and changes only the already-proven SS_HighPalette local comparison:

- compare instruction: VA `0x009EE461`, RAW `0x005ED661`
- V288 source: native double 300.0 at VA `0x00F94648`
- V289A source: native double 400.0 at VA `0x00FAA2D0`
- exactly 3 EXE bytes change
- no code cave
- no injected data
- no global constant modified

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

Future candidates start from **V288** or reproduce it exactly first.
