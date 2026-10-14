# Build history and validation state

## Cumulative-build rule

**The newest validated build is always the complete cumulative patch.**

Current retained cumulative build: **V311**

Original retail EXE SHA-256:  
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V302 EXE SHA-256:  
`db96ff3f6f8e38acdd262a87b0bb0ded7c604abca82e86e2501a12347f512459`

Current work: **clean Core EXE reconstruction audit before ASI migration**

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


## Rejected V312 / current V313A

V312A VeryFarScene thresholds:
- 22/49 -> 88/196
- user result: no visible change to the balcony/facade pop
- status: rejected
- V311 remains the retained baseline

V313A starts directly from V311 and targets the dedicated WSFarSceneObject render
path instead:
- two local 320.0 camera-depth cutoffs -> 1280.0 (x4)
- shared/global 320 constants are not edited
- V312 is not inherited
- test-only until user validation


## Rejected V314A — ModelInfo ZCULL

Base: validated V311.

Purpose:
- test whether explicit ModelInfo `ZCULL` ownership explains the remaining
  architectural balcony/facade transition.

Relevant content evidence:
- many `Ornate_Clamber_*`, `Ornate_WinDoor_Arch_*`,
  `Ornate_WinDoor_DoorHeader*`, `HalfCircle`, and `Head_A` entries carry
  `ZCULL`.

User result:
- **no visible movement of the balcony pop**

Status:
- V314A rejected
- V311 remains canonical
- V312/V313 remain rejected and are not inherited

Next branch:
- Repeat Nodes / Odin instancing / high-resolution segment retention.


## V315A diagnostic — Repeat Nodes ownership

Base executable: validated V311, unchanged.

V315A is a tuner-only ownership diagnostic:
- `Console.gfx.repeatnodes=off`
- no executable patch
- no rejected V312/V313/V314 changes inherited

Artifact:
- `Saboteur_V315A_DIAGNOSTIC_RepeatNodes_OFF_Overlay_ForV311.zip`
- SHA-256 `8fc0888d9c91e193f66d183798f592d178974b50ea22d8efdefec41f65fcdf9d`

Goal:
- prove or reject ownership of the remaining balcony/facade pop by the dedicated
  Repeat Nodes / Odin instancing renderer before patching any deeper Odin code.


## Architectural reset after V314

V311 remains the last retained cumulative **research** executable.

V312, V313 and V314 were rejected for the remaining balcony/facade pop.

The briefly prepared V315 tuner-based Repeat Nodes diagnostic was abandoned
before testing. The project will not use tuner.txt as the correction mechanism.

A full binary-retention audit now replaces the old cumulative-growth strategy.
See `docs/CORE_EXE_AUDIT.md`.

Future distribution target:
1. reconstructed clean Core EXE containing only proven early/system/UI/correctness
   fixes;
2. `SaboteurEnhanced.asi` for V310/V311 runtime rendering fixes, diagnostics,
   LOD/culling work and pool/queue telemetry.


## Core 1 TEST candidate

The first cleaned Core executable has now been built from the exact retail EXE.

- Retail SHA-256: `e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`
- Core1 validated SHA-256: `83995ab6e04fce264309a37a243744d4f5929c5abb87c3f195fb50fef7f5f2a8`
- direct changed bytes: **5,069**
- direct changed regions: **82**
- status: **validated in game**

Core1 deliberately excludes V257 Max Engine, Async32/broad streaming tuning, Sleep(0), V260+ pool/cap increases, broad LOD/distance tuning, HighPalette, V298/V302 expansion and rejected diagnostics.

Core1 retains the validated window/borderless/display path, Hor+ FOV, DirectInput Alt+F4, timeBeginPeriod(1), high-resolution HUD/UI scaler, Caps Lock HUD toggle with menu-safe behavior, reticle/scoped-sight handling, V258Y HUD/minimap placement, V259 markers, SuperRDX and the streaming FIFO full-queue correctness fix.


## SaboteurEnhanced ASI 0.1

Core 1 has now been validated in game and becomes the clean executable baseline.

ASI 0.1 is the first runtime candidate on top of that Core:
- x86 `dinput8.dll` proxy loader
- x86 `SaboteurEnhanced.asi`
- V310 WSModel full-mask fix enabled by default
- V311 explicit ModelInfo full-mask fix enabled by default
- exact .text signatures and original-byte verification
- runtime log: `SaboteurEnhanced.log`
- no tuner.txt modification

