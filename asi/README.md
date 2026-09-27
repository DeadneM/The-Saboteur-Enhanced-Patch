# SaboteurEnhanced ASI

## Current test build: 0.6 — WSDamageablePart variant selector A/B

Validated baseline retained:
- Core 1
- V310 WSModel automatic RenderSlice full-mask correction
- V311 explicit ModelInfo RenderSlice full-mask correction
- x86 dinput8 proxy loader

## Result of 0.5

ASI 0.5 changed only the child rejection branch at VA `0x00667CC7`.

User result: **no visible change to the balcony/facade transition**.

Conclusion:
- the 0.0f child gate is on the same Odin/repeat path but is not the final
  selector responsible for the observed pop;
- ASI 0.5 is rejected and disabled by default.

## New static finding

The next function in the same path is VA `0x006678E0`.

It recalculates the actual child visibility bit at `child+0x4C & 1`.

Before writing that bit, the engine selects between two child groups using:

- parent/state field `this+0x20`, compared against **0** and **1**;
- child resource flag `resource+0x28 & 1`.

The native result is then combined with the incoming visibility boolean:

```asm
...
mov eax, 1
...
xor eax, eax
and al, byte ptr [esp+24h]    ; VA 0x006679AE
mov cl, byte ptr [esi+4Ch]
...
xor byte ptr [esi+4Ch], cl
```

The surrounding object construction path is recovered as
**WSDamageable / WSDamageablePart**.

## ASI 0.6 A/B

Single runtime change:

```text
VA 0x006679AE
22 44 24 24        ; and al,[esp+24h]
->
8A 44 24 24        ; mov al,[esp+24h]
```

Effect:
- preserve the incoming visibility decision;
- bypass only the WSDamageablePart 0/1 child-variant selector;
- leave all global draw distances, RenderSlice, streaming and Odin sync logic
  untouched.

Potential visible side effect for this A/B:
- if the selector is choosing mutually exclusive damage/intact variants, both
  groups may become visible together. That would still be a useful diagnostic
  result and would immediately identify this subsystem as the owner.

## Test

No F9 and no special procedure.

At the balcony/facade location, check whether:
- the late detail pop changes;
- duplicate/overlapping facade or damage geometry appears;
- other destructible buildings show obvious variant overlap.

If there is still no effect, this WSDamageablePart selector is rejected and the
Odin correlation is treated as a downstream consequence rather than the visual
owner.
