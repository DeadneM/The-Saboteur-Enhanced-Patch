# Technical notebook

## Current canonical cumulative build

**V293**

Original retail EXE SHA-256:  
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V279 EXE SHA-256:  
`db2ac0f79b2fcace02ac32d77bdea32c5d9591f6bee56715e9e1976f81999b0a`

Every future candidate starts from V293 or reproduces V293 exactly before adding an experiment.

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