Hashes:
- Core 1 / `Saboteur.exe`: `83995ab6e04fce264309a37a243744d4f5929c5abb87c3f195fb50fef7f5f2a8`
- `SaboteurEnhanced.asi`: `22b4fc9737a2a1430d4967aacdde46c913d4742c2a045e7b31982476b4481ffd`
- `dinput8.dll`: `70cd6d950dbca54bec034d16edc9cbb622f07bab045ad1018343e655784c70d2`
- package ZIP: `dac55984b85630f7bf0b31c2f3c0253cec00f1528b5bc2f787862d36d86eab99`

The next research target is Odin/instancing instrumentation for the remaining
balcony/facade transition, followed by pool/queue telemetry.


## ASI 0.1 runtime validation

The first runtime ASI build is now validated in game.

The supplied runtime log confirmed:
- x86 `dinput8.dll` proxy loaded correctly;
- `SaboteurEnhanced.asi` initialized correctly;
- V310 signature found and patch applied at runtime VA `0x00639622`;
- V311 signature found and patch applied at runtime VA `0x006395AF`;
- clean ASI unload on game exit.

See `docs/ASI_0_1_VALIDATION.md` for the sanitized validation log.

This promotes **Core 1 + ASI 0.1** from test candidate to the current validated
architecture. V311 remains historical research reference only.


## ASI 0.2 Odin diagnostic

The validated Core 1 + ASI 0.1 architecture remains the stable baseline.

ASI 0.2 is a **diagnostic-only** runtime branch for the remaining balcony/facade
transition.

Static audit corrected an important assumption:
- recovered `OdinMeshInstance::RemoveHighResSegments` at VA `0x00E14BA0`
  is only `mov al,[ecx+33h] ; ret`;
- it is therefore traced as a state/query accessor rather than suppressed;
- `OdinMeshInstance::ReInstance` at VA `0x00E14BF0` is the actual
  non-trivial lifecycle routine.

Pass-through hooks trace:
- ReInstance
- recovered RemoveHighResSegments/query
- PreRelease
- IsFullyLoaded

No Odin rendering decision is changed.

Build hashes:
- ASI: `9ee3d7e8f977c6c7c63d6f4548a8f0e4742dfdaf377b038f22700dfb48ed5556`
- dinput8: `60c887b42c0be72a975af10862a9ddeca23c4ec5e348b62583f9b88c4c2b8241`
- test ZIP: `6e6228a03e9dd748f1b03307d0fa7e2b89185919e6388e78ee40768fbddf85dd`

See `docs/ASI_0_2_ODIN_DIAGNOSTIC.md`.


## ASI 0.3 Odin sync correlation diagnostic

ASI 0.2 established that Odin activity is real but broad vtable tracing is too
noisy for the remaining balcony/facade transition.

Runtime 0.2 totals:
- ReInstance: 723
- distinct ReInstance objects: 614
- RemoveHighRes/query: 0
- PreRelease: 0
- IsFullyLoaded/query: 2729

Static follow-up identified VA `0x00E14610` as the synchronization routine that
compares root state against instance `this+0x44` and invokes ReInstance only on
mismatch.

ASI 0.3 hooks only that routine and logs mismatches. It also adds **F9 visual
markers** so the user can mark the exact instant the balcony detail appears or
disappears.

No Odin rendering behavior is altered.

Hashes:
- ASI: `7916b14753327fea5a2397d3654e6826c4ea004b47cb0edf3dc47107bc2c2191`
- dinput8: `60fed9a57205819722686fc851beb03c86e3a79559f0387e1ef384181b03a11f`
- ZIP: `76f950e4aee986c59eb71fbd125e1500e22839842c6843abca7318fdcb65da2c`

See `docs/ASI_0_3_ODIN_SYNC.md`.


## ASI 0.4 Win32Mesh fingerprint diagnostic

ASI 0.3 produced a repeatable correlation with the balcony/facade transition:
the same six OdinMeshInstance/root pairs appeared immediately before F9 markers
#2/#4/#6/#8, while the same three-instance group appeared before #5/#7.

Every correlated root used vtable `0x0108198C`, statically identified as
**Win32Mesh**.

ASI 0.4 therefore fingerprints every unique Win32Mesh root seen within 1200 ms
before F9. It records structural/raw hashes, the first 0xC0 bytes, direct
printable strings and direct AHSM mesh names where available.

