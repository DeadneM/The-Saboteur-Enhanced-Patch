# Core EXE audit before ASI migration

Status: **audit / reconstruction plan**  
Historical cumulative reference: **V311**  
Future architecture: **clean Core EXE + SaboteurEnhanced.asi**

## Purpose

V311 is a valuable research snapshot, but it is no longer a suitable long-term
binary base. Too many stable experiments, capacity increases and draw-distance
changes accumulated in the executable even when their individual user-visible
benefit was weak or unproven.

The new rule is stricter:

> A change stays in the Core EXE only when it is an early/bootstrap requirement,
> a proven user-facing correction, or a genuine correctness fix that is cleaner
> and safer to keep before ASI initialization.

Everything else moves to the ASI, becomes optional/instrumented, or is removed.

## Evidence used

The repository contains an exact compressed Original -> V200 byte manifest plus
the later canonical deltas.

Original retail EXE SHA-256:

`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

Original -> V200:
- 5,274 changed bytes
- 211 changed regions
- target SHA-256 `9d13022f1e889e5aeb16e97b6012c300ffb7acfa0b72b90971143fe7447fccaf`

Later exact milestones:
- V255A: `c2d0a5c17f6b342a1f972819c9c71505d5e31da2705dba1c9f8daa0987ace640`
- V257: `ec1cce3970ddb42b5b649144e05426c6e6d38f2079eb6602a302676157220d34`
- V258Y: `032889675706926c60c54ea2ced31cbb6703b5b4ac9872f4da27413cc9993f4e`
- V259: `8f9883883abab91ae029b078326e93751664204e75ebb2f83044ae014d13fc0b`
- V311: `d87283851502782c1a9d566eeacbd33f40a48f10b31d0983237522386826284e`

The final Core must be rebuilt from selected known changes rather than produced
by trying to subtract a handful of patches from V311.

# Classification

## A. Keep in the Core EXE

These are mature user-facing or bootstrap-level fixes. Removing them would
regress functionality that was explicitly validated.

### Window / borderless / display path

Retain the validated native borderless/windowed architecture:

- D3D9 CreateDevice / Reset present-parameter wrappers
- `Windowed = TRUE`
- fullscreen refresh field forced to 0 for the windowed path
- frameless `WS_POPUP` behavior
- desktop-sized game window while preserving the chosen render/backbuffer
  resolution
- validated display-mode restoration / startup behavior
- no topmost behavior

Original -> V200 contains the surviving D3D9 wrappers in the early code caves
around RAW `0x00007B76` and `0x00007BC0`, with call/jump sites including RAW
`0x009F4BB5` and `0x009F4D30`.

The display-management wrapper also survives in V200, including
`EnumDisplaySettingsA` / `ChangeDisplaySettingsA` resolution paths.

This family should remain in the Core because it is both early and
system-integration sensitive.

### DirectInput keyboard / Alt+F4

Keep:

- RAW `0x002859F8`
- `0x05 -> 0x06`
- DirectInput keyboard cooperative mode:
  exclusive/foreground -> nonexclusive/foreground

This is the validated V190 fix that restored normal Windows Alt+F4 behavior.

### High-resolution HUD scaler

Keep the mature HUD scaling architecture.

The active pre-V200 implementation includes the large custom HUD scaler around
RAW `0x0027FB36` / VA `0x00680936` and the separate button-prompt family
around RAW `0x000E6E26`.

Known HUD descriptor families include:
- Tutorial
- Mail
- ObjectiveTray
- Inventory
- RaceHUD
- Pickup
- ButtonPrompt family

This is functional UI work, not an engine-quality experiment.

### HUD toggle

Keep the validated object-level Caps Lock HUD toggle.

Important design property: it does **not** patch the shared SetVisible function
globally. It walks HUD objects and selectively hides/restores them while keeping
required screens usable.

Core hook/cave evidence in V200 includes:
- RAW `0x00285BB4` -> toggle path
- writable toggle/latch state around `0x015F5FF8`
- HUD-object iteration path around RAW `0x002629D0`
- per-object visibility calls through the native vtable

The whitelist includes required menu/screen classes and the later
`WSHUDBladeScreen` correction, which is important because hiding BladeScreen
produced broken/black menu backgrounds in earlier tests.

`WSHUDSabotage` and `WSHUDPerksPopup` are not part of the final protected
screen whitelist and therefore remain hideable with the gameplay HUD.

This whole feature stays in the Core initially. It can be migrated only after an
ASI implementation reproduces the behavior exactly.

### Hor+ FOV / aspect correction

Keep the already validated dynamic Hor+ correction.

The intended behavior remains:
- vertical framing remains native
- horizontal view expands with aspect ratio
- 16:9, 16:10 and ultrawide formats do not receive the old narrow fixed-ratio
  treatment

This is a display-correctness fix and should not be mixed with later LOD work.

### Frame-pacing timer

Keep the proven `timeBeginPeriod(1)` initialization path.

Original -> V200 contains the `winmm.dll` / timer initialization code around
RAW `0x00007C10`, with its call site around RAW `0x00028FC7`.

The separate historical `Sleep(1) -> Sleep(0)` byte is **not automatically
classified as essential**. It should be omitted from the first cleaned Core
unless a focused comparison proves it is still required. The frame-pacing
improvement was specifically tied to the 1 ms timer-resolution path.

### Reticle scaling and native sniper scope

Keep:
- the validated reticle high-resolution scaler
- V255A's exception that leaves the actual `scope` reticle at native 100%

V200 reticle scaler:
- RAW `0x0028F2D6`
- resolution scale values 100 / 95 / 90 / 70
- `_root.Reticles._xscale`
- `_root.Reticles._yscale`

V255A adds the selective scope exception without undoing the general reticle
scaler.

### Final validated HUD placement lineage

Keep the final V258Y HUD work.

Frozen minimap:
- native/orange X = 100
- native/orange Y = 60
- Scaleform/black X = 33.333333333333336
- Scaleform/black Y = 20

Other retained values:
- Tutorial X = 35, Y = 30
- ObjectiveTray local X magnitude = 18
- ObjectiveTray local Y magnitude = 49.666666666666664
- Inventory/ammo local X = +91.66666666666667
- Inventory/ammo local Y = +51.666666666666664

V258G also includes the proven ObjectiveTray correction:
- descriptor flag RAW `0x0027FD78`: 1 -> 0
- removal of the unwanted WSHUDManager X offset in the dedicated path

### V259 world-marker corrections

Keep the validated marker geometry:
- generic world `om_*` scale = 0.70
- generic vertical anchor = 0.25
- `om_shop_AB` dedicated +0.25 Y correction
- `om_HQ_AB` dedicated +0.25 Y correction
- minimap `mm_*` assets remain separate
- garage remains on the validated generic path

This was explicitly validated in game and belongs with the UI Core.

### Streaming FIFO full-queue correctness fix

Keep the V200 full-queue correction.

Unlike a simple capacity increase, this was a control-flow/correctness fix for
the full 64-slot streaming FIFO. It belongs in the Core unless the ASI later
replaces the streaming path completely.

## B. Keep out of the Core, reimplement in the ASI as default-on fixes

### V310 WSModel automatic RenderSlice fix

Validated visible benefit:
- tiny/unlisted WSModel pop was improved

Historical EXE patch:
- VA `0x00639622`
- RAW `0x00238822`
- `D9 47 -> EB 4F`
- preserves full automatic render mask `0x1F`

This is ideal ASI material: signature-find the path, verify original bytes, and
apply the correction at runtime.

### V311 explicit ModelInfo RenderSlice fix

Validated visible benefit:
- garage/workshop pop corrected

Historical EXE patch:
- VA `0x006395AF`
- RAW `0x002387AF`
- `0F B6 4A 02 -> B1 05 90 90`
- existing conversion yields full mask `0x1F`

Also move to the ASI, default-on.

## C. Remove from the Core and expose only through ASI/configuration if useful

These are real engine settings, but they are tuning rather than indispensable
EXE corrections.

### General graphics-quality settings

Do not hard-bake into the new Core:
- shadow-map 4096
- environment-map 2048
- AF16
- CSM 5
- ToneMap 0.15
- foliage distance
- decal distance
- FarScene distance
- DetailObject distance
- ObjectQuality distances
- RenderSlice distance tables

They can become named ASI/config options after ownership and timing are verified.

### Streaming capacity/tuning

Do not hard-bake solely because the larger value was stable:
- Async read count increases
- streaming buffer-size increases
- merged-read/coalescing sizes
- StreamCoverage increases
- later Low/Medium/High coverage expansions

The ASI should instrument queue depth, stalls and actual peak usage first.

### V257 Max Engine scalar increases

Move out of the Core:
- StreamCoverage 8000/1600/1250
- Object LOD 25/50/70/100/150/200/300
- Foliage 250
- shadow-caster 240
- Particle LOD 150
- FarScene 320
- decal visibility 160-equivalent / squared range

These are coherent quality increases, but not correctness requirements.

### V260-V274 capacity/pool expansion lineage

Do not include in Core 1 by default:
- WSDecal 400 -> 800
- WSPhysicsParticle 1000 -> 2000
- Havok TOI 512 -> 1024
- WSParticleRender arena increases
- WSPhGridObject 1000 -> 2000
- sort scratch increases
- WSActivateSphere 256 -> 512
- WSParticleInfoData 1400 -> 2800
- WSParkingSpace 32 -> 64
- WallPoint / WallSegment 50 -> 100
- WSLuaCall 20 -> 40
- WSDamageSphere 512 -> 1024
- WSReadJob / WSUncompressJob 1200 -> 2400
- WSInventoryStateStow 32 -> 64
- PblCRCTreeNode 40000 -> 60000

Even where the limit is technically real, the new standard is actual runtime
pressure. The ASI should log peak occupancy first. A pool is enlarged only when
the game demonstrably approaches/exceeds it.

Some pools may initialize too early for a late runtime resize. If measurement
proves one is required, that specific allocation can later be promoted back into
the Core with evidence.

### V275-V284 distance / coverage chain

Remove from Core:
- V275 SliceQuality High outer 500 -> 1500
- V276 ObjectQuality High human distances
- V277 RenderSlice3 High 100 -> 300
- V278 default ModelInfo LODDIST 1000 -> 1500
- V279 VeryFarSceneTerrain 5000 -> 10000
- V280/V281 WSDetailSystem 100 -> 500 -> 1000
- V282/V283/V284 streaming coverage expansion

These values can be reintroduced individually through the ASI if they produce a
measurable benefit.

### V285-V296 HighPalette chain

Remove completely from the default Core.

The progression 80 -> 160 -> 200 -> 250 -> 300 -> 400 -> 500 -> 650 -> 800 ->
900 -> 1000 -> 1500 -> 1600 produced a long clean binary lineage, but no isolated
user-visible result justifies permanently baking 1600 into the executable.

If needed later, the ASI can expose the one proven comparison as an experimental
diagnostic.

### V298 / V302 large RenderSlice expansion

Remove from Core.

V298 was historically retained even though the user reported no obvious visual
improvement or regression. V302 kept scenery intact but still did not solve the
distant red-material problem.

Under the new cleanup standard, lack of regression is not enough to make a
change essential.

## D. Reject / do not carry forward

Do not inherit any diagnostic-only branch:
- V201-V254 rejected/nonconclusive distance/red-material experiments
- V297 stress branches
- V299-V301 WTF diagnostics
- V303-V309 diagnostics
- V312 VeryFarScene threshold test
- V313 WSFarSceneObject 320 -> 1280
- V314 ModelInfo ZCULL diagnostic
- V315 tuner/Repeat Nodes diagnostic: abandoned before testing; tuner.txt is not
  part of the new architecture

# Proposed clean Core composition

The first clean Core should contain only:

1. validated window/borderless/display startup behavior
2. DirectInput nonexclusive/foreground keyboard fix
3. validated Hor+ aspect/FOV correction
4. `timeBeginPeriod(1)` frame-pacing initialization
5. high-resolution HUD scaler
6. object-level Caps Lock HUD toggle + final safe screen whitelist
7. validated reticle scaler + native 100% sniper scope exception
8. final V258Y HUD/minimap placement
9. V259 world-marker corrections
10. V200 streaming FIFO full-queue correctness fix

Everything else is deliberately excluded from Core 1 unless subsequent binary
dependency analysis proves that one of these retained systems requires it.

# ASI 0.1 starting scope after Core validation

The first ASI should be deliberately small:

- pattern scanner / byte verification
- logger
- V310 WSModel auto-mask fix
- V311 explicit ModelInfo RenderSlice fix
- Odin/instancing tracing for the remaining balcony/facade pop
- pool/queue occupancy telemetry

No broad distance multiplication and no blind pool doubling.

# Validation rule for the rebuilt Core

The cleaned Core is not promoted merely because it launches.

It must reproduce the mature V311 user-facing behavior for:
- startup/window/borderless behavior
- Alt-Tab / Alt+F4
- all supported resolution choices
- Hor+ FOV
- 4K HUD scaling
- Caps Lock HUD toggle across gameplay, pause, loading, shops and menus
- BladeScreen/menu backgrounds
- sniper scope
- minimap dual-layer alignment
- objective/tutorial/inventory-ammo positions
- world shop/HQ/garage markers

Only after that parity test does ASI 0.1 become the active engine branch.
