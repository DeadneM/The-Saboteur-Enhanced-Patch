# SaboteurEnhanced ASI

## Current test build: 0.6 — WSDynamicPart priority radius A/B

Validated baseline retained:
- Core 1
- V310 WSModel automatic RenderSlice full-mask correction
- V311 explicit ModelInfo RenderSlice full-mask correction
- x86 dinput8 proxy loader

## 0.5 result

ASI 0.5 had **no visible effect** on the balcony/facade pop and is rejected.

Static follow-up explains why: the virtual float used by the 0.5 branch is
WSDamageable slot 9 at VA `0x00451360`, which is simply:

```asm
fld dword ptr [ecx+0Ch]
ret
```

It is WSDamageable state, not a camera-distance or LOD metric.

## Actual value found for 0.6

The relevant destructible/repeated facade path reaches
`WSDynamicPart::Update`, which calls the priority function at VA
`0x00669980`.

That function computes a score containing the proximity term:

```text
max(625 - x², 0)
```

Therefore the native radius represented by the formula is:

```text
sqrt(625) = 25
```

The score is then combined with WSDynamicPart fields at +0x200/+0x204 and an
optional 10000-point priority term, and is compared by the WSDynamicPart manager
when selecting the active/current part.

The two 625 constants are local to this one function. Static xref audit found:

```text
VA 0x00FC76DC   float 625.0   one reference
VA 0x00FC77C8   double 625.0  two references
```

All references come from VA `0x00669980`.

## A/B patch

ASI 0.6 sets:

```ini
WSDynamicPartPriorityRadius=50
```

At runtime it writes both constants coherently:

```text
625.0  -> 2500.0
25²    -> 50²
```

This preserves the native scoring formula and merely doubles the radius over
which the proximity term contributes.

No Odin diagnostic hooks are enabled and the rejected 0.5 branch is disabled.

## Test

No logging procedure or hotkey is required.

Go to the same balcony/facade location and check whether:
- the late detail appears farther away or no longer visibly pops;
- nearby destructible/repeated facade pieces remain sane;
- performance remains normal.

If it changes the balcony, the radius is a real controlling value and can be
refined. If it does nothing, this priority radius is rejected and we move on
without more instrumentation.
