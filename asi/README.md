# SaboteurEnhanced ASI

## Current test build: 0.5 — Odin child visibility A/B

Validated baseline retained:
- Core 1
- V310 WSModel automatic RenderSlice full-mask correction
- V311 explicit ModelInfo RenderSlice full-mask correction
- x86 dinput8 proxy loader

## Why the diagnostic branch stops here

ASI 0.3 already established a repeatable correlation between the remaining
balcony/facade transition and the Odin/Win32Mesh path.

Static follow-up now identifies the actual boolean gate used by that path.

At VA `0x00667C59` the engine calls a virtual method that returns a float.
At VA `0x00667C60` that float is compared against the exact `0.0f` constant
at VA `0x010A45B0`.

A local boolean is then propagated through the repeat/instancing hierarchy.
At VA `0x00667CC7` the zero-result state rejects the child mesh:

```asm
cmp byte ptr [esp+20h], 0
je  invisible
cmp byte ptr [esp+24h], 0
jne invisible              ; 75 04
mov al, 1
jmp done
invisible:
xor al, al
```

ASI 0.5 changes only:

```text
VA 0x00667CC7
75 04 -> 90 90
```

The parent-visible test remains untouched. The patch therefore does not make
all Odin meshes globally visible. It only ignores this one child rejection
caused by the zero-result gate.

The broad SliceQuality tables are not the target. V302 had already expanded
those ranges massively and the balcony still transitioned late.

## Test

No F9 and no diagnostic procedure.

Simply reproduce the balcony/facade location and check whether the late
appearance/disappearance is gone or pushed away.

Also watch for regressions on repeated facade details, windows, balconies,
street dressing and performance.

If this fixes the balcony cleanly, the next step is to identify the upstream
float producer and turn the A/B into a proper distance/LOD policy rather than
keeping a forced branch bypass.
