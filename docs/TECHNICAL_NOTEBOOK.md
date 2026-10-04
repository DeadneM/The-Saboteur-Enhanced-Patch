# Technical notebook

## Current canonical cumulative build

**V298**

Original retail EXE SHA-256:  
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V279 EXE SHA-256:  
`db2ac0f79b2fcace02ac32d77bdea32c5d9591f6bee56715e9e1976f81999b0a`

Every future candidate starts from V298 or reproduces V298 exactly before adding an experiment.

## Frozen minimap

- Native/orange X = `100`
- Native/orange Y = `60`
- Scaleform/black X = `33.333333333333336`
- Scaleform/black Y = `20`

## Retained world-marker state

- world `om_*` scale = 0.70
- generic world vertical anchor = 0.25
- shop/pistol `om_shop_AB` extra Y correction = +0.25
- HQ/Cross `om_HQ_AB` extra Y correction = +0.25

## V261 - WSPhysicsParticle

- pool 1000 -> 2000 at VA `0x009DB5D2`
- matching runtime ceiling 1000 -> 2000 at VA `0x009DB66B`
- object size `0x90` = 144 bytes
- extra contiguous storage: 144,000 bytes (~140.62 KiB)

## V262 - Havok TOI

VA `0x00AC53AD`

`C7 46 64 00 02 00 00` -> `C7 46 64 00 04 00 00`

SizeOfToiEventQueue: 512 -> 1024.

## V263 - WSParticleRender arenas

Allocation sites:

- VA `0x006E7B83`: 4500 x 68 -> 9000 x 68
- VA `0x006E7BAD`: 1000 x 68 -> 2000 x 68
- VA `0x006E7B9C`: 500 x 68 -> 1000 x 68

Matching cap sites include:

- `0x006E3F72`, `0x006E4066`, `0x006E40CD` for the 4500 class
- `0x006E4097` for the 1000 class
- `0x006E40FA`, `0x006E412E`, `0x006E416A` for the 499/500 class

The paired allocation/runtime limits were raised together.

## V264 - WSPhGridObject

- fixed contiguous pool: 1000 -> 2000
- object size: `0x38` = 56 bytes
- capacity fields: `0x0132AF78`, `0x0132AF7C`
- active counter: `0x0132AF80`
- active-cap checks: `0x006CC64C`, `0x006CCB30`, `0x006CCDD2`
- implementation uses a surgical trampoline/code cave; 43 EXE bytes differ from V263
- neighboring WSHKCreationDataContainer remains 1000
- extra contiguous storage: 56,000 bytes (~54.69 KiB)

## V265 - WSParticleRender sort scratch

- Scratch A VA `0x006E7BBE`: `0x8CA0` -> `0x11940` (4500 x 8 -> 9000 x 8)
- Scratch B VA `0x006E7BCF`: `0x1F40` -> `0x3E80` (1000 x 8 -> 2000 x 8)
- +44,000 bytes
- 5 effective EXE bytes changed

## V266 - WSActivateSphere

VA `0x009AE7E6`

`mov eax,0x100` -> `mov eax,0x200`

Fixed pool: 256 -> 512.

## V267 - WSParticleInfoData

- VA `0x009D3173`
- `B8 78 05 00 00` -> `B8 F0 0A 00 00`
- 1400 -> 2800
- object size `0x7C` = 124 bytes
- +173,600 bytes (~169.53 KiB)

## V268 - WSParkingSpace

- VA `0x0090706E`
- `B8 20 00 00 00` -> `B8 40 00 00 00`
- 32 -> 64
- object size `0x54` = 84 bytes
- +2,688 bytes

## V269 - WallPoint / WallSegment

- WallPoint VA `0x009F6B91`: 50 -> 100, object size `0x38`
- WallSegment VA `0x009F6BDE`: 50 -> 100, object size `0x34`
- total extra storage ~5.27 KiB

## V270 - WSLuaCall

- pool object `0x0132B970`
- init VA `0x009F6C74`
- `mov eax,20` -> `mov eax,40`
- object size `0x110` = 272 bytes
- +5,440 bytes

Audit note: the historical Stream Sleep candidate is a no-op here; the modern lineage already contains `Sleep(0)`.

## V271 - WSDamageSphere

- pool `0x0132B700`
- object size `0x9C`
- RAW `0x00B86764`: DWORD 512 -> 1024
- effective byte RAW `0x00B86765: 02 -> 04`
- +79,872 bytes

## V272 - WSReadJob / WSUncompressJob

Shared source:

- VA `0x0162F336`
- RAW `0x00E17D36`
- `BF B0 04 00 00` -> `BF 60 09 00 00`
- 1200/1200 -> 2400/2400
- total extra storage ~98.44 KiB

## V273 - WSInventoryStateStow

- pool `0x0132B908`
- object size `0x24`
- VA `0x00FD0AF4`
- RAW `0x00BCFCF4`
- 32 -> 64
- one effective byte `20 -> 40`
- +1.125 KiB

## V274 - PblCRCTreeNode

- pool `0x01502800`
- object size `0x14`
- VA `0x016055F2`
- RAW `0x00DEDFF2`
- `push 40000` -> `push 60000`
- links use WORD indices with `0xFFFF` sentinel; 60000 deliberately remains below the 16-bit edge
- +400,000 bytes (~390.625 KiB)

## V275 - SliceQuality High outer endpoint

High table base: VA `0x01120AD8`

Final High slice endpoint:

- VA `0x01120B0C`
- RAW `0x00D1F50C`
- 500.0 -> 1500.0

The runtime update loop rewrites only the first four slice records and does not overwrite this final endpoint.

## V276 - ObjectQuality High humans

High-only constants:

- VA `0x004037FC` / RAW `0x000029FC`: 70 -> 100
- VA `0x00403804` / RAW `0x00002A04`: 150 -> 300
- VA `0x0040380C` / RAW `0x00002A0C`: 300 -> 600

Low and Medium remain unchanged.

## V277 - RenderSlice3 High

- VA `0x01120B00`
- RAW `0x00D1F500`
- 100.0 -> 300.0

ModelInfo uses RENDERSLICE3 heavily for urban props. The runtime slice adjustment does not overwrite this upper boundary.

ShadowSlice uses the same slice-distance classes, so SHADOWSLICE3 also benefits from this class extension.

## V278 - ModelInfo default LODDIST

The shared global 1000.0 constant is deliberately not modified.

Only the ModelInfo initializer load is redirected:

- VA `0x00638F2D`
- RAW `0x0023812D`
- from `fld [0x00F7D630]` (1000.0)
- to `fld [0x0040382C]` (native ClipRange High 1500.0)

Explicit `LODDIST25/30` model overrides still replace the default afterward.

## V279 - VeryFarSceneTerrain

The shared global 5000.0 constant is deliberately not modified.

Four VeryFarSceneTerrain-specific `fld` instructions are redirected from native 5000.0 at `0x00FC8A5C` to native 10000.0 at `0x00F7D960`:

- VA `0x008014CB` / RAW `0x004006CB`
- VA `0x008014F7` / RAW `0x004006F7`
- VA `0x00801526` / RAW `0x00400726`
- VA `0x00801A8B` / RAW `0x00400C8B`

VeryFarSceneMonuments and DetailSystem remain untouched.

## Rejected / deliberately untouched patterns

- spill-enabled generic pools where capacity is only a reserve
- WSAICorpse 20, because a separate inline 20-pointer manager array makes a pool-only increase unsafe
- TextureQuality above 3, because quality 3 already maps to a 32768-pixel limit
- a duplicate standalone ShadowSlice distance build after V277
- global shared 1000.0 / 5000.0 constants where a local redirection is possible

## Current open work

1. Audit FarFarScene GeometryDisk radius and whether it is a real coverage limit.
2. Continue VeryFarSceneMonuments / DetailSystem ownership analysis.
3. Keep distant red-prop fallback work separate.
4. Do not alter the frozen minimap.


## V280-V289 modern distance and resource-priority lineage

### V280 / V281 - WSDetailSystem

The real DetailSystem object is 0x240 bytes. Its proven distance field is at +0x218.

V280 redirects the local initial/max/reset sources from 100 to native 500.  
V281 redirects the same three sources from 500 to native 1000.

Sites:
- VA 0x007ECD20 initial value
- VA 0x007ECDC3 maximum comparison
- VA 0x007ECDD0 clamp replacement

Historical V236 fields at +0xC04/+0xC08/+0xC0C are therefore not DetailSystem and remain rejected.

### V282 / V283 / V284 - streaming-grid coverage

Cell sizes remain:
- Low 500
- Medium 60
- High 25

Validated coverage:
- Low 16000
- Medium 3200
- High 2500

Patched table:
- Low VA 0x0104B044
- Medium VA 0x0104B048
- High VA 0x0104B04C

Each tier was increased in a separate validated build.

### V285-V288 - SS_HighPalette threshold

The HighPalette path computes:

    resource[+0x1F8] / resource[+0x1F4]

and performs one threshold compare at VA 0x009EE461 / RAW 0x005ED661.

Validated progression:
- retail/current pre-V285 source 80.0
- V285 160.0
- V286 200.0
- V287 250.0
- V288 300.0

All steps redirect only the local absolute operand to an existing native double. No shared constant is modified.

### V289 - validated

V289A redirects the same HighPalette comparison from native 300.0 at VA 0x00F94648 to native 400.0 at VA 0x00FAA2D0.

Instruction:
- VA 0x009EE461
- RAW 0x005ED661

V288:
    DC 1D 48 46 F9 00

V289A:
    DC 1D D0 A2 FA 00

Effective bytes:
- RAW 0x005ED663: 48 -> D0
- RAW 0x005ED664: 46 -> A2
- RAW 0x005ED665: F9 -> FA

V289A EXE SHA-256:
ad1874548a8c4c6381ad982b1d6ed48fe653d97a43314a518de57963e964774c

V289 was validated in game and is now canonical.

### Still rejected / untouched

- candidate-type-5 threshold 10.0: semantic owner not proven
- VeryFarSceneMonuments inline 256-entry structures: structural, not safe for a pool-only increase
- FarFarScene GeometryDisk: historical effect came from tuner data, not an identified EXE owner
- Water LOD 7.0 / 0.02: developer-tuner values only, native owner not yet proven
- distant red-prop fallback: separate investigation


### V290 - validated

V290A advances only the proven SS_HighPalette local comparison from native 400.0 to native 500.0.

- compare VA `0x009EE461`
- RAW `0x005ED661`
- V289 source `0x00FAA2D0` = 400.0
- V290A source `0x00F97DE0` = 500.0
- instruction `DC 1D D0 A2 FA 00 -> DC 1D E0 7D F9 00`
- effective changes: 3 operand bytes
- no code cave, no injected data, no global constant modification

V290A EXE SHA-256: `f27a94661eb446508ccbbbed6057fd36261ca877f5e54d4d5ccfae0dfb9fa5a8`

V290 was validated in game and is now canonical.


### V291 - validated

V291A advances only the proven SS_HighPalette local comparison from native 500.0 to native 650.0.

- compare VA `0x009EE461`
- RAW `0x005ED661`
- V290 source `0x00F97DE0` = 500.0
- V291A source `0x00FEF000` = 650.0
- instruction `DC 1D E0 7D F9 00 -> DC 1D 00 F0 FE 00`
- effective changes: 3 operand bytes
- no code cave, no injected data, no global constant modification
- no native double 600.0 exists, so 650.0 is the next clean native step

V291A EXE SHA-256: `05555941326fdf66253f166037eb0a51b9aa04aaa45cf1de61721fdb152bc9b2`

V291 was validated in game and is now canonical.


### V292 - validated

V292A advances only the proven SS_HighPalette local comparison from native 650.0 to native 800.0.

- compare VA `0x009EE461`
- RAW `0x005ED661`
- V291 source `0x00FEF000` = 650.0
- V292A source `0x00FD8CF8` = 800.0
- instruction `DC 1D 00 F0 FE 00 -> DC 1D F8 8C FD 00`
- effective changes: 3 operand bytes
- no code cave, no injected data, no global constant modification
- no native double 700.0 or 750.0 exists

V292A EXE SHA-256:
`aad83b503c6a8ae9159a9b960e602c0eade4417bd76d5de82c66c2cc05b5bc28`

V292 was validated in game and is now canonical.


### V293 - validated

V293A advances only the proven SS_HighPalette local comparison from native 800.0 to native 900.0.

- compare VA `0x009EE461`
- RAW `0x005ED661`
- V292 source `0x00FD8CF8` = 800.0
- V293A source `0x00F82660` = 900.0
- instruction `DC 1D F8 8C FD 00 -> DC 1D 60 26 F8 00`
- effective changes: 3 operand bytes
- no code cave, no injected data, no global constant modification
- 900.0 occurs once as a native double in the executable

V293A EXE SHA-256:
`e8f4177caab596f2aaca8a0c7da1bc43f1601bbc15efab325045974b8aabbb92`

V293 was validated in game and is now canonical.


### V294A candidate

V294A advances only the proven SS_HighPalette local comparison from native 900.0 to the unique native 1000.0.

- compare VA `0x009EE461`
- RAW `0x005ED661`
- V293 source `0x00F82660` = 900.0
- V294A source `0x010A45B8` = 1000.0
- 1000.0 RAW location `0x00CA37B8`
- instruction `DC 1D 60 26 F8 00 -> DC 1D B8 45 0A 01`
- effective changes: 4 operand bytes
- PE section mapping verified explicitly
- no code cave, no injected data, no global constant modification

V294A EXE SHA-256:
`888cae2157ff38570f56e8eb6ee974cc45892b2cc84f78f3cf3c465ee4c741f8`

V294A remains test-only until explicit in-game validation.


### V295 - validated

V295A advances only the proven SS_HighPalette local comparison from native 1000.0 to native 1500.0.

- compare VA `0x009EE461`
- RAW `0x005ED661`
- V294 source `0x010A45B8` = 1000.0
- V295A source `0x010207E0` = 1500.0, stored in `.rdata`
- exactly 3 operand bytes change
- no code cave, no injected data, no global constant modification

A second native double 1500.0 exists at VA `0x00403818` inside `.text`; V295A deliberately does not reference that copy.

V295A EXE SHA-256:
`5ed95c8d052e130b0593c28b1f85e306f846784bd012a7614cb586dddcb918a9`