No rendering or streaming decision is modified.

Build hashes:
- ASI: `0936d7534821db66c586148df6264b702caeb0decb60870208bf8aa00becc1a4`
- dinput8: `3d7435365b06dce7344cb239e7e2dd4424d6da856a2cdc074d7609472f24576d`
- full five-file ZIP:
  `a633b844246a9bc15a464f37840ebeb487bd25c5d1b898f6188f92fe9400c726`

See `docs/ASI_0_4_WIN32MESH_FINGERPRINT.md`.


## ASI 0.6 WSDamageablePart variant-selector A/B

ASI 0.5 produced no visible change and is rejected.

Static follow-up found that VA `0x006678E0` recalculates the actual child
visibility bit and selects between two WSDamageablePart child groups using
`this+0x20 == 0/1` and `resource+0x28 & 1`.

ASI 0.6 changes only VA `0x006679AE`:

`22 44 24 24 -> 8A 44 24 24`

This preserves the incoming visibility and bypasses only the local variant
selector.

Hashes:
- ASI: `87418a751d62444cfe42a93902038f7a16e355f6d0ed0aaf3258cd753305a0cb`
- dinput8: `84c17e0b05749afd61ab9e0711f893d126bda259467f5bf5b42d09618d3b23b1`
- ZIP: `996684a339308b83dfc064089e74e047d5a36f03eb207ab9866599a81f45ad39`

See `docs/ASI_0_6_WSDAMAGEABLE_AB.md`.


## ASI 0.6 WSDynamicPart priority radius A/B

ASI 0.5 is rejected: the user observed no balcony/facade change. Static
follow-up showed that its virtual float was WSDamageable state, not distance.

The current static target is the WSDynamicPart priority function at
VA `0x00669980`.

It contains the proximity term `max(625 - x^2, 0)`, encoding a native radius
of 25. The float and double 625 constants are referenced only by this function.

ASI 0.6 changes both coherently to 2500, testing radius **25 -> 50** while
preserving the native score formula.

No Odin/F9/fingerprint diagnostics are active.

Corrected CI build after initialization audit:
- commit: `a1870c09c0740a3c7071d6132ebe682ae140c862`
- rejected WSDamageable selector default: ON -> OFF
- `WSDynamicPartPriorityRadius`: previously defined but not called -> now read/applied
- ASI: `7039ab89a36af71517159bbb353838881be35c679e5e5cfa784da4940dccaca5`
- dinput8: `e325949a73e3d581006bb2beb5e23054ff7600d54fd6c8cb14ea96ddb3779414`
- CI artifact ZIP: `e5913b0b0d01e32d7830b8066c8dfd96882d589f7ef46ca7dd29bba746fde5c7`

See `docs/ASI_0_6_WSDYNAMICPART_RADIUS.md`.

## ASI 0.45 canonical / 0.46 source cleanup / 0.47 red-proxy test

- 0.45 promoted canonical after release-path cleanup.
- 0.46 removes rejected/dormant Odin and WSDamageable source code; CI 239
  succeeds after retaining the shared `g_moduleBase` global.
- 0.47 opens the red-proxy correction branch from the cleaned source.

0.47 single functional delta:
- bypass native fallback entry RVA `0x00092E03`;
- `85 DB 0F 84 3B 01 00 00`
  -> `E9 3E 01 00 00 90 90 90`;
- skips only `rnd civilian prop(%d)` after the real WSCivilianProp lookup has
  already failed;
- native real-prop path remains intact.

This is the modern ASI port of the decisive V243A ownership test, not a shader,
material recolor, WTF, LOD or streaming change.

## ASI 0.47 rejected / 0.48 CivilianProp gate test

0.47 removed the red proxy but also removed pedestrian hand props. Rejected.

0.48 restores 0.46 functional behavior and tests only the historical V244A CivilianProp gate redirect at VA 0x00B85C80: 0x00474A20 -> 0x0048B560.


## ASI 0.49 runtime red-prop audit

- 0.47: diagnostic proof only; removed red proxy but also removed hand props.
- 0.48: superseded before validation; static V244A gate test not retained.
- 0.49: returns to clean 0.46 behavior and instruments the proven fallback
  at RVA `0x00092E03`.
- F6/F7/F8/F9 provide runtime A/B and memory snapshots in one game session.


## ASI 0.49 runtime result

0.49 diagnostic laboratory validated in game.

