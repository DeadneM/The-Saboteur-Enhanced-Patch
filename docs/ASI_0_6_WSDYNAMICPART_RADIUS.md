# ASI 0.6 WSDynamicPart priority radius A/B

Status: **test candidate**

## Baseline

- Core 1 validated SHA-256:
  `83995ab6e04fce264309a37a243744d4f5929c5abb87c3f195fb50fef7f5f2a8`
- V310 default-on runtime fix retained
- V311 default-on runtime fix retained
- ASI 0.5 rejected and disabled

## ASI 0.5 rejection

User result: **no visible change** to the balcony/facade transition.

Static follow-up identified the virtual float used by that branch as
`WSDamageable::slot9` at VA `0x00451360`:

```asm
fld dword ptr [ecx+0x0C]
ret
```

This is an internal damageable-state float, not a camera-distance/LOD metric.
The 0.5 child-visibility bypass is therefore rejected.

## WSDynamicPart priority formula

The destructible/repeated-facade path reaches `WSDynamicPart::Update`, which
calls the priority function at VA `0x00669980`.

That function computes a score containing:

```text
max(625 - x^2, 0)
```

Therefore the native proximity radius represented by that term is:

```text
sqrt(625) = 25
```

The result is added to WSDynamicPart fields at `+0x200` and `+0x204`, plus
an optional `10000.0` priority term. WSDynamicPartManager compares these
scores when selecting the current/active dynamic part.

## Constant ownership

Full executable disassembly xref audit found:

- VA `0x00FC76DC` / RAW `0x00BC68DC`
  - float `625.0f`
  - bytes `00 40 1C 44`
  - exactly one xref, from `0x00669980`

- VA `0x00FC77C8` / RAW `0x00BC69C8`
  - double `625.0`
  - bytes `00 00 00 00 00 88 83 40`
  - exactly two xrefs, both from `0x00669980`

No other function references either constant.

## A/B

ASI 0.6 sets:

```ini
WSDynamicPartPriorityRadius=50
```

Runtime patch:

```text
radius 25 -> 50
radius^2 625 -> 2500

625.0f -> 2500.0f
625.0  -> 2500.0
```

The native scoring formula is otherwise unchanged.

Rejected 0.5 branch:
```ini
OdinChildVisibilityGate=0
```

Diagnostics:
```ini
OdinInstancing=0
```

## Build hashes

- `SaboteurEnhanced.asi`:
  `8ed0474d3bc5c6c9c633fae4bcfdb6473575df7f27d51c93e9c985a3d494e1f7`
- `dinput8.dll`:
  `7f1b42c61267805fb413a9e6c864111c0c562fd389ea7ce27dac826af2949de5`
- five-file package ZIP:
  `e064bd8ab3053e3ff5ff33e33cdf1b738d2bc87580d7b5a3428e1e9c46e0d3b7`

Both runtime binaries are x86.

## Test

No marker, log capture or diagnostic sequence is required.

Visit the known balcony/facade location and compare the transition.

If the transition moves farther away, the 25-unit WSDynamicPart priority radius
is a controlling value and can be refined.

If it does not move, reject this radius and continue statically from the
WSDynamicPart manager selection path without returning to broad runtime logging.