V295 was validated in game and is now canonical.


### V296 - validated

V296A advances only the proven SS_HighPalette local comparison from native 1500.0 to native 1600.0.

- compare VA `0x009EE461`
- RAW `0x005ED661`
- V295 source `0x010207E0` = 1500.0
- V296A source `0x010140A8` = 1600.0
- source RAW `0x00C132A8`
- instruction `DC 1D E0 07 02 01 -> DC 1D A8 40 01 01`
- effective changes: 3 operand bytes
- no code cave, no injected data, no global constant modification
- relative threshold increase: +6.67%

V296A EXE SHA-256:
`2c40dfe0953b300d2da65ea6cdf1f7d02df50cb013dabe7101a51199b4208b5f`

V296 was validated in game and is now canonical.


### V297A EXTREME candidate

After V296, the test strategy changes: stop using small increments and push the proven HighPalette threshold to an extreme native value first, then reduce only if the engine shows instability or unacceptable memory/frame-time cost.

- compare VA `0x009EE461`
- RAW `0x005ED661`
- V296 source `0x010140A8` = 1600.0
- V297A source `0x00F86DE0` = 1,000,000,000.0
- native 1e9 RAW `0x00B85FE0`
- instruction `DC 1D A8 40 01 01 -> DC 1D E0 6D F8 00`
- effective changes: 4 operand bytes
- no branch forcing
- no code cave
- no injected data
- no global constant modification

V297A EXE SHA-256:
`990cb7d7fad5b78ed272675f0a50c522c7509c1ab03b496f7fc2014a746ab94e`

Fallback ladder if the extreme test fails:
1. 100000
2. 10000
3. 5000
4. 2000


### V296 - validated

HighPalette local threshold 1500 -> 1600 using the unique native double at VA `0x010140A8`.

### V297A x4 final HighPalette test

User-directed strategy change: stop micro-incrementing this threshold. Use one meaningful multiplier and then move on.

- threshold: 1600 -> 6400 (x4)
- compare VA `0x009EE461`
- RAW `0x005ED661`
- V296 source `0x010140A8`
- V297A source `0x01011D00`
- exactly 2 effective EXE bytes change
- 6400.0 is unique as a native double
- 16000.0 is absent as a native double

The experimental 1e9 idea is explicitly rejected as excessive and must not be restored.

## V298 - retained all-RenderSlice High expansion

User symptom: substantial world-prop pop-in around roughly 50 units/metres in-game.

Audit correction:

ModelInfo RENDERSLICE n is converted to a bit mask with:

    mask = (1 << n) - 1

Therefore RENDERSLICE3 activates slices 0+1+2 and is bounded by the class-2 far edge. The earlier V277 description that equated record 3 directly with RENDERSLICE3 was conceptually incomplete.

High SliceQuality table base:
- VA 0x01120AD8
- RAW 0x00D1F4D8

V298 modifies the effective High chain:
- record1 start 4 -> 16 at VA 0x01120AE4 / RAW 0x00D1F4E4
- record2 start 20 -> 80 at VA 0x01120AF0 / RAW 0x00D1F4F0
- record3 start 50 -> 200 at VA 0x01120AFC / RAW 0x00D1F4FC
- record3 far 300 -> 1200 at VA 0x01120B00 / RAW 0x00D1F500
- record4 far 1500 -> 6000 at VA 0x01120B0C / RAW 0x00D1F50C

Only five effective EXE bytes change from V296.

V298 EXE SHA-256:
`57878890c4b9ba3bb9705109b4b216623d226b28598b3a187cf4b2795574a09a`

User test result: no obvious improvement or regression was observed. The user explicitly requested that all these Slice increases remain in every following build, so V298 is retained as the new cumulative base.

## Red-material investigation - Will to Fight / WTF

User observation: occupied Nazi zones intentionally render in black/white/red, and some normally grey metallic materials become red under that artistic state. This strongly links the distant red-material bug to the native Will to Fight rendering pipeline rather than to a simple missing texture.

Static EXE evidence includes WTF-specific shader/material symbols such as:
- WTF Filters / WTF Zones
- WTFInfluenceGridTexture%d
- g_vWTFGreyscale / g_vWTFGreyscale2
- g_vWTFAmbientHigh / Low
- g_vWTFDiffuseHigh / Low
- vWTFColor
- fWTFIntensity / smpWTFIntensity
- WSWillToFightFilter.hlsl
- WSWillToFightZone.hlsl

RTTI identifies the low-resolution influence class as WSWillToFightGrid. Its object allocation is 0xB0 bytes and its constructor is around VA 0x009768C0.

Important correction: LowResWorldWTF and LowResWorldWTFVertex are render-target / texture resources, not shader names.

The PC-default selector at global byte 0x012100F4 is zero-initialized, causing consumers to select the LowResWorldWTF resource path by default.

WSWillToFightGrid maintains:
- a 256x256 8-bit CPU influence buffer
- LowResWorldWTF 256x256
- LowResWorldWTFVertex 256x256
- bilinear GetInfluence sampling normalized by 255

No separate structural 256 cap was found. Other 0xFF values in the class are intensity saturation and must remain 255.

## V299A - current WTF low-resolution 1024 test

Built directly from V298, preserving all RenderSlice changes.

1. CPU/grid local dimension source:
- VA 0x0097690C / RAW 0x00575B0C
- D9 05 48 77 02 01 -> D9 05 34 33 FF 00
- source 256.0 at VA 0x01027748 -> native 1024.0 at VA 0x00FF3334

2. LowResWorldWTF:
- width RAW 0x0057523B: push 0x100 -> push 0x400
- height RAW 0x00575240: push 0x100 -> push 0x400

3. LowResWorldWTFVertex:
- width RAW 0x00575285: push 0x100 -> push 0x400
- height RAW 0x0057528A: push 0x100 -> push 0x400

Effective EXE diff versus V298: 8 bytes.

CPU influence buffer grows from 65,536 bytes to 1,048,576 bytes (+983,040 bytes). The two GPU render targets each grow 16x in pixel count; native format is unchanged.

No WTF color, greyscale, ambient/diffuse, zone distance or 0..255 intensity value is modified.

V299A EXE SHA-256:
`46ddd1d3b146166b0220d7f0337db3c726500988863d4eb5d13554d3ee4a1abf`

V299A remains test-only pending in-game comparison of the exact same occupied-zone grey-metal surfaces.


## V302 retained baseline and post-V302 diagnostics

### V302 retained

Base: V298.

Rejected V299-V301 diagnostics are excluded.

High SliceQuality fields:
- RAW 0x00D1F4E4: 16 -> 24.9765625
- RAW 0x00D1F4F0: 80 -> 124.8828125
- RAW 0x00D1F4FC: 200 -> 312.20703125
- RAW 0x00D1F500: 1200 -> 1873.2421875
- RAW 0x00D1F508: 80 -> 124.8828125
- RAW 0x00D1F50C: 6000 -> 9366.2109375

V302 EXE SHA-256:
`db96ff3f6f8e38acdd262a87b0bb0ded7c604abca82e86e2501a12347f512459`

User validation:
- scenery remains correct
- distant red issue remains visible

### Post-V302 SliceQuality diagnostics

V303-V307 proved that moving Q2 Slice4.first into the ~124.88-125 region can hide the red symptom but breaks scenery. Scaling the entire Q2 table by the same factor also breaks scenery, so the problem is not merely a discontinuity between neighboring Q2 records.

V308 changed only Q2 record2 far (RENDERSLICE3 terminal far) from 50 to 200. The user's short-range object pop remained unchanged. Therefore the observed ~3 m pop in the supplied garage/workshop capture is not explained by that Q2 RENDERSLICE3 far bound.

All V303-V308 branches are rejected. Future binary work starts from V302.


## Complete near-object display-distance audit after V308

V308 changed active-Q2 record2 far from 50 to 200 and produced no visible movement of the short-range prop pop shown in the supplied garage/workshop capture. V308 is rejected.

The follow-up static audit found a separate per-model hard-cull layer in WSModel.

### WSModel automatic size policy

`WSModel+0x58` is used as a size/radius-like metric by the model classifier and by proximity-sphere callers.

Automatic render-mask thresholds:
- <0.3 -> 0x03
- <0.6 -> 0x07
- <4.0 -> 0x0F
- >=4.0 -> 0x1F

Independent camera-depth fields:
- constructor A8 = 10000
- constructor AC = 10000
- for metric <1.5: A8 = 20 + 60*metric
- for metric <5.0: AC = 15 + 20*metric

Runtime function VA `0x00638710` compares camera-forward depth against A8 and sets the model render mask to zero when exceeded. AC controls a secondary low-nibble/shadow mask cutoff.

This means a tiny model can be killed by a per-model hard distance before enlarged SliceQuality, ModelInfo or streaming ranges become relevant.

Candidate one-byte A/B, not yet built:
- VA `0x0063954E`
- RAW `0x0023874E`
- `7A 1A -> EB 1A`
- effect: skip only the small-model A8 rewrite and retain default A8=10000
- AC/shadow formula remains native

### WSSphereActivator

Separate from V266 pool capacity, the real activation sphere creation at VA `0x0068EF80` clamps:
`effective_radius = min(requested_radius*1.05, 2.06)`

This is a genuine ~2-unit engine limit. Current callers and lifetime behavior make it look like a transient spatial activation/query system, not yet a proven static-prop renderer owner. Do not globally raise it without a focused diagnostic because it may affect gameplay/physics/AI/triggers.

### Current distance-limit priority

Already pushed / unlikely to explain the ~3m pop:
- SliceQuality global tables
- Q2 RENDERSLICE3 far
- ObjectQuality human distances
- ModelInfo default LODDIST
- VeryFarSceneTerrain
- WSDetailSystem
- streaming coverage
- HighPalette resource priority

Still relevant:
1. WSModel A8 size-derived hard visibility cutoff — strongest candidate
2. object-family-specific activation / WSSphereActivator — secondary
3. global occlusion/indoor culling — secondary and intrusive to disable
4. automatic size-to-RenderSlice classifier for models with no ModelInfo entry
5. VeryFarSceneMonuments / FarFarScene for genuinely distant world objects, not the present indoor few-metre symptom


## V310 validated - unlisted WSModel full masks

Base: retained V302.

V309A first tested the WSModel +0xA8 hard-cull rewrite bypass:
- VA 0x0063954E / RAW 0x0023874E
- `7A 1A -> EB 1A`
- user result: no visible change
- V309 rejected

V310A then targeted the automatic size-to-RenderSlice classifier used only when
there is no explicit ModelInfo entry:
- VA 0x00639622 / RAW 0x00238822
- `D9 47 -> EB 4F`
- jump destination: existing mask pack/store at VA 0x00639673
- preserves preinitialized ESI/EBP/EBX = 0x1F

User result: the short-range small-object pop in the garage/workshop scene was
visibly improved.

Conclusion:
- the auto size classifier was a real PC draw-distance limiter.
- V310 is retained.
- next logical target is the explicit ModelInfo RenderSlice path, while leaving
  explicit ZPassSlice, ShadowSlice and LODDIST unchanged.


## V311 validated - explicit ModelInfo full RenderSlice

Base: validated V310.

V311 forces only the explicit ModelInfo RenderSlice load to 5:
- VA 0x006395AF / RAW 0x002387AF
- `0F B6 4A 02 -> B1 05 90 90`
- resulting render mask 0x1F

User result:
- garage/workshop short-range object pop corrected.

This confirms both sides of the WSModel RenderSlice policy were real draw-distance
limiters:
1. V310 fixes unlisted/auto-classified models.
2. V311 fixes explicit ModelInfo RenderSlice-limited models.

A later street video still shows a balcony/facade component appearing late.
That object family is therefore outside these two WSModel RenderSlice gates.

VeryFarScene audit found two internal profile thresholds stored in each 24-byte
profile record at +0x08:
- profile 0: 22.0
- profile 1: 49.0

Runtime method VA 0x00801700 computes camera-forward depth and only enters the
VeryFarScene processing path once depth reaches approximately:
- 22 - 5 = 17
- 49 - 5 = 44

These values are a strong match for the remaining architectural transition
distance seen in the supplied video.


## V312 rejected / WSFarSceneObject audit

V312A changed VeryFarScene profile thresholds 22/49 -> 88/196.
User result: no visible change to the balcony/facade pop.
V312 is rejected; V311 remains canonical.

RTTI/vtable audit then identified WSFarSceneObject's own render path around
VA 0x0048A2E0.

Helper VA 0x0048A1C0 computes camera-forward depth for the object.
The render path compares this depth against two class-local 320.0 thresholds:
- VA 0x0048A355 reads 0x01114DD8 = 320.0
- VA 0x0048A367 reads 0x01114DD4 = 320.0

When depth exceeds the first threshold, the low five render bits (0x1F) are
cleared from the object's own mask. The second threshold controls bit 0x200.

The two threshold globals have no other executable references.

V313A redirects only these two local loads to native read-only 1280.0 at
VA 0x00FF333C, leaving the original globals untouched.


## V314 rejected - ModelInfo ZCULL

V314A starts directly from V311 and tests the explicit ModelInfo ZCULL branch.
The architectural ModelInfo corpus contains many ZCULL-tagged ornaments,
especially `Ornate_Clamber_*` and `Ornate_WinDoor_*`, making this a plausible
balcony/facade owner.

User result:
- the balcony pop is unchanged.

Conclusion:
- explicit ModelInfo ZCULL is not the controlling gate for this symptom.
- V314A is rejected.
- V311 remains the retained cumulative base.

### Repeat Nodes / Odin branch opened

The developer tuner exposes:
- `Console.gfx.genericobjects=on`
- `Console.gfx.repeatnodes=on`

RTTI/vtable recovery proves a separate Odin instancing layer:
- `OdinBaseInstancedMesh`
- `OdinInstancedMesh`
- `OdinMeshInstance`
- `OdinMeshInstance::RemoveHighResSegments` at VA `0x00E14BA0`
- `OdinMeshInstance::ReInstance` at VA `0x00E14BF0`

This is now the strongest architectural explanation for a facade element whose
parent building is already present but whose high-detail geometry appears later.

Next diagnostic rule:
- do not change WSModel RenderSlice, VeryFarScene thresholds,
  WSFarSceneObject 320 cutoffs, or ModelInfo ZCULL again for this balcony.