Result:
- hook/hotkeys/logging work;
- fallback-entry RVA 0x00092E03 is too early for proxy identity;
- 19/20 initial events had template=NULL;
- verbose/F9 snapshot also had template=NULL.

Next branch will instrument actual proxy creation around
VA 0x00492F38 -> 0x00988B30 and capture the returned object for direct
proxy-vs-real-prop comparison.


## ASI 0.50 native proxy-creation return probe (09 Oct 2026)

**TEST ONLY; never promote above canonical 0.45 before in-game evidence.**

Base source: main e429156 (0.49 runtime result), isolated on branch
dev/asi-0.50-proxy-creation-capture. No EXE or normal render/LOD/HUD/INI quality
changes. The old 0.49 broad-entry fallback hook is NOT installed in 0.50.

The new passive x86 CALL interception targets site RVA 0x00092F38
(VA 0x00492F38 at standard image base) and expects an E8 direct call to
RVA 0x00588B30 (VA 0x00988B30). It validates the opcode and decoded
rel32 call target and skips patching if either differs. The shim tail-jumps
to the genuine native target with entry GPRs, EFLAGS and argument stack
preserved, and its return thunk observes EAX after the native call finishes.
Per-thread 8-deep pre/post frames avoid correlating unrelated calls.
Sampled diagnostics include raw stack arguments, GPRs, returned EAX and
immediately copied candidate object, ECX and arg0 memory (guarded reads).
EAX is NOT yet proven to be a true prop pointer or material handle.

Hotkeys are deliberately changed: F6 quiet native, F7 capture next 16
completed creates (not proxy suppression!), F8 capture next 64, F9 replay
latest captured snapshot without dereferencing stale pointers. Real
WSCivilianProp successful-lookup path RVA 0x00092DD5 is read-only:
its original bytes are logged as groundwork, not patched. The paired
normal-prop creation hook still requires static owner verification.

CI build: Windows x86 SUCCESS (run 251, 2026-10-09).
ASI SHA-256: bdf6e043923234b73e44bb8d9f7fae1cb054e4a6cafdff7325fa713412b28696
Loader SHA-256: b741567a9ef766262cae9cab26a817d1204f705aab81e8d2894760b016216292
GitHub artifact ID: 11631392340
Artifact digest SHA-256: 2201b5f1efb7317523e4c9248b9f38e539ed23b872afa5b671551423b24aa441
Build artifact: https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/37960412042/artifacts/11631392340

Next test: known occupied-zone route, confirm [OK] PROXY50 hook (else
record [SKIP] observed bytes/target), press F8 and then F9, send
SaboteurEnhanced.log; report whether red proxies and pedestrian accessories
remain intact. Runtime behavior/stack integrity are not yet in-game validated.
Rollback to canonical 0.45 ASI + matching INI if any instability.


## ASI 0.50 runtime RESULT -> 0.51 out-parameter memory diff (10 October 2026)

**Validated by actual user log:** 0.50 loaded and hooked a real CALL
RVA 0x00092F38 -> RVA 0x00588B30. Native CALL completion events #1..#8
all returned EAX=00000000. This **does not prove** a missing proxy;
the function may use return status, a void return, or an out-parameter.
Input from #8: ECX=0AAE3D80, ESI/first argument=0B83DA50,
EDX/second argument=01B3F5A8, EDI=0AAE6E40.
F8 was enabled at 22:24:02 **after** last event #8 at 22:16:48;
F9 at 22:24:11 therefore replayed stale capture #8, not a new one.
The ASI unloaded normally; visual/gameplay state was not recorded in log.

**0.51 research approach:** Keep same strict checked native E8 CALL and
no-effects forwarding hook, copy candidate ECX/ARG0/ARG1/EDI pointee memory
256 bytes before/after each native call, log changed dword offsets, and
automatically capture the first 16 calls from startup to avoid missing
events before hotkeys. F6 cancels auto/quiet, F7 records next 16,
F8 next 64, F9 prints latest captured snapshot + total call count.
Read-only callee prologue and caller continuation bytes also logged.
No material/prop overrides, no target EXE changes, no speculative normal
WSCivilianProp hook, no LOD/quality experiments. This version is a
**diagnostic candidate**, not a red-prop correction.

