# SaboteurEnhanced ASI

## Current diagnostic build: 0.4 — Win32Mesh fingerprint

Validated baseline retained:
- Core 1
- V310 WSModel automatic RenderSlice full-mask correction
- V311 explicit ModelInfo RenderSlice full-mask correction
- x86 dinput8 proxy loader

## Result of ASI 0.3

The F9 correlation test produced a repeatable directional pattern.

The same group of six Odin/Win32Mesh roots was observed immediately before
markers #2, #4, #6 and #8.

Markers #5 and #7 were preceded by the same three Odin instances, including
two instances sharing one root.

This is strong evidence that the Odin synchronization path is correlated with
the target balcony/facade transition rather than being unrelated scene noise.

The root vtable seen in every correlated event is:

`0x0108198C`

Static RTTI/vtable recovery identifies it as **Win32Mesh**, derived from
OdinMesh.

## What 0.4 adds

ASI 0.4 keeps the 0.3 synchronization detour and F9 marker.

For every unique Win32Mesh root observed during the configurable time window
before F9, it logs:

- root pointer
- Win32Mesh vtable
- stable structural fingerprint
- raw memory fingerprint
- last OdinMeshInstance pointer
- caller batch
- entry count
- segment count
- first 0xC0 bytes of the root object as dwords
- printable strings reachable through direct root fields
- AHSM/MSHA mesh names when a direct field points to an AHSM header

The purpose is to replace session-local heap addresses with an asset/resource
signature that can survive a game restart.

No render or streaming decision is changed.

## Test

Use the same balcony:

1. approach until the detail appears;
2. press F9;
3. move away until it disappears;
4. press F9;
5. repeat 2–3 times;
6. quit normally;
7. send SaboteurEnhanced.log.

Useful new lines:
- `[FPRINT]`
- `[FPRINT-DW]`
- `[FPRINT-STR]`
- `[FPRINT-NAME]`

No tuner.txt modification is used.