- first prove Repeat Nodes ownership, then locate the instancing/high-res segment
  distance or retention gate and patch only that local owner.


## V315A diagnostic - Repeat Nodes ownership

No EXE bytes are changed. The validated V311 executable remains the base.

Single override:
```
Console.gfx.repeatnodes=off
```

Why this test is narrow:
- the developer tuner exposes Repeat Nodes separately from generic objects;
- RTTI/vtable recovery identifies a distinct Odin instancing layer;
- `OdinMeshInstance::RemoveHighResSegments` at VA `0x00E14BA0` is a strong
  candidate for high-detail geometry retention, but touching it before proving
  Repeat Nodes ownership would be premature.

Artifact SHA-256:
`8fc0888d9c91e193f66d183798f592d178974b50ea22d8efdefec41f65fcdf9d`

Decision gate:
- visible balcony/facade change => audit OdinInstancedMesh / high-res segment path;
- no change => reject Repeat Nodes ownership for this object.


## Core EXE reconstruction reset

The V311 cumulative executable remains the historical research reference but is
no longer the intended future binary base.

Reason:
- the cumulative lineage mixes proven user-facing fixes with many stable but
  weakly justified engine-capacity and draw-distance increases;
- lack of crashes is no longer sufficient evidence for permanent retention.

New retention audit:
- `docs/CORE_EXE_AUDIT.md`

Hard-retain Core families include validated borderless/display behavior,
DirectInput 0x05->0x06, Hor+ FOV, timeBeginPeriod(1), HUD scaling, object-level
Caps Lock HUD toggle with BladeScreen-safe menu behavior, reticle/scope, final
V258Y HUD/minimap placement, V259 world markers, and the V200 streaming
full-queue correctness fix.

V310/V311 have demonstrated visual value but are planned for ASI runtime
reimplementation.

Pool/capacity inflation, HighPalette progression, broad RenderSlice expansion
and other scalar tuning are removed from Core by default and must earn their way
back through telemetry or isolated visible benefit.

The V315 tuner.txt diagnostic is abandoned and is not part of the new
architecture.


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


## Retail EXE re-audit checkpoint — October 2026

A clean second-pass audit is now being performed against the exact retail
14,834,176-byte executable, SHA-256
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`.

This audit supersedes assumptions made solely from the old cumulative V200+
lineage.

Critical corrections already established:

- the ASI key formerly named `CSMQuality` targets an embedded AO
  `PsDepthConv` shader instruction and is not a CSM quality selector;
- historical full-resolution AO is a coherent multi-site feature and the
  current ASI grouping is incomplete;
- the 24 historical shadow-PCF constants are texel/sample-offset compensation
  for 1024 -> 4096 shadow maps, while the actual 3x3 -> 5x5 shader selection is
  a separate pair of selector changes;
- ToneMap 0.25 -> 0.15 directly reduces the recovered shader RGB contribution
  and is no longer considered a quality improvement;
- historical `PsBloomFinal` changes are artistic composition changes and must
  not be mixed into AO;
- the so-called RenderSlice High table participates in the
  SliceQuality/ShadowSlice/CSM path and must not be treated as a generic object
  draw-distance table;
- retail streaming coalescing threshold is 512,000 bytes, while V200 set it
  directly to 128 MiB;
- old Async32 is a real multi-submit scheduler/hook, not a simple 16 -> 32
  scalar patch;
- retail Havok values are TOI queue 250 and broad-phase query size 1024.

The current later ASI research line is therefore WIP. **Core 1 + ASI 0.1**
remains the validated architecture baseline until a corrected candidate is
built and tested.

Full authoritative restart plan:
`docs/RETAIL_EXE_REAUDIT_CHECKPOINT.md`.


## ASI 0.11 clean visual baseline

Status: **VALIDATED — canonical ASI baseline**

Final candidate source/docs head:
`b61ddf1f8a05f14c1d2433dd945bbcc98f2b6040`

CI:
- workflow run: 104
- run ID: `36879200872`
- artifact ID: `11171260493`
- artifact: `SaboteurEnhanced_ASI_0.11_CLEAN_VISUAL_BASE_x86`
- artifact ZIP SHA-256:
  `4543cb1970491aebfaec33fec191a056d451dd208f920184f17ea62adcb2cec0`
- `SaboteurEnhanced.asi` SHA-256:
  `7dc77a36797c97d18bfee01ba771d35930b880eeb7fb822199119d3d553c35cb`
- `dinput8.dll` SHA-256:
  `209a99922ef087525faf9ee13319469ae1107a94de81f188db7c6cb4295da189`
- PE machine: x86 / 0x14C for both binaries

Key cleanup commits:
- `5850b4e`: remove invalid CSMQuality runtime path and restore safe fallbacks
- `69f8f02`: rebuild full-resolution AO from exact retail owners
- `5f116dc`: separate shadow-map texel scaling from true PCF 5x5 selection
- `0e9c7d2`: reset audited visual fallbacks to retail-safe values
- `f8d5ae1`: ship clean visual-baseline INI
- `568dab3`: label CI artifact as 0.11
- `b61ddf1`: document 0.11 test target

0.11 test baseline intentionally keeps:
- EnvironmentMapResolution 2048
- AF16
- ShadowMapResolution 4096 with derived PCF texel offsets
- V310/V311
- validated non-shadow distance/LOD work

It returns the audited visual suspects to retail:
- MIP bias 0
- ToneMap 0.25
- PCF selector 3x3
- CSM lambda 0.5
- CSM far ~100
- shadow bias scales 1/1
- SpotShadow scale 0.5
- AO half-resolution and blur/erode 2/2
- ShadowSlice High bounds 100/500
- invalid CSMQuality path removed entirely

User validation result (2026-10-01): **"tout marche bien"**.

The 0.11 clean visual baseline is therefore promoted to the canonical ASI
baseline. The exact tested artifact and hashes above are frozen as the
reference build.

Future graphics/engine improvements must start from this 0.11 architecture and
be reintroduced by proven owner/family. Do not restore the invalid 0.8
CSMQuality/AO/PCF assumptions.


## ASI 0.12 true PCF5x5 + full-resolution AO

Status: **VALIDATED — canonical ASI baseline**

Source/config commit:
`afc0f62ae1d35c14d436d99cd23d070138b83fc1`

No C++ owner/hook change from validated 0.11. 0.12 only enables:

- `Shadows.ShadowPCF5x5=1`
- `AmbientOcclusion.FullResolution=1`

CI:
- workflow run: 105
- run ID: `36885624563`
- artifact ID: `11174301634`
- artifact:
  `SaboteurEnhanced_ASI_0.12_TRUE_PCF5X5_FULLRES_AO_TEST_x86`
- artifact ZIP SHA-256:
  `f0dc4a5ece02905b74b859e9ba9d5a58590f88447c357f483ac2ad490b98205f`
- `SaboteurEnhanced.asi` SHA-256:
  `d046158ec6c2bff1bdad7659bf2f2dc30902e5091bb8076f954ae9e3e5da873f`
- `dinput8.dll` SHA-256:
  `2f8b4fdb73476ce4eafdfd301c8bf12af41cd60293ccb01cfe62e739b165e0e3`
- both PE machine: x86 / 0x14C

A/B fallback using the shipped INI:
- PCF only: `FullResolution=0`
- AO only: `ShadowPCF5x5=0`
- exact validated 0.11 visual profile: both = 0

User validation result (2026-10-01): **validated**.

ASI 0.12 is therefore promoted to the canonical ASI baseline. The exact tested
artifact and hashes above are frozen as the reference build.

Future graphics/engine improvements must start from the 0.12 architecture:
verified PCF 5x5 enabled, coherent full-resolution AO enabled, and all other
0.11 cleanup/isolation decisions preserved. 0.11 remains the immediate
rollback/reference profile, but is no longer the current canonical build.


## ASI 0.13 full-resolution PostFX candidate

Status: **TEST CANDIDATE; 0.12 remains canonical until user validation**

Base:
- Core 1 validated EXE
- ASI 0.12 canonical quality baseline
- no C++ hook/owner changes in 0.13

0.13 enables one coherent screen-space render-target quality pass through
already audited owners:

- MotionBlurDownsampledBackBuffer: half -> full resolution
- Bloom/GodRays pyramid multiplier: 1x -> 2x
- ScaledTexture source: half -> full resolution
- DepthBlurMask2x2/Temp scale: 0.5 -> 1.0
- DepthBlurColor pyramid factor: 0.75 -> 0.5
- DamageBlur target scale: 0.5 -> 1.0
- LightVolumeRT multiplier: 1x -> 2x
- WSParticleRender RT multiplier: 1x -> 2x

Exact INI deltas from 0.12:
- `MotionBlurFullResolution 0 -> 1`
- `BloomResolutionMultiplier 1 -> 2`
- `ScaledTextureFullResolution 0 -> 1`
- `DepthBlurMaskResolutionScale 0.5 -> 1.0`
- `DepthBlurColorPyramidFactor 0.75 -> 0.5`
- `DamageBlurResolutionScale 0.5 -> 1.0`
- `LightVolumeResolutionMultiplier 1 -> 2`
- `Particles.RenderTargetResolutionMultiplier 1 -> 2`

Frozen from canonical 0.12:
- PCF 5x5 ON
- full-resolution AO ON
- ShadowMapResolution 4096 with coherent 24-offset scaling
- ToneMap 0.25
- CSM lambda 0.50 / far ~100
- depth/slope bias 1.0 / 1.0
- SpotShadowResolutionScale 0.5
- all validated distance/LOD fixes
- no tuner.txt modification

Water, sky, rain and WTF render-target families remain native for this build.
They are reserved for a later world-render-target quality pass so 0.13 remains
diagnostically useful.

Source/config commits:
- `1277b0aab879b5557df50bb5c9e1bef5196ee386`: 0.13 INI profile
- `aa7cbc861e9e24a1d7401178576411de799bf5bd`: 0.13 CI artifact label
- `8c4d1cc7aa17ce14f911bbd1744cefae1293cdc4`: 0.13 README/test plan

Validation target:
1. no brightness/contrast regression against 0.12;
2. cleaner post-FX edges;
3. no broken compositing, halos or screen-space banding;
4. no particle/light-volume regression;
5. evaluate GPU cost separately from correctness.

CI:
- workflow run: 109
- run ID: `36889918093`
- artifact ID: `11176261686`
- artifact: `SaboteurEnhanced_ASI_0.13_FULLRES_POSTFX_TEST_x86`
- artifact ZIP SHA-256:
  `5405b9906498e35f0148ddfeaeb8c3c31442c639f1bd2e388e551599c5c8af1c`
- `SaboteurEnhanced.asi` SHA-256:
  `fd1ccf7e8ecfb4679948025e76e51ca44375d816936cba841243fff412fc6e39`
- `dinput8.dll` SHA-256:
  `b18fa91a73736a86000b95ac43d1f6a1f42872dc432b60c2e819bcbd4c2ab018`
- both binaries verified PE machine x86 / 0x14C

Do not promote 0.13 until user validation.


## ASI 0.13A Bloom/GodRays isolation candidate

Status: **TEST CANDIDATE; 0.12 remains canonical**

User result for 0.13:
- overall build is stable and visually good;
- one visible artifact remains around very bright areas during camera movement;
- artifact appears as white/blue fragmented highlights around the bright-window/GodRays region in the supplied video.

Video review points away from shadows/AO and toward the bright-pass family.
The first surgical isolation is therefore:

- `ExperimentalPostFX.BloomResolutionMultiplier 2 -> 1`

Everything else from 0.13 remains enabled:
- MotionBlurFullResolution=1
- ScaledTextureFullResolution=1
- DepthBlurMaskResolutionScale=1.0
- DepthBlurColorPyramidFactor=0.5
- DamageBlurResolutionScale=1.0
- LightVolumeResolutionMultiplier=2
- Particles.RenderTargetResolutionMultiplier=2

0.13A goal:
- if the artifact disappears, Bloom/GodRays 2x is rejected while the other
  0.13 render-target improvements can continue toward validation;
- if the artifact remains, restore Bloom 2x and isolate the next highest-risk
  bright-pass owner, starting with ScaledTexture/MotionBlur rather than touching
  validated 0.12 shadows or AO.

Source/config:
- `ca0c2cb3067fee785e547d7882c23f16ceb3b1e1`
- CI label commit:
  `27d7e66b592bd420c88bc72a27ba247b660a25da`

CI:
- workflow run: 111
- run ID: `36891879140`
- artifact ID: `11176449035`
- artifact: `SaboteurEnhanced_ASI_0.13A_BLOOM_GODRAYS_ISOLATION_TEST_x86`
- artifact ZIP SHA-256:
  `4a8f2541807b1e5efbd5063acba9f9fc40c557d76f4ae71a967dd61e6839da41`
- `SaboteurEnhanced.asi` SHA-256:
  `464386533a9b7ca1df9c233f3bde7f6867d6b8fc245064f97dd29219904a0b9f`
- `dinput8.dll` SHA-256:
  `0d09e614a46e3ef649bf219702b5f1eb48e02caf2ecb874c8890b80bcdbf23bc`
- both binaries verified PE machine x86 / 0x14C


## ASI 0.13A result and 0.13B ScaledTexture isolation

0.13A result:
- Bloom/GodRays pyramid was restored from 2x to the retail 1x layout.
- User screenshot still shows the same bright fragmented artifact above the
  window/light source.
- Conclusion: BloomResolutionMultiplier=2 is not the primary owner of the bug.
  Keep it native during isolation, but do not attribute the artifact to it.

0.13B changes exactly one additional setting from 0.13A:
- `ExperimentalPostFX.ScaledTextureFullResolution 1 -> 0`

Rationale:
- retail ScaledTexture is a half-resolution screen-space source;
- several downstream post-FX paths consume it with retail sampling/layout
  assumptions;
- changing only its dimensions without changing every downstream coordinate
  transform is a plausible source of the observed white fragmented mask.

Everything else from 0.13A stays enabled:
- MotionBlurFullResolution=1
- DepthBlurMaskResolutionScale=1.0
- DepthBlurColorPyramidFactor=0.5
- DamageBlurResolutionScale=1.0
- LightVolumeResolutionMultiplier=2
- Particles.RenderTargetResolutionMultiplier=2

Frozen canonical 0.12 features remain untouched:
- PCF 5x5 ON
- coherent full-resolution AO ON
- ShadowMapResolution 4096 with derived texel offsets
- ToneMap 0.25
- native CSM/bias profile from the 0.11 cleanup

Source/config:
- `ee6e24676bac5bcc304b79363822d1fb8317e3f4`
- CI label commit:
  `b4abe2eadd1794fec3538690dfcefcb5aa247926`

0.12 remains canonical until a later 0.13-family candidate is clean and
explicitly validated by the user.

CI:
- workflow run: 113
- run ID: `36892487481`
- artifact ID: `11176937976`
- artifact: `SaboteurEnhanced_ASI_0.13B_SCALED_TEXTURE_ISOLATION_TEST_x86`
- artifact ZIP SHA-256:
  `ef59bdaaaf4f284121c89d59d7b1e1c1492d7a530d760338f01a1837f647f6c6`
- `SaboteurEnhanced.asi` SHA-256:
  `f48235cee105c98855295668f56a0c40f51790273013cd4b40f04ea9fd47fb9a`
- `dinput8.dll` SHA-256:
  `3c626c01b6b107a7c4fd33c879864659440f56077ea4f83b732d3c8c09668d90`
- both binaries verified PE machine x86 / 0x14C.


## ASI 0.13B result -> ASI 0.14 CorrectUV / texel compensation

0.13B result:
- Bloom/GodRays had already been returned to native in 0.13A;
- ScaledTexture was additionally returned to native half-resolution in 0.13B;
- user result: **the same bright fragmented artifact remains**.

Conclusion:
- neither BloomResolutionMultiplier=2 nor ScaledTextureFullResolution=1 is
  sufficient on its own to explain the artifact;
- do not continue solving the problem by deleting quality increases;
- follow the historical full-resolution AO lesson instead: keep the higher
  render-target resolution and repair shader-side sampling assumptions.

User direction:
- keep all 0.13 quality improvements;
- expose additional independently configurable INI controls.

### 0.14 static shader audit

The complete historical V200 executable was re-opened only as a static shader
reference. Its SHA-256 is:
`9d13022f1e889e5aeb16e97b6012c300ffb7acfa0b72b90971143fe7447fccaf`.

The embedded DepthBlur shaders around the exact retail/V200 shader layout
contain two independently recoverable spatial-sampling families.

#### DepthBlur mask tap offsets

Two verified mask shaders use `g_uvScale` plus literal spatial offsets.

Shader family A retail offsets:
- RVA `0x00D66AE0`: 7.5
- RVAs `0x00D66AF8/FC/B00/B04`: 2 / 4 / 6 / 8

Shader family B retail offsets:
- RVAs `0x00D66DB8/DBC`: 6 / 7.5
- RVAs `0x00D66DD0/D4/D8/DC`: 2 / 4 / 5 / 8

The separate 1/9 normalization constants are deliberately not modified.

New INI:
`ExperimentalPostFX.DepthBlurMaskTapOffsetScale`

- native = 1.0
- 0.14 test = 0.5
- reason: DepthBlurMaskResolutionScale changes 0.5 -> 1.0, exactly doubling
  the target resolution per dimension; multiplying spatial tap offsets by 0.5
  preserves their retail screen-space radius.

#### DepthBlur color texel offsets

Two verified color shaders multiply `g_vTexelSize` by literal -1/+1 offsets:

- RVAs `0x00D664A0/0x00D664A4`: -1 / +1
- RVAs `0x00D666B0/0x00D666B4`: -1 / +1

The adjacent 0.2 threshold is deliberately left untouched.

New INI:
`ExperimentalPostFX.DepthBlurColorTexelOffsetScale`

- native = 1.0
- 0.14 test = 0.6666667
- reason: DepthBlurColorPyramidFactor changes 0.75 -> 0.5. The retail
  divisors 1.5/3/6/12 become 1/2/4/8, making every level 1.5x larger per
  dimension. The inverse ratio 2/3 preserves the retail screen-space texel
  offset.

Both patch groups verify every exact retail float before writing anything.

### 0.14 quality profile

Unlike 0.13A/B, 0.14 restores the complete original 0.13 render-target pass:

- MotionBlurFullResolution=1
- BloomResolutionMultiplier=2
- ScaledTextureFullResolution=1
- DepthBlurMaskResolutionScale=1.0
- DepthBlurMaskTapOffsetScale=0.5
- DepthBlurColorPyramidFactor=0.5
- DepthBlurColorTexelOffsetScale=0.6666667
- DamageBlurResolutionScale=1.0
- LightVolumeResolutionMultiplier=2
- Particles.RenderTargetResolutionMultiplier=2

Canonical 0.12 graphics remain untouched beneath this experimental layer:
PCF5x5, coherent full-resolution AO, ShadowMap 4096, retail ToneMap/CSM/bias
cleanup, and validated distance/LOD fixes.

Source commits:
- `d4a676b9857998b64ec39250be08f1a4a54d4f29` — new shader compensation code
- `e7ceba450c4354c53ae105c0dadd0bda5d32f0ec` — 0.14 INI profile
- `afb895d707466bf50ba8d447c431b32d2617fd5b` — CI artifact label
- `463af53fbfddbf002b366310439d37589494f154` — README/test plan

0.12 remains canonical until the user validates a clean 0.14-family candidate.

CI:
- workflow run: 116
- run ID: `36898159642`
- artifact ID: `11181405260`
- artifact: `SaboteurEnhanced_ASI_0.14_CORRECT_UV_TEXEL_COMPENSATION_TEST_x86`
- artifact ZIP SHA-256:
  `c4313cda68079cfcdf5341bfd0f721068870329978a76963948696c01041339e`
- `SaboteurEnhanced.asi` SHA-256:
  `d6e4e0d181a65eaf8ef78cc83f46593e9f635916f073bcc66b96aed2218d5381`
- `dinput8.dll` SHA-256:
  `a85aa76fe9c6b61dc8831ebe710b58fcdc4ac5cea4b97a2c9a2ab2e49e790abc`
- both binaries verified PE machine x86 / 0x14C.


## ASI 0.14 result -> 0.15 coherent LightVolume audit

0.14 result:
- user tested the CorrectUV/texel-compensated DepthBlur candidate;
- the same bright white fragmented/polygonal artifact remained visible above
  and around the bright window;
- no useful change to the defect was observed.

Conclusion:
- DepthBlur resolution/texel geometry is not the primary owner of this defect;
- retain the coherent 0.14 compensations, but continue auditing the light
  composition path without removing any 0.13 quality increase.

### Exact retail WSLightVolumeManager finding

Retail baseline:
- Saboteur.exe size: 14,834,176 bytes
- SHA-256:
  `e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