Source branch: dev/asi-0.51-proxy-outparams-diff; initial source commit
829a711400ef7805bb3ccbdfa4101cd2080ecdff. Preserve canonical 0.45.
Detailed test and rollback procedure: docs/ASI_0_51_RUNTIME_AUDIT.md.


### CI build verification (10 October 2026)

- Workflow: GitHub Actions Build Core ASI x86, **run #253**, SUCCESS.
- URL: https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/38083836095
- Compiled source commit: `829a711400ef7805bb3ccbdfa4101cd2080ecdff`.
- ASI SHA-256: `c5bfc8b8da63d791b13740ef8404174be985ba9a2e9587faa99820423897acaf`.
- dinput8.dll SHA-256: `9cd4fa8329996d01c1c84701890e9df0c6aa10cc64c76536303a3e010ecdaf2`.
- GitHub artifact ID: `11681238421`, zipped payload SHA-256:
  `bfe0367731d4aa853ba32d7caf60287ea8f70327594f80f496f44d920bd4d06b`.
- Artifact URL: https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/38083836095/artifacts/11681238421
- Local ZIP verified good CRCs and **flat root** with exactly:
  `dinput8.dll`, `README.txt`, `SaboteurEnhanced.asi`, `SaboteurEnhanced.ini`.
- This validates **build and packaging only**, not in-game operation or the red-prop material cause. Do not promote to main/release before user's log/visual tests.


## ASI 0.52 TEST: native context follow-up from user-validated 0.51 log

**State (14 October 2026): COMPILED + PUBLISHED AS PRERELEASE, awaiting in-game validation.**

The supplied 0.51 log proved that the x86 CALL at RVA 0x00092F38
reached native target RVA 0x00588B30. It recorded 22 completed calls,
including 16 automatic full captures, and a clean ASI unload.
All captured EAX values were zero. That is not proof of a null
material/object: zero might be status/void residue. The actual
memory mutations occurred in ECX (+04,+08,+18,+1C,+34) and ARG0 (+48).
ARG1 and EDI did not change in the first 256 bytes.

0.52 keeps the 0.51 fast-path instrumentation and native behavior,
adding *read-only, guarded* 64-byte before/after samples around:
- ECX+0x207C deep candidate field, with no assumption of its meaning;
- ECX[+0x08] and ECX[+0x1C] child pointers, with distinct-object
  transitions identified (never compare two allocations as same object);
- EBX -> [EBX+0x1384] owner -> [owner+0x2DC] template, including
  snapshots of the same owner/template pointers before/after the call.

No change to EXE, shaders, materials, props, graphics, HUD, pool
capacities, LOD or streaming. The previous broad fallback suppression
hook is **not enabled**. All the first 16 calls are captured without F8;
F6 quiet; F7 next 16; F8 next 64; F9 latest + total.

Code source SHA: 7b9cc6d4e7f74d92f9e80c6b7f749601cfef29f8.
Core 1 + ASI 0.45 remains the canonical validated baseline.
0.52 publication: https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/tag/ASI-0.52-TEST
Download: https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/download/ASI-0.52-TEST/SaboteurEnhanced_ASI_0.52_NATIVE_STRUCTURE_CONTEXT_TEST_x86.zip

Windows x86 GitHub Actions run #254 SUCCESS:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/38542535779
ASI SHA-256 DAF2F41645C6DB7B6A0F1D4DB232E704EDC21863CBF3D4B1E9936CBE46FE14E4.
dinput8.dll SHA-256 393F19DFBEE81E66027CBA6402D93388CEA4D4D3BD9432679E6863D832CF7506.
Prerelease ZIP 186042 bytes; SHA-256
4bfe9c14d072f8fae5ec34ed918576c7ad9ff0d191cb6ab5bc38359cc1c47208.
Flat ZIP with exactly ASI, dinput8.dll, INI and README.
GitHub Actions artifact ID 11883181080, SHA-256
f88932e25e6fb79391fde023c884f3f57e576e512a74f9356c477cd7e84eb581.

Next test: reproduce red prop and F8/F9, send SaboteurEnhanced.log;
report red visibility and accessory retention. Do not declare visual
fix until controlled in-game testing proves it.


## ASI 0.52 user runtime result / ASI 0.53 tested compilation (14 October 2026)

0.52 `SaboteurEnhanced(3).log` confirmed: [OK] native hook;
24 calls counted; 16 automatically verbose and #24 captured after F8;
F9 replayed #24. No [SKIP]/[FAIL] messages. No ASI-unload
line; this file alone cannot establish exit conditions or a crash.
User did not report whether red texture persisted visually.

