# Build history and validation state

## Cumulative-build rule

**The newest validated build is always the complete cumulative patch.**

Current canonical cumulative build: **V280**

Original retail EXE SHA-256:  
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V279 EXE SHA-256:  
`db2ac0f79b2fcace02ac32d77bdea32c5d9591f6bee56715e9e1976f81999b0a`

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
| V263 | WSParticleRender arenas A/B/C: 4500/1000/500 -> 9000/2000/1000 |
| V264 | WSPhGridObject 1000 -> 2000 + three active-count gates |
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

V274 closed the straightforward generic fixed-pool sweep. Spill-enabled reserves are not treated as hard limits, and inline-layout pools such as WSAICorpse remain rejected where increasing capacity would desynchronize manager layout.

## Validated LOD / distance lineage

| Build | Validated change |
|---|---|
| V275 | SliceQuality High final outer range 500 -> 1500 |
| V276 | ObjectQuality High human ranges 70/150/300 -> 100/300/600 |
| V277 | RenderSlice3 High far bound 100 -> 300 |
| V278 | ModelInfo default LODDIST 1000 -> 1500 |
| V279 | VeryFarSceneTerrain dedicated range 5000 -> 10000 |\n| V280 | WSDetailSystem maximum detail distance 100 -> 500 |

## Current candidate

**V281A TEST** starts from validated V280 and raises only the proven WSDetailSystem +0x218 maximum detail distance from 500 to 1000, including initial value, maximum comparison, and clamp replacement.

V281A EXE SHA-256:
`2125cf72e5a0743e468f50f3c2e33651d292173e81d8e92b471a83925dc76549`

## Important audit conclusions

- `SliceQuality=0` is the High table.
- `TextureQuality=3` is already effectively unrestricted for retail assets.
- ShadowSlice shares the slice distance system; no fake duplicate shadow-distance build was created after V277.
- Per-model `LODDIST25/30` overrides remain intact.
- V279 modifies only VeryFarSceneTerrain-specific loads; Monuments and DetailSystem remain separate.
- The old proposed `Sleep(1) -> Sleep(0)` streaming change is a no-op in the modern lineage because V270 already contains `Sleep(0)`.

## Frozen minimap

- Native/orange: X = 100, Y = 60
- Scaleform/black: X = 33.333333333333336, Y = 20

## Next rule

Future candidates start from **V280** or reproduce it exactly first.