The WSLightVolumeManager vtable contains method VA `0x007FE560`. It first
refreshes resource bindings and then tail-jumps to VA `0x007FE060`.

The latter routine owns a second resolution profile independent from
LightVolumeRT creation:

- VA `0x007FE07F..0x007FE09D`: caches physical backbuffer width;
- VA `0x007FE0A5`: `fld qword ptr [0x00F7AC88]`;
- retail value at VA `0x00F7AC88`: double `0.5`;
- VA `0x007FE0AD..0x007FE0CD`: width * 0.5;
- VA `0x007FE0D7..0x007FE121`: height * 0.5, using the same loaded factor;
- VA `0x007FE139+`: derives reciprocal/coordinate values from the full and
  scaled dimensions.

This is structurally the same class of mistake as the historical
full-resolution AO issue: changing the render-target dimensions alone leaves a
second coordinate owner at the old half-resolution assumption.

0.15 adds:
`Lighting.LightVolumeCoordinateResolutionScale`

- native = 0.5;
- candidate = 1.0;
- exact patched instruction RVA = `0x003FE0A5`;
- expected opcode = `DD 05`;
- expected retail operand RVA = `0x00B7AC88`;
- only this local operand is redirected to ASI-owned double storage.

0.15 preserves the complete 0.13 profile and all coherent 0.14 settings.
0.12 remains canonical until explicit user validation.

Source commits:
- `31f9967848165d0a4fb7b889a89302fce2a7dbc3` — new LightVolume coordinate owner
- `f1e2318883363113b736e3d0dd40d67c098f4f86` — 0.15 INI profile
- `e40a84ddc68300dc75063a7b488745fc337c3e18` — CI artifact label

CI:
- workflow run: 121
- run ID: `36902970888`
- artifact ID: `11182312213`
- artifact: `SaboteurEnhanced_ASI_0.15_LIGHTVOLUME_COHERENT_FULLRES_TEST_x86`
- artifact ZIP SHA-256:
  `21f863c30f13e7adf1bb5b2713ec008bb39ed1068b17dbc1629fe4a5e402f2c0`
- `SaboteurEnhanced.asi` SHA-256:
  `21e1cc81e2622bef001ad3bc50d607fc40906b6addea6636d3638bb8d8a2671d`
- `dinput8.dll` SHA-256:
  `be5716d82ccd4f8683aabf3e7e0e1179f1e7089147acc15a389d21530a2d5083`
- both binaries verified PE machine x86 / 0x14C.


## ASI 0.15 result -> 0.16 particle RestoreDepthBuffer CorrectUV audit

0.15 result:
- `LightVolumeResolutionMultiplier=2` remained enabled;
- `LightVolumeCoordinateResolutionScale=1.0` made the second manager-side
  width/height and reciprocal profile coherent with the full-resolution target;
- user screenshot result: **the same bright white fragmented/polygonal artifact
  remains** around the bright window.

Conclusion:
- the LightVolume half-resolution coordinate owner was real and worth
  correcting, but it is not sufficient to explain this artifact;
- retain the 0.15 correction;
- continue the same CorrectUV method on the remaining 0.13 full-resolution
  consumers rather than reverting quality.

### Exact particle RestoreDepthBuffer shader finding

Static reference:
- exact historical V200 executable SHA-256:
  `9d13022f1e889e5aeb16e97b6012c300ffb7acfa0b72b90971143fe7447fccaf`
- embedded shader source/debug identity:
  `WildStar/Particles/ApplyPS.hlsl`
- compiled variant:
  `RestoreDepthBuffer`

The retail WSParticleRender main target family is created at half backbuffer
resolution. 0.13 removes the two local `shr 1` operations and promotes that
family to full resolution, while retaining the BB3 hierarchy increase /16 -> /8.

The RestoreDepthBuffer variant contains a separate hard-coded half-column
reconstruction. Exact embedded constants:

- RVA `0x00D40DDC`: float `0.0125` = 1/80
- RVA `0x00D40DE8`: float `2.0`
- RVA `0x00D40DF8`: float `-80.0`
- RVA `0x00D40DFC`: float `80.0`

The shader uses these values to classify/reconstruct alternating packed columns
before applying `g_Resolution`. That is appropriate for the retail /2
ParticleBB0 layout but remains stale after the target becomes full resolution.

### 0.16 coherent full-resolution mapping

New INI:
`Particles.FullResolutionDepthRestore`

Native/default-safe meaning:
- 0 = retail packed half-column mapping.

0.16 candidate:
- 1 = pair the full-resolution particle RT with full-resolution depth-restore
  coordinates.

Atomic shader values:
- 0.0125 -> 0.0
- 2.0 -> 1.0
- -80.0 -> 0.0
- +80.0 -> 0.0

With these substitutions, the packed half-column transformation collapses to
the normal full-resolution screen coordinate before multiplication by
`g_Resolution`.

Safety:
- all four exact retail floats are verified before the particle target resize
  is attempted;
- the new correction is only meaningful when
  `Particles.RenderTargetResolutionMultiplier=2`;
- the existing render-target signatures still fail closed;
- no 0.13 quality increase is disabled;
- all 0.14 DepthBlur and 0.15 LightVolume coherence corrections remain enabled.

Source commits:
- `eb3643594dca911593def963b4575ff96503778c` — particle shader owner and coherent application path
- `0fce30db5bf5b8a3526638558274bf6083526294` — 0.16 INI profile
- `08291cf17931a0137630aad7719beb5637c03660` — CI artifact label
- `8e2ed461bf8e8ac32059a2976b3485054b66af91` — README/test plan

0.12 remains canonical until explicit user validation.


CI:
- workflow run: 125
- run ID: `36923747387`
- artifact ID: `11192398903`
- artifact: `SaboteurEnhanced_ASI_0.16_PARTICLE_DEPTH_RESTORE_CORRECTUV_TEST_x86`
- artifact ZIP SHA-256:
  `a4100fa3ae838ea7c9e19e26740a1b7c30c4d9b5dc5e496d7b9b1ead0a34391f`
- `SaboteurEnhanced.asi` SHA-256:
  `7d7545103f0c32afc4f99725f67169067de44d6119a9c66b1b95f16962205079`
- `dinput8.dll` SHA-256:
  `0aa1e3338f7cd340900bdfb8f10090b045fae453b61a5a0b399c00046785dd3e`
- both binaries verified PE machine x86 / 0x14C.


## ASI 0.16 validated fix -> 0.17 bloom-energy audit

0.16 user result:
- the bright white fragmented/polygonal defect is gone;
- this validates the particle `RestoreDepthBuffer` full-resolution coordinate
  correction as the missing dependency for the 0.13 particle RT upgrade;
- preserve this correction in all later candidates.

New remaining symptom:
- excessive brightness is concentrated around lamps, mirrors and bright
  reflections;
- the scene is not merely globally overexposed, so `Graphics.ToneMap=0.25`
  remains frozen.

### Exact PsBloomFinal owner

Retail shader:
- `WSBloomFilterHDR.hlsl / PsBloomFinal`
- exact contribution constant RVA: `0x00D69360`
- retail float: `4.0`

Historical V200 changed:
- this scalar `4.0 -> 2.0`;
- and separately rerouted `SkyBloomTextureSampler s4 -> BackBufferSampler s0`.

The sampler change is an artistic composition change and remains rejected for
the default path. 0.17 exposes only the scalar.

New INI:
`ExperimentalPostFX.BloomFinalContribution`

- retail/native = 4.0
- 0.17 test = 2.0
- allowed range = 0.5 .. 4.0
- exact retail float is verified before writing
- Bloom/GodRays resolution stays at 2x
- ToneMap, adaptive luminance and sampler routing remain untouched