Crucial 0.52 insight: EBX as observed *at the native CALL* cannot be
assumed the WSHumanSpore actor, since owner[EBX+1384]
returned 0, 0xDDDDDDDD, 0x00010001 or other unreadable/invalid values.
ECX+0x207C changed on #5, #6, #7, #24, often float-like values;
this does NOT prove a color/material field. ECX child pointers changed
identities; same-object comparison must not cross pointer transitions.
EAX remains zero; a successful real proxy creation has NOT been proven
solely by completed native calls.

0.53 pivots from speculative field offsets to passive two-site
**temporal correlation**. New exact-signature-verified fallback gate at
RVA 0x00092E03 faithfully replays native TEST EBX / JE behavior,
without ever suppressing the fallback. It snapshots the gate's own EBX,
owner and template if readable, and per-thread sequence/timestamp.
The existing native CALL hook at RVA 0x00092F38 still captures 0.52
data, but labels its own EBX as unverified and attaches last
same-thread fallback sequence/age. A new 1-second proximity counter
is diagnostic only and NEVER proves identical object ownership.
F9 logs both call and fallback counters. No EXE/HUD/engine/material
parameters changed. Stable baseline remains Core 1 + ASI 0.45.

**CI build SUCCESS** GitHub Actions #255 (Windows MSVC x86):
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/38607768693
Source code commit `3b0f02eea1ccf2f96db8ff25f444e51e6370281f`.
ASI SHA-256:
`A6140FA767B3F33AB34986856B0DAB636520B1580ED542067AF6373CF33800BE`
Loader SHA-256:
`6C5CFF053270EEC5F33C2B96A2DB46A568B2AB32DED371C6E769351A9F7A6266`
Prerelase tag `ASI-0.53-TEST`:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/tag/ASI-0.53-TEST
Direct flat-root test ZIP:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/download/ASI-0.53-TEST/SaboteurEnhanced_ASI_0.53_FALLBACK_NATIVE_CORRELATION_TEST_x86.zip
ZIP size 187609 bytes, SHA-256
`9909fdb9190e054ff17a9574ad32ac0adab41d3c0d992d28ad36539fe2f064f2`.
CI artifact ID 11915733861, digest
`5204d7cd487977111cc1eaac6f752d0f5628ac768a72e9c7a5b0c9a9cdf61d9c`.
Flat ZIP verified at build time by ZIP entry-name comparison;
exact root members: dinput8.dll, SaboteurEnhanced.asi,
SaboteurEnhanced.ini, README.txt.

Do not promote 0.53 to main/stable before the user tests game
startup, red occupied zone, pedestrian-held accessories, and
fallback/native logs. Roll back to stable 0.45 ASI + INI on issues.


## ASI 0.53 user log verified -> 0.54 native ownership probe (14 October 2026)

User log `SaboteurEnhanced(4).log` (770 lines):
- Two exact hooks installed: fallback gate RVA 0x00092E03,
  native creator CALL RVA 0x00092F38 -> 0x00588B30.
- Gate count 78, with 77 nonzero EBX; native call count 16.
  All 16 native calls correlate with a fresh same-thread gate.
- **100% of 16 call events** have
  nativeEBX = fallbackGateEBX + **0xD78** AND
  nativeArg0 = fallbackGateTemplate.
  This is strongly corroborated by gate owner+template and the
  immediate call arguments, not proof of a rendered/material object.
- Exactly two distinct templates appeared among the 16 native
  calls: template 0x0B842520 linked to owner 0x0C60A960,
  and template 0x0B82DA50 linked to owner 0x0C5165A0.
  These absolute addresses are session-specific, not stable offsets.
- F8 arrived after native #16; F9 replayed #16. Normal
  ASI unload; visual red accessory outcome not reported.
- The previously guessed owner dereference nativeEBX+0x1384
  is invalid (it reads an interior address); RETIRED in 0.54.

0.54 observes guarded 256-byte before/after native-call snapshots
from precisely gate-proven owner and template, requiring per-event
actor +0xD78 AND template==ARG0 identities. It logs first-seen
template prefixes and link match/mismatch counts; does not
write them back, touch materials/shaders, change props or affect
EXE/graphics/HUD/LOD/engine limits. It preserves the normal
fallback and native CALL semantics. Only tested by compilation,
**not yet validated in-game or as a red-bug correction**.
Rollback: validated Core 1 + ASI 0.45 with matching INI.

