# ASI 0.2 Odin / instancing diagnostic

Status: **test candidate**

Baseline:
- Core 1 validated EXE SHA-256: `83995ab6e04fce264309a37a243744d4f5929c5abb87c3f195fb50fef7f5f2a8`
- ASI 0.1 architecture already validated in game
- V310 and V311 remain default-on runtime fixes

## Objective

Investigate the remaining late balcony/facade transition that survived the
validated WSModel RenderSlice fixes and the rejected VeryFarScene /
WSFarSceneObject / ModelInfo ZCULL diagnostics.

## Static finding

Recovered `OdinMeshInstance` vtable:
- VA `0x01081898`
- slot 0 -> `0x00E14C10` destructor
- slot 1 -> `0x00E14BF0` ReInstance
- slot 2 -> `0x00E0C460`
- slot 3 -> `0x00E14BE0` GetRoot
- slot 4 -> `0x00E14BB0` IsInstance
- slot 5 -> `0x00E14BC0` GetMeshInstance
- slot 6 -> `0x00E14BA0` recovered RemoveHighResSegments
- slot 7 -> `0x00E14420` Release
- slot 8 -> `0x00E143D0` PreRelease
- slot 9 -> `0x00E14BD0` IsFullyLoaded

Important correction:

`0x00E14BA0` disassembles to only:

```asm
mov al,[ecx+33h]
ret
```

Therefore the recovered `RemoveHighResSegments` entry behaves like a
state/query accessor in the PC executable. ASI 0.2 deliberately does **not**
neutralize it.

`OdinMeshInstance::ReInstance` at `0x00E14BF0` is non-trivial and calls the
instance rebuild path.

`IsFullyLoaded` at `0x00E14BD0` reads byte `this+0x44`.

## Diagnostic hooks

ASI 0.2 verifies the full 10-entry vtable before changing any slot.

Pass-through hooks:
- slot 1 ReInstance
- slot 6 recovered RemoveHighResSegments/query
- slot 8 PreRelease
- slot 9 IsFullyLoaded

The original method is always called and its return value is preserved.

Recorded fields:
- object pointer
- caller RVA in Saboteur.exe
- segment pointer at `this+0x14`
- segment count at `this+0x28`
- byte at `this+0x33`
- fields `this+0x38/+0x3C/+0x40`
- loaded byte at `this+0x44`
- per-object query changes
- shutdown summary call counts

Default filtering:
- first observation is logged
- state changes are logged
- repetitive getter calls are suppressed
- `OdinTraceAllQueries=1` can disable filtering
- `OdinEventLimit=5000` caps diagnostic output

## INI

```ini
[Fixes]
WSModelFullRenderMask=1
ModelInfoFullRenderSlice=1

[Diagnostics]
OdinInstancing=1
OdinTraceAllQueries=0
OdinEventLimit=5000
PoolTelemetry=0
```

## Build

GitHub Actions Win32/x86 build passed.

- `SaboteurEnhanced.asi` SHA-256:
  `9ee3d7e8f977c6c7c63d6f4548a8f0e4742dfdaf377b038f22700dfb48ed5556`
- `dinput8.dll` SHA-256:
  `60c887b42c0be72a975af10862a9ddeca23c4ec5e348b62583f9b88c4c2b8241`
- assembled test ZIP SHA-256:
  `6e6228a03e9dd748f1b03307d0fa7e2b89185919e6388e78ee40768fbddf85dd`

Both binaries were verified as PE machine `0x14C` / x86.

## Test protocol

At the known street/balcony location:
1. move toward the balcony until the delayed detail appears;
2. move away until it disappears;
3. repeat several times;
4. exit normally;
5. inspect/send `SaboteurEnhanced.log`.

Useful lines begin with `[ODIN]`.

Decision rule:
- correlation with `+0x33`, `+0x44` or ReInstance events -> follow the proven
  Odin object path to the exact high-resolution retention gate;
- no correlation -> reject this OdinMeshInstance lifecycle hypothesis and move
  to the next renderer ownership layer.


## Runtime result

ASI 0.2 was tested in game at the balcony/facade location.

Observed totals:
- ReInstance: **723**
- distinct ReInstance objects: **614**
- RemoveHighResSegments/query calls: **0**
- PreRelease calls: **0**
- IsFullyLoaded/query calls: **2729**
- logged ODIN events: **2661**

All first-state IsFullyLoaded observations returned 0.

ReInstance occurred in broad scene-update bursts, not as a uniquely isolated
single-object event. Every ReInstance hook entry was called from the same
internal OdinMeshInstance synchronization path.

Static follow-up identified that path at VA `0x00E14610`.

The routine:
1. obtains the instance root;
2. calls root virtual slot 2;
3. compares that state against the OdinMeshInstance loaded byte at `this+0x44`;
4. calls virtual slot 1 / ReInstance when the states differ.

Its only direct callers found in the executable are:
- VA `0x0066BA4A`, return RVA `0x0026BA4F`
- VA `0x0066BAC8`, return RVA `0x0026BACD`
- VA `0x0066C36F`, return RVA `0x0026C374`

Conclusion:
- the broad vtable hook proved Odin activity is real but is too noisy to assign
  the balcony transition to one object;
- recovered RemoveHighResSegments/query is not participating in this test path;
- next diagnostic must trace the synchronization mismatch directly and correlate
  it with a user-supplied visual marker.