Source commits:
- `d899e4da9775900f9badd3705fa34dcb7a56b60a` — PsBloomFinal scalar owner
- `402f259017bf918f0c24ba2da302776240a0c924` — 0.17 INI value
- `5d0f21b3a89c919a2fa34cb5cbc03f958d4b8760` — CI artifact label

0.12 remains the formal canonical baseline until the full 0.13+ quality branch
is explicitly promoted, but 0.16's particle CorrectUV fix is now a retained
validated correction within that branch.


CI:
- workflow run: 128
- run ID: `36927173448`
- artifact ID: `11194447259`
- artifact: `SaboteurEnhanced_ASI_0.17_BLOOM_ENERGY_COMPENSATION_TEST_x86`
- artifact ZIP SHA-256:
  `68395d8c7076615dce1cfc5bc45b076c7df9283f33c51799e7ae745786367d47`
- `SaboteurEnhanced.asi` SHA-256:
  `54bb5f54d5fda5644cafb05e80790705151f241b27e2dd691ce149cc512a5fac`
- `dinput8.dll` SHA-256:
  `fff71781d69636232e60c82c6cb8311ab4f454e0e59197b86fc2b32b69fcc7c6`
- both binaries verified PE machine x86 / 0x14C.


## ASI 0.17 result -> 0.18 historical V28/V29 brightness + anti-blur reconstruction

0.17 user result:
- excessive brightness was **not materially reduced**;
- image became visibly softer / blurrier;
- therefore `BloomFinalContribution=2.0` on its own is rejected as a
  brightness fix.

This reproduces the old September V22/V23/V24 lesson.

Recovered historical sequence from the project conversations:
- V22: PsToneMap/HDR `0.25 -> 0.20` plus PsBloomFinal `4.0 -> 2.0`;
  brightness reduced, but the user reported an ugly luminous blur;
- V23: BloomFinal `2.0 -> 1.0` worsened the blur;
- V24: BloomFinal pushed toward 0 made the image nearly black;
- V28/V29 corrected the soft composite by pairing the reduced BloomFinal
  contribution with the sharp BackBuffer sampler path; V29 later tested
  ToneMap/HDR `0.20 -> 0.18`.

The historical Original -> V200 byte manifest was decoded again in CI and
confirms the relevant exact retail deltas:

- PsToneMap raw `0x00D6327C` / runtime RVA `0x00D6487C`
  - retail 0.25;
  - later V200 target 0.15.
- PsBloomFinal contribution raw `0x00D67D62` / runtime RVA
  `0x00D69360`
  - retail float 4.0;
  - V200 float 2.0.
- PsBloomFinal sampler selector raw `0x00D67EFC` / runtime RVA
  `0x00D694FC`
  - retail byte 0x04 = SkyBloomTextureSampler s4;
  - V200 byte 0x00 = BackBufferSampler s0.

The old conversation also described a PsBloom threshold of 1.0. Rechecking
the cumulative V200/V224A shader evidence shows no Original->V200 byte delta
for the PsBloom threshold family, while the retained shader constant is already
1.0. No synthetic threshold patch is added to the current ASI.

### 0.18 candidate

0.18 preserves every validated 0.13-0.16 quality/CorrectUV improvement and
reconstructs the old brightness path coherently:

- `Graphics.ToneMap=0.20`
- `ExperimentalPostFX.BloomFinalContribution=2.0`
- new `ExperimentalPostFX.BloomFinalBackBufferSampler=1`
  - exact byte RVA `0x00D694FC`: 0x04 -> 0x00
- Bloom/GodRays pyramid stays at 2x resolution
- particle RestoreDepthBuffer CorrectUV from 0.16 stays enabled
- no reduction of render-target quality
- no change to AO, shadows or the 0.16 geometry-artifact fix

Source/config commits:
- `856a8f689df1eff73173448e9f5754bf6ed6b2b8` — sampler control
- `76b3c9a31941ee16894a04bf078bc9d8a11df7e6` — 0.18 profile
- `f882ebcafc65e862c561f81cc6ba41d204097443` — clean 0.18 CI label

0.17 is rejected. 0.16 remains the last visually clean candidate before this
brightness experiment; 0.12 remains the formal canonical baseline.


## ASI 0.18 rejected -> 0.19 ToneMap 0.15 isolation

0.18 was not promoted. Its combined historical sampler/bloom reconstruction
would alter more than brightness, including perceived saturation and image
character.

User direction: test the brightness owner alone at 0.15.

0.19 therefore keeps the visually clean 0.16 rendering path and isolates only
the ToneMap scalar:

- `Graphics.ToneMap=0.15`
- `ExperimentalPostFX.BloomFinalContribution=4.0` (retail)
- `ExperimentalPostFX.BloomFinalBackBufferSampler=0` (retail sampler)
- all 0.13-0.16 quality and CorrectUV fixes retained
- particle RestoreDepthBuffer full-resolution correction retained

This test is intended to answer one question only: whether ToneMap 0.15 removes
the excessive highlight brightness without reintroducing blur or changing the
bloom composition.

Source/config:
- `ede15d65dc0603a29c99604f9494e8e807c68056` — 0.19 INI profile
- `0a5e6d381209f005fe941b66b88237e90913c947` — runtime banner
- `e45cde127c8d83233c726c6f2ab68327457bf52e` — CI artifact label


CI:
- workflow run: 137
- run ID: `36930583879`
- artifact ID: `11195119295`
- artifact: `SaboteurEnhanced_ASI_0.19_TONEMAP_015_ISOLATION_TEST_x86`
- artifact ZIP SHA-256:
  `83f22130eb1d1436118348c0465e8b493acf2acef72535091f7a46149a473f85`
- `SaboteurEnhanced.asi` SHA-256:
  `2e656b5e6a8688ef2aa0daf1b7c1d36d19d3a08880f9a8f6936bf21afc2b9a2a`
- `dinput8.dll` SHA-256:
  `b78abf345f7ee9b935b70329845e44092381535f1798d183b54100522cc2b645`
- both binaries verified PE machine x86 / 0x14C.


## ASI 0.19 result -> 0.20 true V29 bloom path reconstruction

0.19 result:
- `Graphics.ToneMap=0.15`
- `BloomFinalContribution=4.0`
- retail bloom samplers
- user result: excessive brightness still looks essentially unchanged.

Conclusion:
- ToneMap alone is not the owner of the observed local overbright lamps/mirrors;
- do not keep pushing the ToneMap scalar in isolation.

A recheck of the original September V28/V29 notes found an important mismatch
in our 0.18 reconstruction.

The actual V28/V29 anti-blur path was:
- `PsBloom`: fixed luminance threshold at 1.0;
- `PsBloomFinal`: replace **DownsampledBackBufferSampler s2** with
  **BackBufferSampler s0**;
- `BloomFinal=2.0`;
- V29 then changed only the HDR/ToneMap scalar `0.20 -> 0.18`.

Our 0.18 candidate instead changed the later V200
`SkyBloomTextureSampler s4 -> BackBufferSampler s0` path. That is a distinct
source and can alter image character/saturation, so 0.18 was not an exact V29
reconstruction.

The exact PsBloomFinal sampler operand has now been recovered from the embedded
shader bytecode:

- retail file raw offset: `0x00D67E3C`
- PE runtime RVA: `0x00D6943C`
- retail operand: sampler register 2 = DownsampledBackBufferSampler
- V29 path: sampler register 0 = BackBufferSampler
- exact byte: `0x02 -> 0x00`

The separate V200 SkyBloom sampler byte at RVA `0x00D694FC` remains retail
(`0x04`) in 0.20.

PsBloom's embedded threshold constant is already 1.0 in the current retail
shader, so no synthetic threshold patch is added.

### 0.20 candidate

Built on the retained 0.16 CorrectUV/particle fix path:

- `Graphics.ToneMap=0.18`
- `ExperimentalPostFX.BloomFinalContribution=2.0`
- `ExperimentalPostFX.BloomFinalBackBufferSampler=0`
  - keep SkyBloomTextureSampler s4 unchanged
- new `ExperimentalPostFX.BloomFinalDownsampledBackBufferSampler=1`
  - DownsampledBackBufferSampler s2 -> BackBufferSampler s0
- all 0.13-0.16 quality/CorrectUV fixes retained
- particle RestoreDepthBuffer fix retained
- no render-target quality reduction

Source/config:
- `a2b1e88a30611390cfef65d56ffb59274cca7939` — exact V29 sampler owner
- `c7a3b1db2e55954aea25f7dbef109ca4eb2e0b12` — 0.20 profile
- `aad9710634ddff45d40af70480a08c402c13cb4e` — CI artifact label

0.19 rejected. 0.16 remains the last visually clean reference before brightness
experiments.


## ASI 0.20 rejected -> 0.21 PsBloom prefilter energy normalization

0.20 result:
- true historical V29 path tested;
- user result: excessive brightness still unchanged.

Therefore neither isolated ToneMap changes nor the V29 final-composite sampler
path is the owner of the current local overbright lamps/mirrors.

A fresh audit of the retail PsBloom bytecode found the upstream extraction gain
that was not yet exposed:

WSBloomFilterHDR.hlsl / PsBloom
- retail DEF c1 = { 1.0, 4.0, 0.0, -1/3 }
- shader sequence:
  - sample bloom source;
  - multiply sampled RGB by c1.y;
  - compute luminance with c2 = {0.27, 0.67, 0.06, 0.01};
  - subtract the runtime luminance threshold;
  - clamp at zero;
  - feed the resulting bright-pass into the blur/composite chain.

Exact owner:
- raw retail file offset: 0x00D68568
- PE runtime RVA: 0x00D69B68
- retail float: 4.0

This is upstream of the blur and final composite, making it a cleaner brightness
normalization owner than BloomFinalContribution.

0.21 deliberately restores the visually clean 0.16 brightness/composite path:
- ToneMap=0.25
- BloomFinalContribution=4.0
- SkyBloom sampler retail
- DownsampledBackBuffer sampler retail

All 0.13-0.16 quality/CorrectUV improvements remain enabled.

New INI:
ExperimentalPostFX.BloomPrefilterGain

- retail = 4.0
- 0.21 test = 1.0
- rationale: BloomResolutionMultiplier=2 changes the bloom pyramid from /4 to
  /2, doubling width and height. 1.0 is the exact 1/4 area-energy normalization
  test relative to the retail gain 4.0, while keeping the higher-resolution
  pyramid itself.

Source/config:
- `5453b65e4f0589458d31db89db436c05a68d4904` — PsBloom gain owner
- `abd39f6607f1d314edbbe2c32494f68ad80e741c` — 0.21 profile
- `45061cdbdb0361c6b1829a610eb9cef0565319c2` — CI artifact label

0.16 remains the last visually clean reference before brightness experiments.


## ASI 0.21 user validation — PROMOTED TO CANONICAL

User result: **validated**.

0.21 successfully resolves the excessive local highlight brightness that
remained after 0.16 while preserving the 0.16 fix for the white fragmented
geometry artifact.

Canonical consequences:
- retain the 0.16 particle RestoreDepthBuffer full-resolution coordinate fix;
- retain every validated 0.13-0.16 quality/CorrectUV improvement;
- reject 0.17, 0.18, 0.19 and 0.20 as brightness solutions;
- keep ToneMap at retail 0.25;
- keep BloomFinal contribution at retail 4.0;
- keep both final bloom samplers on their retail paths;
- keep `BloomResolutionMultiplier=2`;
- promote `BloomPrefilterGain=1.0` as the validated energy normalization for
  the 2x Bloom/GodRays pyramid.

Validated 0.21 artifact:
- workflow run: 143
- run ID: `36934389357`
- artifact ID: `11196979670`
- artifact: `SaboteurEnhanced_ASI_0.21_BLOOM_PREFILTER_ENERGY_NORMALIZATION_TEST_x86`
- artifact ZIP SHA-256:
  `984ac9d8fc221466118738f6e40b490378612b80cac13011e304ef0934a351bf`
- `SaboteurEnhanced.asi` SHA-256:
  `c97d8f8d29d98b5f54024a61a6bb588a8840b4d6d19fa63c90cac619148bacdb`
- `dinput8.dll` SHA-256:
  `6a11d71d7142b2ac4d61fe99d3b089078d97abebdccce6aa102463001f73ec31`

0.21 is now the canonical ASI baseline for future work.


## ASI 0.22 world render-target quality candidate

Status: **TEST CANDIDATE; 0.21 remains canonical until user validation**.

Base:
- canonical ASI 0.21;
- all 0.21 brightness and CorrectUV fixes frozen.

Exact 0.22 INI deltas:
- `Water.ReflectionWidth 512 -> 2048`
- `Water.ReflectionHeight 128 -> 512`
- `Water.NormalMapResolution 128 -> 512`
- `Sky.ResolutionMultiplier 1 -> 2`
- `Rain.CubeResolution 128 -> 512`

Rationale:
- user preference is to push clearly enough to make the effect observable,
  then reduce only if a regression appears;
- WaterReflection keeps its native 4:1 aspect ratio;
- Water reflection dimensions are shared coherent owner globals;
- WaterNormals/Temp use one matched fixed-size family;
- SkyDome already has a coherent 2x implementation across all three internal
  RT families;
- RainCubeRT is an isolated cubemap face dimension and does not touch the
  separately owned HardwareRainDepthTexture;
- rain density remains native to keep the test focused on render-target
  quality rather than scene density/art direction.

Frozen canonical 0.21:
- ShadowMap 4096 + PCF5x5;
- full-resolution AO;
- all 0.13-0.16 high-resolution PostFX + CorrectUV fixes;
- particle RestoreDepthBuffer fix;
- ToneMap 0.25;
- BloomPrefilterGain 1.0;
- retail BloomFinal and sampler paths.

Source/config:
- `5a33932af214628a03ba04da8a553b7369757a3c` — 0.22 INI profile
- `663edac116246260f24143c872eb4da2fdbc082e` — runtime banner
- `b0bc24dd118c5a4f78805b79624834c6b25fe9ba` — CI artifact label

Do not promote 0.22 until explicit user validation.


