# The Saboteur ASI 0.54 - Verified fallback owner/template memory probe

## Real 0.53 runtime result (14 October 2026)
User-provided `SaboteurEnhanced(4).log`: 770 lines,
session 22:38:43.904 through 22:39:44.327.
Both verified hooks installed; no [SKIP]/[FAIL] hook errors.
F9 at 22:39:39.788: **78** fallback-gate entries,
**77** with nonzero EBX, **16** native calls and **16** recent
same-thread gate/call correlations. F8 was pressed *after*
call #16; no additional call before F9. Normal ASI unload
at 22:39:44.327. No red-prop visual result was supplied.

More important than time proximity, **two exact identities**
held on 16/16 individually parsed completed events:
1. `EBX at native call == EBX at preceding gate + 0xD78`.
2. `native argument 0 == gate template
   [[gateEBX+0x1384]+0x2DC]`.

Concrete evidence:
- Call 1: nativeEBX 0x38DCB978, gateEBX 0x38DCAC00
  (difference 0xD78); owner 0x0C60A960;
  template and arg0 both 0x0B842520.
- Call 4: nativeEBX 0x3317D218, gateEBX 0x3317C4A0
  (difference 0xD78); owner 0x0C5165A0;
  template and arg0 both 0x0B82DA50.
- All 16 captured calls meet both equalities. They also
  share the gate's thread and a zero-ms GetTickCount age.
- This demonstrates a repeatable **actor and template identity
  relationship**, not proof of a material, shader or red color.
- The prior nativeEBX+0x1384 reads in 0.52 and 0.53 were
  from an internal pointer offset and often reported 0xAAAAAAAA,
  0xDDDDDDDD or nonsense. Those guessed nativeEBX owner
  dereferences have been removed entirely in 0.54.
- `EAX=0` still has unverified return semantics.

## New 0.54 instrumentation

Passive x86 diagnostic only; normal fallback and native
creation/forwarding unchanged. Preserve strict RVA and signature
checks from 0.53 at 0x00092E03 and 0x00092F38.

For each captured CALL with a *fresh same-thread* gate record,
check both relationships at runtime; count matches
and failures for F9.

Only when both identities match and the guarded gate owner
pointer is nonzero:
- snapshot the real fallback **owner** and **template**
  first 256 bytes BEFORE the genuine native function;
- snapshot the exact same objects AFTER the function, only
  if the actor owner and owner template pointers still
  refer to identical original objects;
- emit offsets/old/new words when changed, report separate
  unreadable or identity-change cases;
- dump the full BEFORE prefix once for each newly seen
  valid template per thread (max 8);
- keep earlier ECX/ARG0/ARG1/EDI and child-pointer diagnostics.

No guessed `nativeEBX+0x1384` dereferences.
No shaders, textures, objects, scene distances, render
parameters, game EXE, gameplay HUD or engine limits are
modified by this test. This is a diagnostic candidate,
**not a validated red-material correction**.

Hotkeys: AUTO detailed first 16 calls at startup;
F6 quiet; F7 capture next 16 calls; F8 next 64;
F9 latest snapshot plus verified-link totals.

## In-game validation

Install a flat x86 ZIP (`dinput8.dll`, `SaboteurEnhanced.asi`,
`SaboteurEnhanced.ini`, `README.txt`) at game root.
Visit a known occupied-district reproduction site.
Press F8 **before** spawning/approaching new civilians,
then F9. Send SaboteurEnhanced.log, noting whether
the red accessories remain and whether normal hand props
are preserved. Roll back to Core 1 + validated ASI 0.45
with matching INI if any instability occurs.

## Publication state
Source branch `dev/asi-0.54-verified-fallback-owner-template`.
Windows x86 CI and prerelease are pending until compiled;
do not claim success based solely on this document.
