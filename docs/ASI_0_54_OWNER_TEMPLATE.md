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


## ASI 0.53 user log verified -> 0.54 native ownership probe (14 October 2026)

User log `SaboteurEnhanced(4).log` (770 lines):
- Two exact hooks installed: fallback gate RVA 0x00092E03,
  native creator CALL RVA 0x00092F38 -> 0x00588B30.
- Gate count 78, with 77 nonzero EBX; native call count 16.
  All 16 native calls correlate with a fresh same-thread gate.
- **100% of 16 call events** have
  nativeEBX = fallbackGateEBX + **0xD78** AND
  nativeArg0 = fallbackGateTemplate.
  This is strongly corroborated by gate owner+template and the
  immediate call arguments, not proof of a rendered/material object.
- Exactly two distinct templates appeared among the 16 native
  calls: template 0x0B842520 linked to owner 0x0C60A960,
  and template 0x0B82DA50 linked to owner 0x0C5165A0.
  These absolute addresses are session-specific, not stable offsets.
- F8 arrived after native #16; F9 replayed #16. Normal
  ASI unload; visual red accessory outcome not reported.
- The previously guessed owner dereference nativeEBX+0x1384
  is invalid (it reads an interior address); RETIRED in 0.54.

0.54 observes guarded 256-byte before/after native-call snapshots
from precisely gate-proven owner and template, requiring per-event
actor +0xD78 AND template==ARG0 identities. It logs first-seen
template prefixes and link match/mismatch counts; does not
write them back, touch materials/shaders, change props or affect
EXE/graphics/HUD/LOD/engine limits. It preserves the normal
fallback and native CALL semantics. Only tested by compilation,
**not yet validated in-game or as a red-bug correction**.
Rollback: validated Core 1 + ASI 0.45 with matching INI.

Published TEST prerelease ASI-0.54-TEST:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/tag/ASI-0.54-TEST
Direct flat ZIP:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/download/ASI-0.54-TEST/SaboteurEnhanced_ASI_0.54_VERIFIED_OWNER_TEMPLATE_TEST_x86.zip
SHA-256 ZIP: ade596c6e4545f3e99efe0fb6d981bb6d8e5545139f950ceebd3b3ae8e885649
ASIx86: FF1CABD6FB575EA632403FBB6ED1497394F6F5185899F6595CEE44389180F276
Loaderx86: AD099393F655C10C1B3DFF80D7D9B551C8AA6FCFAB973DA152EA540F9E9020EC
GitHub CI #256: https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/38634986820
CI artifact ID 11926664618, SHA-256 c4f7ee9e835153c8b6710405dc546d6ea32539fc2bd363266be7c914ee9e6343.
Flat ZIP validated by CI: dinput8.dll, SaboteurEnhanced.asi,
SaboteurEnhanced.ini, README.txt directly at the archive root.