CI:
- workflow run: 147
- run ID: `36985870325`
- artifact ID: `11217656886`
- artifact: `SaboteurEnhanced_ASI_0.22_WORLD_RENDER_TARGET_QUALITY_TEST_x86`
- artifact ZIP SHA-256:
  `1c5f82018e64d3e461d095e8e223b5b80abd8ae439b3cb10957567d3a761f427`
- `SaboteurEnhanced.asi` SHA-256:
  `dca2088d0cdb031e0e2e7b484d8ec219e070a0abc56e5bc8c053c40bd445da1e`
- `dinput8.dll` SHA-256:
  `7631ad8af83e40c77d46b1a0f786207a31198273e55bb13a5f2298d0f4727165`
- both binaries verified PE machine x86 / 0x14C.


## ASI 0.22A user validation — PROMOTED TO CANONICAL

User result: **validated**.

0.22A is functionally identical to 0.22. The only additional change is
packaging correction:
- `README_ASI.md` -> `README.txt` inside the distributed artifact.

Validated rendering deltas:
- Water.ReflectionWidth 512 -> 2048
- Water.ReflectionHeight 128 -> 512
- Water.NormalMapResolution 128 -> 512
- Sky.ResolutionMultiplier 1 -> 2
- Rain.CubeResolution 128 -> 512

Frozen from 0.21:
- ShadowMap 4096 + PCF5x5
- full-resolution AO
- 0.13-0.16 high-resolution PostFX + CorrectUV fixes
- particle RestoreDepthBuffer full-resolution correction
- ToneMap 0.25
- BloomPrefilterGain 1.0
- retail BloomFinal and sampler paths

Validated 0.22A artifact:
- workflow run: 149
- run ID: `36989287352`
- artifact ID: `11218681459`
- artifact: `SaboteurEnhanced_ASI_0.22A_WORLD_RENDER_TARGET_QUALITY_REPACK_x86`
- artifact ZIP SHA-256:
  `8c6cf32c6b1758f950cae5af2e9e233e9ec85d0fe3e4ccde498db9031cb55d42`
- `SaboteurEnhanced.asi` SHA-256:
  `5fc84ad5c0aaadb45c0320dc5eb66d6f64cb492a98b9f9f59d9443f76d377ddb`
- `dinput8.dll` SHA-256:
  `e18038a936fbad3c6d095b5dac5a602f3d3b7f7ddb040863884dfa73f80987bd`

0.22A is now the canonical ASI baseline for future work.


## ASI 0.23 extreme world render-target candidate

Status: **TEST CANDIDATE; 0.22A remains canonical until explicit validation**.

Base:
- canonical ASI 0.22A;
- no changes to any post-processing, brightness, shadow, AO, particle,
  LightVolume, distance or engine-limit owner.

Exact 0.23 deltas:
- `Water.ReflectionWidth 2048 -> 4096`
- `Water.ReflectionHeight 512 -> 1024`
- `Water.NormalMapResolution 512 -> 1024`
- `Rain.CubeResolution 512 -> 1024`
- `Sky.ResolutionMultiplier` remains validated `2`

Rationale:
- these exact owner families were already validated at the 0.22A values;
- this candidate follows the project rule to push clearly first, then reduce
  only if a visible or stability regression appears;
- WaterReflection keeps the 4:1 aspect ratio;
- WaterNormals/Temp remain dimensionally matched;
- RainCubeRT remains isolated from HardwareRainDepthTexture;
- SkyDome is intentionally not pushed beyond x2 because x2 is the highest
  already-audited coherent implementation.

Frozen 0.22A:
- ShadowMap 4096 + PCF5x5;
- full-resolution AO;
- full-resolution MotionBlur/ScaledTexture/DamageBlur;
- coherent DepthBlur;
- full-resolution LightVolume + coordinate correction;
- particle RT x2 + RestoreDepthBuffer identity mapping;
- ToneMap 0.25;
- BloomPrefilterGain 1.0;
- retail BloomFinal and sampler paths;
- packaged documentation name `README.txt`.

Source/config:
- `a82ef56b64633bcb915c7ba36880be9fcbf97d91` — 0.23 INI profile
- `1252a803cd8aa45891cc0479efd2aa51a54c0dee` — runtime banner
- `f3f0436c4e8dbe1af10ef0fceb66ac3e0edb2a14` — CI artifact label

Do not promote 0.23 until explicit user validation.


CI:
- workflow run: 154
- run ID: `36991037292`
- artifact ID: `11219553092`
- artifact: `SaboteurEnhanced_ASI_0.23_EXTREME_WORLD_RENDER_TARGET_TEST_x86`
- artifact ZIP SHA-256:
  `8a457ce9379160de0d12b5f57c5217d72f423428b8b4658fab59665130227cb8`
- `SaboteurEnhanced.asi` SHA-256:
  `75c6343db5056c8a29704ec52a0862d7f97008ef57f7861004fa2b5dfd454b51`
- `dinput8.dll` SHA-256:
  `8a76f2da2fc3c99a3922f4f597ef486e959e0a187b1aa4a06a9b6c1fe46adec0`
- both binaries verified PE machine x86 / 0x14C.


## ASI 0.23 user validation — PROMOTED TO CANONICAL

User result: **validated**.

Canonical world-render values:
- Water.ReflectionWidth = 4096
- Water.ReflectionHeight = 1024
- Water.NormalMapResolution = 1024
- Sky.ResolutionMultiplier = 2
- Rain.CubeResolution = 1024

No regressions were reported. 0.23 therefore supersedes 0.22A as the current
canonical ASI baseline.

Validated 0.23 artifact:
- workflow run: 154
- run ID: `36991037292`
- artifact ID: `11219553092`
- artifact: `SaboteurEnhanced_ASI_0.23_EXTREME_WORLD_RENDER_TARGET_TEST_x86`
- artifact ZIP SHA-256:
  `8a457ce9379160de0d12b5f57c5217d72f423428b8b4658fab59665130227cb8`
- `SaboteurEnhanced.asi` SHA-256:
  `75c6343db5056c8a29704ec52a0862d7f97008ef57f7861004fa2b5dfd454b51`
- `dinput8.dll` SHA-256:
  `8a76f2da2fc3c99a3922f4f597ef486e959e0a187b1aa4a06a9b6c1fe46adec0`


## ASI 0.24 maximum world render-target candidate

Status: **TEST CANDIDATE; 0.23 remains canonical until explicit validation**.

Base:
- canonical ASI 0.23.

Exact deltas:
- `Water.ReflectionWidth 4096 -> 8192`
- `Water.ReflectionHeight 1024 -> 2048`
- `Water.NormalMapResolution 1024 -> 2048`
- `Rain.CubeResolution 1024 -> 2048`
- `Sky.ResolutionMultiplier` remains 2

These values are the current audited ASI limits:
- WaterReflection safe range allows width up to 8192;
- WaterNormals limit is 2048;
- RainCubeRT limit is 2048.

No other rendering or engine setting changes in 0.24.

Frozen canonical 0.23:
- ShadowMap 4096 + PCF5x5
- full-resolution AO
- all validated full-resolution PostFX/CorrectUV fixes
- LightVolume coherent full-resolution path
- particle RT x2 + RestoreDepthBuffer correction
- ToneMap 0.25
- BloomPrefilterGain 1.0
- retail BloomFinal/sampler paths
- README packaged as `README.txt`

Source/config:
- `7e1c0cf8f381bd0c00ed399d5db7163126cf6383` — 0.24 INI profile
- `602063b8cc19d648e8bfd32f5486c84759d8b627` — runtime banner
- `3b13049d041328db0f7b0167bef2e9cec739c345` — CI artifact label

Do not promote 0.24 until explicit user validation.


CI:
- workflow run: 159
- run ID: `36991945476`
- artifact ID: `11220180460`
- artifact: `SaboteurEnhanced_ASI_0.24_MAX_WORLD_RENDER_TARGET_TEST_x86`
- artifact ZIP SHA-256:
  `f4d80c3fd383a5d1f887fe5bc97406720f8b24c66bd77ef768576af7f191e138`
- `SaboteurEnhanced.asi` SHA-256:
  `880bc32a1c6b1d9de603ff33ee6ada65b7e7c9d349ef85baade9ef28173d4418`
- `dinput8.dll` SHA-256:
  `41fc890644aad0c305f0e855062be40b1891f5db9689dad3e7ad44c478354130`
- both binaries verified PE machine x86 / 0x14C.


## 0.24 rejected / abandoned by direction

User direction: stop this class of test entirely.

0.24 maximum world-RT escalation is **not validated** and must not become a
future base.

Repository restored to canonical 0.23 values:
- Water.ReflectionWidth = 4096
- Water.ReflectionHeight = 1024
- Water.NormalMapResolution = 1024
- Sky.ResolutionMultiplier = 2
- Rain.CubeResolution = 1024

New project rule:
- no more blind render-target size escalation;
- audit a real engine owner first;
- build only when the change addresses a concrete issue or verified bottleneck.

Next investigation priority:
- streaming / micro-freezes / pop-in ownership and scheduling.


## ASI 0.25 streaming-pressure telemetry audit

Status: **DIAGNOSTIC ONLY. 0.23 remains canonical.**

This branch follows the post-0.24 audit-first rule. No scalar is increased.

### Why WSReadJob / WSUncompressJob are being measured

The current ASI retains validated enlarged streaming coverage:
- Low 16000;
- Medium 3200;
- High 2500.

The clean Core/ASI reset intentionally restored WSReadJob and WSUncompressJob
from historical V272 2400/2400 back to retail 1200/1200 because stability alone
was not evidence that the larger pools were necessary.

Historical V272 owner remains exact:
- shared initializer VA 0x0162F336;
- RVA 0x0122F336;
- retail `BF B0 04 00 00` = 1200;
- historical validated `BF 60 09 00 00` = 2400.

0.25 measures real runtime pressure before considering that change again.

### Generic pool descriptor semantics proven

Static re-audit of the exact pool manager:
- initializer at VA 0x00DC1700 sets +0x34 and +0x30 to capacity;
- it allocates `objectSize(+0x2C) * capacity(+0x34)`;
- allocation path at VA 0x00DC1940 removes one object from +0x44 and increments
  +0x38 after successful allocation;
- release path at VA 0x00DC1A20 decrements +0x38 and pushes the object back to
  +0x44;
- invariant check at VA 0x00DC17C0 traverses +0x44 and verifies
  freeNodes == (+0x34 - +0x38).

Therefore +0x34/+0x38 are suitable direct capacity/occupancy telemetry fields.

Streaming descriptor locations:
- WSReadJob VA 0x0132B9D8 / RVA 0x00F2B9D8, object size 44;
- WSUncompressJob VA 0x0132BA40 / RVA 0x00F2BA40, object size 40.

### FIFO telemetry

Core 1 retains only the V200 64-slot full-queue correctness fix:
- hook RVA 0x009B5A77;
- retained cave RVA 0x00281084;
- continuation RVA 0x009B5A83.

The native queue object uses:
- +0x10C full flag;
- +0x110 read/head index;
- +0x114 write/tail index;
- +0x118 ring storage.

The retained cave performs:
- load +0x110;
- increment;
- AND 0x3F;
- store +0x110;
- continue enqueue.

0.25 verifies both the Core1 hook bytes and complete retained cave bytes before
redirecting that cave through a pass-through counter. The hook increments only
a diagnostic counter, reproduces the exact drop-oldest instructions, and jumps
to the same continuation. Queue capacity and behavior are unchanged.

### Diagnostic cadence and decision gate

Default:
- sample pool occupancy every 50 ms;
- aggregate report every 5000 ms;
- no per-event disk logging in the FIFO hook.

The diagnostic summary records:
- WSReadJob peak / capacity-hit transitions;
- WSUncompressJob peak / capacity-hit transitions;
- FIFO-full event count.

Only measured pressure can justify the next functional streaming change.

Source/config:
- `e32511b7d6b615b8894d1afd62efd624d5c9d7d6` — telemetry implementation;
- `43ae89ec8f644c29f6ad8292c31ec1fd92fa16a6` — diagnostic INI defaults;
- `18cfabfbb3f0201eb3b1682d34f8df7663413274` — CI artifact label.


ASI 0.25 CI:
- workflow run: 167
- run ID: `36995007567`
- artifact ID: `11220798391`
- artifact: `SaboteurEnhanced_ASI_0.25_STREAMING_TELEMETRY_DIAGNOSTIC_x86`
- artifact ZIP SHA-256:
  `7e2be4cdcc39e60019dd38c8583038b286d0d441a6c7848420193d9c3d3ce270`
- `SaboteurEnhanced.asi` SHA-256:
  `5e7312edda4e6ca55292a26c1d8e9b4008cf8e836685d31856743d2306ef6199`
- `dinput8.dll` SHA-256:
  `254d6fc9d277a070d9a9de8eaec77b709186528726880e77aee312ea7714564f`
- both binaries verified PE machine x86 / 0x14C.
- packaged documentation remains `README.txt`.

0.25 remains diagnostic-only and must not replace canonical 0.23 until its
telemetry is interpreted and a separate functional change is explicitly
validated.


## ASI 0.25 streaming telemetry runtime result

User runtime log (2026-10-02) confirms the telemetry path worked correctly:
- WSReadJob descriptor online at capacity 1200;
- WSUncompressJob descriptor online at capacity 1200;
- WSReadJob observed peak: 94 / 1200 (~7.8%);
- WSUncompressJob observed peak: 492 / 1200 (41.0%);
- sampled capacity-hit transitions: 0 / 0;
- exact Core1 FIFO full-queue events: 0;
- final summary: ReadPeak=94, UncompressPeak=492, FIFOFullEvents=0.

Decision:
- do not restore historical V272 1200 -> 2400 on current evidence;
- do not enlarge the 64-slot FIFO;
- the next streaming investigation moves downstream/upstream to the real retail
  single-submit async scheduler, completion accounting, and I/O service latency;
- aggressive coalescing remains unmodified until scheduler pressure is measured.

Important limitation:
- pool occupancy is sampled every 50 ms, so very short transient spikes are not
  mathematically impossible to miss. The large observed headroom plus zero exact
  FIFO-full events nevertheless argues strongly against either 1200-entry pool
  or the 64-slot FIFO being the current sustained bottleneck.


## ASI 0.26 retail scheduler audit and telemetry candidate

Status: **DIAGNOSTIC ONLY. 0.23 remains canonical.**

