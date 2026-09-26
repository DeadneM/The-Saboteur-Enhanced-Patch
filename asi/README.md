# SaboteurEnhanced ASI

Runtime half of the cleaned **The Saboteur Enhanced Patch** architecture.

## Current diagnostic build: 0.3

Validated default-on fixes:
- V310 WSModel automatic RenderSlice full-mask correction
- V311 explicit ModelInfo RenderSlice full-mask correction

### Why 0.3 exists

ASI 0.2 proved that:
- recovered `RemoveHighResSegments` was never queried during the balcony run;
- `PreRelease` was never called;
- `IsFullyLoaded` was queried heavily;
- `ReInstance` ran in scene-wide bursts;
- those calls all came from the same internal OdinMeshInstance synchronization
  routine at VA `0x00E14610`.

Static disassembly of that routine shows the exact gate:

1. obtain the instance root;
2. query root virtual slot 2;
3. compare it with the instance loaded-state byte at `this+0x44`;
4. call `ReInstance` only when the two states differ.

ASI 0.3 hooks that synchronization routine directly.

### Logged data

Only the meaningful mismatch path is logged:

- OdinMeshInstance pointer
- which of the three known engine call sites invoked the sync
- entry-list pointer and count
- root pointer and root vtable
- root loaded state
- instance loaded state
- `this+0x2A`
- `this+0x35`
- segment count
- before/after state around the original routine

No Odin decision is changed. The original function is always executed.

### F9 correlation marker

Press **F9** exactly when the balcony/detail appears or disappears.

The ASI writes:

```text
[MARK] F9 #1
```

into `SaboteurEnhanced.log`.

This lets the next audit correlate the visual transition against Odin mismatch
events without guessing from timestamps.

### Test procedure

1. Install the five package files in the game directory.
2. Go to the known street/balcony location.
3. Approach until the detail changes, then press **F9**.
4. Move away until it changes back, then press **F9**.
5. Repeat at least 3 times.
6. Exit normally.
7. Send `SaboteurEnhanced.log`.

The bundled `dinput8.dll` remains the x86 proxy loader and no `tuner.txt`
modification is used.
