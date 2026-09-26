# SaboteurEnhanced ASI

Runtime half of the cleaned **The Saboteur Enhanced Patch** architecture.

## Current diagnostic build: 0.2

Validated default-on fixes retained from ASI 0.1:
- V310 WSModel automatic RenderSlice full-mask correction
- V311 explicit ModelInfo RenderSlice full-mask correction

New pass-through diagnostic:
- OdinMeshInstance vtable verification
- ReInstance trace
- recovered RemoveHighResSegments/query trace
- PreRelease trace
- IsFullyLoaded trace
- per-object state-change filtering
- caller RVA capture
- event-count safety cap

ASI 0.2 does **not** suppress Odin calls and does **not** change Odin rendering
behavior. It only replaces four verified virtual-method entries with wrappers
that call the original methods and record their behavior.

Static finding behind the diagnostic:
- recovered `OdinMeshInstance::RemoveHighResSegments` at VA `0x00E14BA0`
  is only `mov al,[ecx+33h] ; ret`;
- therefore it behaves as a state/query accessor in the PC executable rather
  than a large destructive routine;
- `OdinMeshInstance::ReInstance` at VA `0x00E14BF0` is the non-trivial
  re-instancing path;
- `IsFullyLoaded` at VA `0x00E14BD0` returns byte `this+0x44`.

## Test procedure

1. Install the five package files in the game directory.
2. Launch the game and reproduce the street/balcony transition.
3. Move toward and away from the transition several times.
4. Exit normally.
5. Send `SaboteurEnhanced.log`.

Useful log lines begin with `[ODIN]`.

## Loader

The bundled `dinput8.dll` is an x86 proxy loader. It forwards the standard
DirectInput8 exports to the real System32 `dinput8.dll` and loads
`SaboteurEnhanced.asi` from the game directory.

No `tuner.txt` modifications are used.

Build target: Win32 / x86.