### Historical Async32 reconstructed exactly enough for instrumentation

The Original -> V200 manifest was decoded again and the relevant streaming
regions were audited directly.

Retail/Core1 scheduler:
- entry RVA 0x009B6750;
- retail prologue bytes: `55 56 57 8B F9`;
- scheduler object in EDI;
- single current I/O request pointer: scheduler +0x218;
- queue object begins at scheduler +0x10C;
- submit routine: RVA 0x009B5540;
- completion callback: RVA 0x009B56C0.

The untouched retail body after the prologue:
1. tests `[scheduler+0x218] == 0`;
2. peeks the 64-slot queue;
3. validates the request;
4. pops one request;
5. stores it in `[scheduler+0x218]`;
6. submits exactly that one request via RVA 0x009B5540.

Retail completion:
- callback status is the first stack argument;
- on success it marks the current request state 3;
- clears `[scheduler+0x218]`;
- peeks/pops one next queue request;
- stores that request at +0x218;
- calls the same submit routine once.

### What V200 Async32 actually changed

V200 region 146 redirects the scheduler entry to cave RVA 0x0000368D.

The recovered V200 loop at RVA 0x0000369C:
- `cmp dword ptr [edi+0x224], 32`;
- while below 32, peek/pop another queued request;
- `lock inc dword ptr [edi+0x224]`;
- submit request;
- loop again.

V200 region 140 redirects completion through cave RVA 0x000036DC, where the
successful completion path executes:
- `lock dec dword ptr [scheduler+0x224]`;
- then re-enters the same multi-submit loop.

Therefore old Async32 is a genuine multi-I/O scheduler with matched
submit/completion accounting, not a one-byte or one-immediate tweak.

### 0.26 instrumentation design

No multi-submit behavior is enabled.

Verified hook sites:
- tick submit CALL RVA 0x009B679E, expected `E8 9D ED FF FF`;
- completion-chain submit CALL RVA 0x009B572A,
  expected `E8 11 FE FF FF`;
- callback entry RVA 0x009B56C0,
  expected `8B 44 24 04 85 C0`.

The two submit CALLs are redirected through a pass-through wrapper which records
metrics and tail-jumps to the original submit routine. The callback entry is
redirected through a pass-through wrapper, then reproduces the exact replaced
`mov eax,[esp+4] ; test eax,eax` sequence and returns to RVA 0x009B56C6.

Telemetry:
- exact submit/completion counts;
- QPC service latency;
- service >=2/5/10/20/50 ms buckets;
- queue-depth peak;
- backlog present at submit/completion;
- 5 ms samples of busy / queued / busy+queued / idle+queued;
- maximum consecutive idle+queued time;
- read byte counts reconstructed from native fields +0x14/+0x20/+0x24;
- read-size distribution;
- consistency checks for overlapping submits, unmatched completions and current
  job pointer mismatch.

0.25 telemetry remains active:
- WSReadJob/WSUncompressJob pool occupancy;
- exact Core1 FIFO-full events.

Source/config:
- `3c80cd349efc43550b90b42aeb255c08e7964de8` — scheduler telemetry implementation;
- `59cbc0d1dd2b8f0ed76623391f7dd73634cc0c5c` — scheduler diagnostic INI;
- `6f9760209534c649f471780a2c28c3d383d02a58` — clean 0.26 CI label and removal of temporary manifest audit.

Decision gate after runtime log:
- long service latency + frequent busy-with-queued backlog => multi-submit is a
  credible next functional A/B;
- low service latency / little backlog => do not revive Async32;
- idle-with-queued periods => investigate scheduler wake/dispatch timing;
- large read sizes / latency correlation => investigate coalescing separately,
  without bundling it into Async scheduling.


ASI 0.26 final CI:
- workflow run: 173
- run ID: `37002033029`
- artifact ID: `11223834444`
- artifact: `SaboteurEnhanced_ASI_0.26_RETAIL_SCHEDULER_TELEMETRY_DIAGNOSTIC_x86`
- artifact ZIP SHA-256:
  `25d1fa7a40cbb7d99702d1bb2e5ce3ed47f09ff6f082ac72f0dd3bdd2ac25af0`
- `SaboteurEnhanced.asi` SHA-256:
  `d7c962fef6cdb7f429f9d1a42f3392197dc3a2274b664b9561da5607323f58c7`
- `dinput8.dll` SHA-256:
  `af456363e35c154260947909adce1428618b3fec827b043b7fd074d00b564d95`
- both binaries verified PE machine x86 / 0x14C;
- packaged documentation remains `README.txt`;
- artifact commit `e3b92221fdf8996cfb336e3543a25468755e7b0e` includes the
  0.26 README and diagnostic-labeled INI.

0.26 remains diagnostic-only. Canonical gameplay/render baseline remains 0.23
until telemetry justifies and a separate functional scheduler candidate is
explicitly validated.


## ASI 0.26 runtime result — historical Async32 path dormant

The user-supplied 0.26 runtime log confirms:
- both 0.25 pool telemetry and 0.26 scheduler telemetry installed successfully;
- WSReadJob peak reached 121 / 1200;
- WSUncompressJob peak reached 542 / 1200;
- no pool capacity hit;
- no Core1 FIFO-full event.

However every 0.26 scheduler metric remained exactly zero for the entire run:
- Submit=0;
- Complete=0;
- QueuePeak=0;
- Busy=0.0%;
- Busy+Queued=0.0%;
- no read-size samples;
- no latency samples.

Therefore the region-146/V200 Async32 scheduler path is not the active path for
the observed current streaming workload. This invalidates the assumption that
the active WSReadJob/WSUncompressJob traffic necessarily flows through that
historical scheduler.

Decision:
- do not restore Async32;
- disable 0.26 scheduler telemetry by default;
- trace the active pool allocation/free provenance directly.

## ASI 0.27 active streaming-pool provenance diagnostic

Status: **DIAGNOSTIC ONLY. 0.23 remains canonical.**

Exact generic pool manager functions:
- Allocate VA 0x00DC1940 / RVA 0x009C1940;
- Release VA 0x00DC1A20 / RVA 0x009C1A20.

Exact verified 9-byte prologues:
- allocate: `56 8B F1 83 7E 3C 00 75 21`;
- release: `56 8B F1 F6 46 48 02 74 2C`.

Descriptor filter:
- WSReadJob RVA 0x00F2B9D8;
- WSUncompressJob RVA 0x00F2BA40.

Hook architecture:
- exact fail-closed prologue verification;
- local executable trampoline reproduces the replaced 9 bytes;
- pass-through __fastcall wrappers preserve the original thiscall argument
  contract;
- unrelated pools are ignored after two pointer comparisons;
- no per-event disk logging.

Collected provenance:
- allocation/free totals;
- distinct caller return RVAs;
- approximate direct CALL RVA as returnRVA - 5;
- per-caller counts;
- first/last observed tick.

0.25 pool/FIFO pressure telemetry remains enabled.
0.26 scheduler probe remains compiled for reference but defaults OFF.

Source/config:
- `6e3af6e9cdf362e6e840bfb161b57496977ef1ce` — pool provenance implementation;
- `c6ef474845e15afdf7e90beccfc9b3349777914c` — diagnostic INI switch;
- `0c907ebed13486dd1bdddfc25e2fac4ac1120322` — 0.27 CI artifact label.


ASI 0.27 final CI:
- workflow run: 177
- run ID: `37004322673`
- artifact ID: `11224723464`
- artifact: `SaboteurEnhanced_ASI_0.27_STREAMING_POOL_PROVENANCE_DIAGNOSTIC_x86`
- artifact ZIP SHA-256:
  `80a672c08d51383d9005dc737cd5a06b94a7626d3694b7dadbd6a96cab256c2e`
- `SaboteurEnhanced.asi` SHA-256:
  `346dd9dab4835719730b963e2eb4019a158a3a10be9fdf402b2e979b9ffd4a75`
- `dinput8.dll` SHA-256:
  `3a1872ffc21d8b61018b857eb6cd0875acd84a737c61f6d7d8f17a183fa56cc2`
- both binaries verified PE machine x86 / 0x14C;
- packaged documentation remains `README.txt`.

0.27 remains diagnostic-only. Canonical gameplay/render baseline remains 0.23.


## 0.27 runtime result — REJECTED

User result: runtime crash.

The generic pool allocation/release provenance hooks introduced by 0.27 are
therefore rejected and must not be used as a future base.

Development decision:
- stop this class of invasive allocator/scheduler telemetry;
- preserve the useful conclusions from 0.25:
  WSReadJob and WSUncompressJob had substantial headroom and FIFO-full was 0;
- preserve the useful conclusion from 0.26:
  the historical V200 Async32 path was dormant during the observed workload;
- do not spend further builds instrumenting deeper runtime internals merely to
  chase streaming provenance;
- return to static reverse-engineering and only create a candidate when a
  concrete owner and expected user-facing improvement are established.

Repository source/config/workflow restored to the exact post-0.24 0.23
canonical state.


## ASI 0.28 static correction — DepthBlur RAW/RVA ownership

Status: **FUNCTIONAL TEST CANDIDATE. 0.23 remains canonical.**

### Trigger

Recent canonical runtime logs repeatedly showed:
- `[SKIP] DepthBlur mask shader tap offsets native value mismatch at RVA 0x00D66AE0`
- `[SKIP] DepthBlur color shader texel offsets native value mismatch at RVA 0x00D664A0`

All other canonical 0.23 render owners applied normally.

### Static root cause

ASI 0.14 recovered the DepthBlur literal constants while inspecting an
executable file. The recorded addresses were subsequently described and coded
as runtime RVAs.

A second-pass PE mapping audit proves these values belong to the same embedded
shader section whose RAW-to-RVA displacement is +0x1600.

Independent already-correct examples in current source:
- PsBloomFinal DownsampledBackBuffer sampler:
  RAW 0x00D67E3C -> RVA 0x00D6943C;
- PsBloom prefilter gain:
  RAW 0x00D68568 -> RVA 0x00D69B68.

The old DepthBlur code omitted that +0x1600 displacement.

Corrected mask RVA family:
- 0x00D66AE0 -> 0x00D680E0
- 0x00D66AF8 -> 0x00D680F8
- 0x00D66AFC -> 0x00D680FC
- 0x00D66B00 -> 0x00D68100
- 0x00D66B04 -> 0x00D68104
- 0x00D66DB8 -> 0x00D683B8
- 0x00D66DBC -> 0x00D683BC
- 0x00D66DD0 -> 0x00D683D0
- 0x00D66DD4 -> 0x00D683D4
- 0x00D66DD8 -> 0x00D683D8
- 0x00D66DDC -> 0x00D683DC

Corrected color RVA family:
- 0x00D664A0 -> 0x00D67AA0
- 0x00D664A4 -> 0x00D67AA4
- 0x00D666B0 -> 0x00D67CB0
- 0x00D666B4 -> 0x00D67CB4

No expected float or scale changes:
- mask expected literals remain 7.5, 2, 4, 6, 8 and 6, 7.5, 2, 4, 5, 8;
- color expected literals remain -1/+1;
- canonical compensation remains 0.5 for the doubled mask RT;
- canonical compensation remains 2/3 for the 1.5x color-pyramid pixel density.

### Scope

Only the two address tables and runtime banner change.
0.23 quality/profile values are otherwise frozen.

This is not a telemetry build and adds no runtime hook.

Source/config:
- `60e9206ccaca88c204760612eab6f524f8d64fdf` — corrected shader RVAs;
- `9db17aff281ccce5570a3ef1f129433a4fa830b7` — candidate-labelled INI;
- `a2ab85ee8ed7cc4c98775590dfa1d6deabf77ed0` — 0.28 CI label.

Decision gate:
- both DepthBlur groups must pass exact float verification and log `[OK]`;
- image must remain clean, without the old fragmented artifact;
- no blur/halo regression;
- only then can 0.28 supersede canonical 0.23.


ASI 0.28 CI:
- workflow run: 187
- run ID: `37015359254`
- artifact ID: `11229427250`
- artifact: `SaboteurEnhanced_ASI_0.28_DEPTHBLUR_RAW_RVA_CORRECTUV_FIX_TEST_x86`
- artifact ZIP SHA-256:
  `00bf03d6ed7f2c3094a062f349fc7efa6d9af33e1fb37ebfedea9bb206ce0376`
- `SaboteurEnhanced.asi` SHA-256:
  `100e9f98cbb94561e1ce5fb1c4f2ad350348959c159bba5b3cf43d7870d8c6ff`
- `dinput8.dll` SHA-256:
  `9a909fa33ec19a2d264633ddf848cb4764ee7fb6d7945bc4091b4a2263a079f9`
- both binaries verified PE machine x86 / 0x14C;
- packaged documentation remains `README.txt`.

0.28 is functional test only. 0.23 remains canonical until explicit validation.


## ASI 0.28 user validation — PROMOTED TO CANONICAL

User result: **validated**.

0.28 is now the canonical ASI baseline.

Validated functional delta over 0.23:
- corrected DepthBlur mask shader owner RVAs using RAW + 0x1600;
- corrected DepthBlur color shader owner RVAs using RAW + 0x1600;
- no quality scalar, render-target size, streaming, LOD, shadow, AO, bloom or
  engine-capacity change.

Validated 0.28 artifact:
- workflow run: 187
- run ID: `37015359254`
- artifact ID: `11229427250`
- artifact: `SaboteurEnhanced_ASI_0.28_DEPTHBLUR_RAW_RVA_CORRECTUV_FIX_TEST_x86`
- artifact ZIP SHA-256:
  `00bf03d6ed7f2c3094a062f349fc7efa6d9af33e1fb37ebfedea9bb206ce0376`
- `SaboteurEnhanced.asi` SHA-256:
  `100e9f98cbb94561e1ce5fb1c4f2ad350348959c159bba5b3cf43d7870d8c6ff`
- `dinput8.dll` SHA-256:
  `9a909fa33ec19a2d264633ddf848cb4764ee7fb6d7945bc4091b4a2263a079f9`

Future builds must start from 0.28 and must not restore the old DepthBlur
addresses 0x00D66AE0 / 0x00D664A0 as runtime RVAs.


## ASI 0.29 Scaleform / high-resolution UI cache candidate