Published TEST prerelease ASI-0.54-TEST:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/tag/ASI-0.54-TEST
Direct flat ZIP:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/download/ASI-0.54-TEST/SaboteurEnhanced_ASI_0.54_VERIFIED_OWNER_TEMPLATE_TEST_x86.zip
SHA-256 ZIP: ade596c6e4545f3e99efe0fb6d981bb6d8e5545139f950ceebd3b3ae8e885649
ASIx86: FF1CABD6FB575EA632403FBB6ED1497394F6F5185899F6595CEE44389180F276
Loaderx86: AD099393F655C10C1B3DFF80D7D9B551C8AA6FCFAB973DA152EA540F9E9020EC
GitHub CI #256: https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/38634986820
CI artifact ID 11926664618, SHA-256 c4f7ee9e835153c8b6710405dc546d6ea32539fc2bd363266be7c914ee9e6343.
Flat ZIP validated by CI: dinput8.dll, SaboteurEnhanced.asi,
SaboteurEnhanced.ini, README.txt directly at the archive root.


## ASI 0.54 verified player log / 0.55 x86 published prerelease (2026-10-14)

Player confirmed **red textures still visible** using 0.54. User log
`SaboteurEnhanced(5).log`: 15 distinct native completions,
15/15 gate+0xD78 matches, 15/15 template equals native ARG0,
15/15 valid/unchanged owner and template pointer identity.
Owner first 256 bytes: **0/15 modified**. Template first
256 bytes: **only +0x48 incremented, all 15/15**.
Two process-local template pointers, family A 0x0B8E2520
(5 events) and B 0x0B8CDA50 (10 events). Templates contain
literal strings `!Is_LowWtf` and `!Is_Raining` /
`Is_Raining`. Their meaning here is UNPROVEN, and these
are not discovered red-material offsets.
Normal ASI unload at 23:04:32.176; no F9 entry in this log.
Nothing in 0.54 modifies rendering, so persistent red is
consistent with being an observational build, not surprising
evidence of incorrect 0.54 instrumentation.

**0.55 pivots to a DIFFERENT original native path**:
`RVA 0x00092DD5` where original read-only runtime bytes
were `8B CD 85 C9 74 28 83 39 00 74 23`.
Verified hook uses 6 bytes (E9 rel32 + NOP) and faithfully
replays `mov ecx,ebp; test ecx,ecx; je 0x92E03`;
non-null falls through to original `cmp [ecx],0` at
`RVA 0x00092DDB`. Logs whether the **normal** table
lookup is null/unreadable/empty/populated, copies a
populated resource header when safely readable, and
correlates fallback gate entries with last same-thread,
same-actor lookup. F9 summarizes normal lookup classes
plus old fallback/native counts. No fixes, material edits,
false skipping of props or changes to graphics, EXE,
LOD, HUD or engine limits.

0.55 build Windows MSVC x86, GitHub Actions #257 SUCCESS:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/38637887900
Build source commit `8d17f253c21c32798e72909b739a311588c4cbc8`.
ASI SHA-256:
`3B2CD2B026902C0177F339BF6F3CF300704F7562091CE4A2F1028174206568AE`.
Loader SHA-256:
`595F0D21B0186EC64D03BABC377F66B628C7E19001811C53451D8AD9B6D6F694`.
Experimental prerelease:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/tag/ASI-0.55-TEST
Direct root-level ZIP:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/download/ASI-0.55-TEST/SaboteurEnhanced_ASI_0.55_NORMAL_LOOKUP_VS_FALLBACK_TEST_x86.zip
Size 190090 bytes; SHA-256
`b80930a2c6f35cc255aaf2780b3c7fcad501f6efcbd3a9927ef09c53150de982`.
Artifact ID 11928048765, sha256
`eed5c4b94833b1cd49c07828375f0fd2a430710d80410866322abfd49684c26c`.
CI verified flat ZIP exactly dinput8.dll, SaboteurEnhanced.asi,
SaboteurEnhanced.ini, README.txt in root.
Version 0.55 is untested in-game; Core1 + 0.45
remains validated baseline, and **main untouched**.
Test: red district, F8 BEFORE new props load, F9 for
normal-vs-fallback totals, send log; confirm visible red
and normal held prop behavior, rollback 0.45 on issue.