Status: **FUNCTIONAL TEST CANDIDATE. 0.28 remains canonical.**

This candidate follows the post-0.27 audit-first rule and contains no runtime
telemetry hook.

Exact owners were already re-audited against both the exact retail executable
and the byte-perfect reconstructed V200 image.

### _Mesh_Cache

- adjacent retail cache name VA 0x01062064;
- constructor VA 0x00BB4667 / RVA 0x007B4667 / RAW 0x007B3867;
- exact retail instruction:
  `C7 46 14 00 00 80 00`
  = `mov dword ptr [esi+14h], 0x00800000` = 8 MiB;
- V200:
  `C7 46 14 00 00 00 01`
  = 16 MiB.

INI owner:
- `UI.MeshCacheMiB`
- supported in 0.29: 8 or 16 only.

### Vector glyph cache

- constructor VA 0x00BE6AD7 / RVA 0x007E6AD7;
- field [esi+0x9C0];
- exact retail:
  `C7 86 C0 09 00 00 00 02 00 00` = 512;
- V200:
  `C7 86 C0 09 00 00 00 04 00 00` = 1024;
- nearby retail warning explicitly names SetMaxVectorCacheSize.

INI owner:
- `UI.VectorGlyphCache`
- supported in 0.29: 512 or 1024 only.

### Font-cache texture count

Constructor A:
- VA 0x00BE7D3E / RVA 0x007E7D3E;
- exact retail `C7 46 1C 01 00 00 00`.

Constructor B:
- VA 0x00BE7E19 / RVA 0x007E7E19;
- exact retail `C7 46 1C 01 00 00 00`.

Historical V200 changes both to:
`C7 46 1C 02 00 00 00`.

0.29 verifies both constructor signatures before writing either site.

INI owner:
- `UI.FontCacheTextures`
- supported in 0.29: 1 or 2 only.

### Scope

Default 0.29 test profile:
- MeshCacheMiB = 16;
- VectorGlyphCache = 1024;
- FontCacheTextures = 2.

Every 0.28 validated graphics/render/DepthBlur setting is otherwise frozen.

Source/config:
- `3a93cef7f5b441ead8d902b2a8c7b26b83662b03` — exact UI cache owners;
- `806aaf18f046601f6c6b38614f3aee4db92fe76a` — 0.29 UI profile;
- `6e0d079d7555c8b7a02b5135afbe2ca5bf14731f` — CI artifact label.

Do not promote 0.29 until explicit user validation.


ASI 0.29 CI:
- workflow run: 195
- run ID: `37133207518`
- artifact ID: `11277238768`
- artifact: `SaboteurEnhanced_ASI_0.29_SCALEFORM_UI_CACHE_TEST_x86`
- artifact ZIP SHA-256:
  `e5d862bffab7e3510e2d38740a5fade0e9f3d1b125a27fee74e0625c9dbcea4f`
- `SaboteurEnhanced.asi` SHA-256:
  `89989868fa02f62e914df42154bcf084fd11dbde552ec26c9cd2ee16888d1210`
- `dinput8.dll` SHA-256:
  `befb7361d5c9591a7bf83ceed9cc579b1844d55dc678ce3d0362aa25abb59c19`
- both binaries verified PE machine x86 / 0x14C;
- packaged documentation remains `README.txt`.

0.29 remains test-only. 0.28 remains canonical until explicit validation.


## ASI 0.29 user validation — PROMOTED TO CANONICAL

User result: **validated**.

0.29 is now the canonical ASI baseline.

Validated functional delta over 0.28:
- Scaleform _Mesh_Cache 8 MiB -> 16 MiB;
- vector glyph cache 512 -> 1024;
- both font-cache constructors 1 -> 2 textures.

Validated 0.29 artifact:
- workflow run: 195
- run ID: `37133207518`
- artifact ID: `11277238768`
- artifact: `SaboteurEnhanced_ASI_0.29_SCALEFORM_UI_CACHE_TEST_x86`
- ZIP SHA-256:
  `e5d862bffab7e3510e2d38740a5fade0e9f3d1b125a27fee74e0625c9dbcea4f`
- ASI SHA-256:
  `89989868fa02f62e914df42154bcf084fd11dbde552ec26c9cd2ee16888d1210`
- dinput8 SHA-256:
  `befb7361d5c9591a7bf83ceed9cc579b1844d55dc678ce3d0362aa25abb59c19`

Future builds must start from 0.29.


## ASI 0.30 WSModel small-object hard-cull candidate

Status: **FUNCTIONAL TEST CANDIDATE. 0.29 remains canonical.**

Target:
- renderer-specific hard visibility cutoff for small unlisted WSModel objects;
- chosen because prior SliceQuality, DetailSystem, VeryFarScene and other
  broader distance experiments did not explain the very-near prop pop.

Retail ownership:
- WSModel constructor initializes +0xA8 = 10000 and +0xAC = 10000;
- setup uses the model size/radius metric at WSModel+0x58;
- if metric < 1.5, +0xA8 is rewritten to:
  `20 + (metric / 1.5) * 90 = 20 + 60*metric`;
- at VA 0x00638710, camera-forward depth greater than +0xA8 zeroes the render
  mask, so this is a true hard cull.

Surgical branch:
- VA 0x0063954E;
- runtime RVA 0x0023954E;
- RAW 0x0023874E;
- retail bytes `7A 1A`;
- candidate bytes `EB 1A`.

The change skips only the small-model A8 formula/store and leaves the
constructor default 10000 active. Execution rejoins the existing AC/shadow
distance path, preserving that independent calculation and x87 stack behavior.

INI:
- `Fixes.WSModelSmallObjectHardCullBypass=1`.

Scope:
- one 2-byte branch edit;
- exact retail verification before write;
- no telemetry/hook;
- no streaming/pool/LOD-table/RT change;
- all canonical 0.29 settings frozen.

Source:
- `c402b2ccb447440ea9b7a303659450608f971744` — hard-cull owner and INI/runtime integration;
- `674db8f120e2a73ba0e564d39779c5aff1ee445f` — 0.30 CI artifact label.

Validation target:
- small props should remain visible substantially farther away;
- compare especially the previous garage/workshop near-pop scenes;
- reject if there is obvious scene clutter explosion, geometry corruption,
  severe performance loss or unrelated visibility regression.


ASI 0.30 CI:
- workflow run: 203
- run ID: `37145483911`
- artifact ID: `11282033353`
- artifact: `SaboteurEnhanced_ASI_0.30_WSMODEL_SMALL_OBJECT_HARDCULL_TEST_x86`
- ZIP SHA-256:
  `a497ad329fad590ccf23fa5e56875e523cda87830deec27ad0f36954130a473b`
- `SaboteurEnhanced.asi` SHA-256:
  `addb16eb605c5b5151c0dc8f13981376e0de348712042ec7c5f6c3892be9b645`
- `dinput8.dll` SHA-256:
  `5b558de59c90c111dc25cb853c828a5b3c0fad0ef420d0a7ef878803ed58bca9`
- both binaries verified PE machine x86 / 0x14C;
- packaged documentation remains `README.txt`.

0.30 remains test-only. 0.29 remains canonical until explicit validation.


## ASI 0.30 user validation — PROMOTED TO CANONICAL

User result: **validated**.

0.30 is now the canonical ASI baseline.

Validated functional delta over 0.29:
- WSModel small-object hard-cull bypass enabled;
- exact branch RVA 0x0023954E;
- retail bytes `7A 1A`;
- canonical bytes `EB 1A`;
- preserves the independent +0xAC shadow cutoff path;
- no streaming, pool, render-target, shader-quality or LOD-table change.

Validated 0.30 test artifact:
- workflow run: 203
- run ID: `37145483911`
- artifact ID: `11282033353`
- artifact: `SaboteurEnhanced_ASI_0.30_WSMODEL_SMALL_OBJECT_HARDCULL_TEST_x86`
- ZIP SHA-256:
  `a497ad329fad590ccf23fa5e56875e523cda87830deec27ad0f36954130a473b`
- ASI SHA-256:
  `addb16eb605c5b5151c0dc8f13981376e0de348712042ec7c5f6c3892be9b645`
- dinput8 SHA-256:
  `5b558de59c90c111dc25cb853c828a5b3c0fad0ef420d0a7ef878803ed58bca9`

Future builds must start from 0.30 and preserve the hard-cull bypass unless an
explicit isolated rollback is requested.


ASI 0.30 canonical CI:
- workflow run: 206
- run ID: `37146342807`
- artifact ID: `11281869894`
- artifact: `SaboteurEnhanced_ASI_0.30_CANONICAL_x86`
- artifact ZIP SHA-256:
  `1436c8aae5af91315064ddf4522449f7cf4f60c57a961b5610d8b0f1b3e62ff8`
- `SaboteurEnhanced.asi` SHA-256:
  `784b32a05ee4dd941ebcfaaa6e9b1485cb9080c0bbcbb359bfcc17cc7ea46706`
- `dinput8.dll` SHA-256:
  `a3a33c29115cf0cc5402e4ee9f95aad259818c6bec16b5999631fa9d1c5a5f0d`
- both binaries verified PE machine x86 / 0x14C;
- package keeps `README.txt`.

This canonical artifact is the distribution baseline for all future 0.31+ work.


## ASI 0.31 WSModel +0xAC byte-audit diagnostic

Status: **DIAGNOSTIC ONLY. ASI 0.30 remains canonical.**

Reason for this step:
- 0.30 validated the independent WSModel +0xA8 hard-render cutoff bypass;
- historical V310/V311 already validated both automatic and explicit RenderSlice full-mask corrections;
- the separate WSModel +0xAC size-derived shadow-distance path has never been isolated in a functional build;
- existing documentation proves the formula semantics but does not preserve enough exact retail instruction bytes to patch the +0xAC path safely without guessing.

0.31 therefore makes no new renderer decision. It captures the untouched Core1
instruction stream before 0.30/V310/V311 modify the WSModel setup area.

Logged byte window:
- runtime RVA start: 0x00239540
- runtime RVA end: 0x002395BF
- length: 0x80 / 128 bytes
- log prefix: [AUDIT]

Known semantics to correlate with the dump:
- constructor WSModel+0xAC = 10000;
- for metric < 5.0 retail derives AC = 15 + 20*metric;
- the visibility consumer treats AC independently from A8 and clears the
  secondary low-nibble/shadow mask when the camera-forward depth exceeds AC.

The byte audit is executed before ApplyWsModelSmallObjectHardCullBypass and
before V310/V311 runtime writes. The audit function itself performs no writes.

Source commit:
- e4b7ec77d2b6340c705c1018330aebd7246d8343

CI:
- workflow: Build Core ASI x86
- run: 208
- run ID: 37151980715
- conclusion: success
- artifact ID: 11283699752
- artifact: SaboteurEnhanced_ASI_0.31_WSMODEL_AC_BYTE_AUDIT_x86
- artifact ZIP SHA-256:
  3fd19404767a7d7fde855f843743b45c44b953f4b3b9084eff9a497bcf5233c5
- SaboteurEnhanced.asi SHA-256:
  45ebc48111b668456061def857c9273c9259cb20b92a2ec3fd350b6aa2f40fbf
- dinput8.dll SHA-256:
  1b9dc983d8ef8d97022c574fded72207f33500bc7332cafb5067d550d0a97534
- both binaries verified PE machine x86 / 0x14C.

Next step after user runtime log:
1. disassemble the [AUDIT] window;
2. identify exact +0xAC comparison/branch/store sequence;
3. prove stack/x87 rejoin behavior;
4. build one isolated +0xAC A/B from canonical 0.30 if and only if the patch can
   preserve all unrelated WSModel setup behavior.

0.30 remains the canonical rollback and functional baseline.


## ASI 0.32 functional candidate - WSModel +0xAC and DepthBlur signature repair

Status: **FUNCTIONAL TEST CANDIDATE. ASI 0.30 remains canonical.**

### Trigger from 0.31 runtime log

The diagnostic-only 0.31 log confirmed the untouched Core1 WSModel setup bytes:

- RVA 0x00239577 = `75 1A`;
- the +0xAC formula/store occupies RVA 0x00239579..0x00239590;
- RVA 0x00239593 = `DD D8` / `fstp st(0)`.

Local disassembly proves that the conditional path which skips the formula
already lands on the native x87 cleanup. Therefore forcing the branch does not
leave an extra x87 value live.

Functional 0.32 edit:

- RVA: 0x00239577
- retail/Core1: `75 1A`
- 0.32: `EB 1A`
- effect: retain constructor WSModel+0xAC = 10000 instead of applying the
  size-derived approximately `15 + 20*metric` cutoff for small models.

The independent validated 0.30 +0xA8 bypass at RVA 0x0023954E remains enabled.

### DepthBlur cumulative invariant repair

The same 0.31 runtime log also showed:

- DepthBlur mask shader tap group: `[SKIP]`;
- DepthBlur color texel-offset group: `[SKIP]`.

This contradicts the intended cumulative 0.28+ lineage. Core1 reconstruction
does not retain broad post-process shader edits in this region, so 0.32 removes
the fragile fixed-RVA assumption instead of adding another diagnostic build.

0.32 runtime ownership rules:

Mask signature:
- one narrow search window: RVA 0x00D66000..0x00D69000;
- require all 11 retail floats across both known mask shaders;
- second shader anchor is +0x2D8 from the first;
- require exactly one combined match before writing.

Color signature:
- one narrow search window: RVA 0x00D65800..0x00D68800;
- require both retail -1/+1 pairs together;
- second pair is +0x210 from the first;
- require exactly one combined match before writing.

Fail-closed behavior:
- zero matches: no write;
- more than one match: no write;
- only one complete signature: patch the existing validated compensation
  values.

Retained values:
- DepthBlurMaskTapOffsetScale = 0.5;
- DepthBlurColorTexelOffsetScale = 0.6666667.

### Scope

0.31 byte logging is removed from the source and is not part of 0.32.
No new telemetry is added.
All canonical 0.30 features, V310/V311 fixes, Scaleform caches, graphics,
distance, AO, shadow, post-process and streaming settings are otherwise
preserved.

Artifact label:
`SaboteurEnhanced_ASI_0.32_WSMODEL_SHADOW_DEPTHBLUR_REPAIR_TEST_x86`

0.30 remains canonical until explicit in-game validation of 0.32.
