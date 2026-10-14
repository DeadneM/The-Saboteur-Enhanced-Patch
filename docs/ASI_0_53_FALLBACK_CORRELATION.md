# The Saboteur ASI 0.53: Fallback/native chronology correlation

## Confirmed 0.52 runtime evidence (2026-10-14)
The user's `SaboteurEnhanced(3).log` shows installation of the verified
x86 native call hook, 24 total calls, 16 automatic detailed events
plus explicit event #24, and F9 replay of #24. No errors
from hooks were logged. The file ends at F9; no clean-unload or crash
conclusion is possible from this file alone.

The `ECX+0x207C` value changed on #5, #6, #7 and #24,
apparently float-like values converging to 1 in some cases.
Meaning (e.g. color) NOT established. The child pointer fields
`ECX+0x08` and `ECX+0x1C` can change object identity.
`ARG0+0x48` continues increasing on native calls.
`EBX+0x1384` sampled **at the native CALL** often has 0,
`0xDDDDDDDD`, `0x00010001` or unreadable values.
Do not infer EBX is a WSHumanSpore actor at that *callsite*.

## New instrumentation
Two exact checked in-place passive hooks:
- RVA `0x00092E03` fallback gate:
  original `85 DB 0F 84 3B 01 00 00`. Callback copies
  the **gate's EBX** to a per-thread TLS record, increments
  global gate sequence, and attempts guarded owner
  `[EBX+0x1384]` and template `[owner+0x2DC]`.
  A native `test ebx,ebx; je exit` is replayed unmodified
  in semantic effect; the gate never suppresses anything.
  Strict opcode mismatch means this second hook is SKIPPED.
- RVA `0x00092F38` native direct call to RVA `0x00588B30`:
  retains all 0.52 guarded before/after snapshots
  and native convention-preserving shim. At each call,
  attaches same-thread most recent fallback gate record,
  age in ms and fresh-event flag. A gate within 1000 ms
  is counted as *temporal proximity*, not conclusive proof
  of matching prop identity. The **native CALL EBX**
  remains separately logged and UNVERIFIED as an actor.

Only first 16 native calls are verbose automatically.
F6 quiet; F7 next 16 native; F8 next 64 native;
F9 prints total gate entries + native calls and replays
latest detailed native snapshot.

## Scope and precautions
**TEST ONLY, not the root-cause fix.** No EXE, material,
shader, LOD, HUD, graphics, prop or engine-limit values changed.
No suppressions. The historical 0.49 broad fallback hook
remains source history only and is not installed.

Canonical baseline: Core 1 + ASI 0.45 validated.
Source branch: `dev/asi-0.53-fallback-correlation`.
Expected ZIP root: loader `dinput8.dll`,
`SaboteurEnhanced.asi`, `SaboteurEnhanced.ini`,
`README.txt`.

In-game test: load occupied district where red props are
known to appear; watch props visually; press F8,
move through zone, then F9; send log. Confirm whether
red props and normal pedestrian-held props remain.
Unvalidated probe may still trigger instability;
rollback to 0.45 ASI and matching INI if so.


## ASI 0.52 user runtime result / ASI 0.53 tested compilation (14 October 2026)

0.52 `SaboteurEnhanced(3).log` confirmed: [OK] native hook;
24 calls counted; 16 automatically verbose and #24 captured after F8;
F9 replayed #24. No [SKIP]/[FAIL] messages. No ASI-unload
line; this file alone cannot establish exit conditions or a crash.
User did not report whether red texture persisted visually.

Crucial 0.52 insight: EBX as observed *at the native CALL* cannot be
assumed the WSHumanSpore actor, since owner[EBX+1384]
returned 0, 0xDDDDDDDD, 0x00010001 or other unreadable/invalid values.
ECX+0x207C changed on #5, #6, #7, #24, often float-like values;
this does NOT prove a color/material field. ECX child pointers changed
identities; same-object comparison must not cross pointer transitions.
EAX remains zero; a successful real proxy creation has NOT been proven
solely by completed native calls.

0.53 pivots from speculative field offsets to passive two-site
**temporal correlation**. New exact-signature-verified fallback gate at
RVA 0x00092E03 faithfully replays native TEST EBX / JE behavior,
without ever suppressing the fallback. It snapshots the gate's own EBX,
owner and template if readable, and per-thread sequence/timestamp.
The existing native CALL hook at RVA 0x00092F38 still captures 0.52
data, but labels its own EBX as unverified and attaches last
same-thread fallback sequence/age. A new 1-second proximity counter
is diagnostic only and NEVER proves identical object ownership.
F9 logs both call and fallback counters. No EXE/HUD/engine/material
parameters changed. Stable baseline remains Core 1 + ASI 0.45.

**CI build SUCCESS** GitHub Actions #255 (Windows MSVC x86):
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/38607768693
Source code commit `3b0f02eea1ccf2f96db8ff25f444e51e6370281f`.
ASI SHA-256:
`A6140FA767B3F33AB34986856B0DAB636520B1580ED542067AF6373CF33800BE`
Loader SHA-256:
`6C5CFF053270EEC5F33C2B96A2DB46A568B2AB32DED371C6E769351A9F7A6266`
Prerelase tag `ASI-0.53-TEST`:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/tag/ASI-0.53-TEST
Direct flat-root test ZIP:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/download/ASI-0.53-TEST/SaboteurEnhanced_ASI_0.53_FALLBACK_NATIVE_CORRELATION_TEST_x86.zip
ZIP size 187609 bytes, SHA-256
`9909fdb9190e054ff17a9574ad32ac0adab41d3c0d992d28ad36539fe2f064f2`.
CI artifact ID 11915733861, digest
`5204d7cd487977111cc1eaac6f752d0f5628ac768a72e9c7a5b0c9a9cdf61d9c`.
Flat ZIP verified at build time by ZIP entry-name comparison;
exact root members: dinput8.dll, SaboteurEnhanced.asi,
SaboteurEnhanced.ini, README.txt.

Do not promote 0.53 to main/stable before the user tests game
startup, red occupied zone, pedestrian-held accessories, and
fallback/native logs. Roll back to stable 0.45 ASI + INI on issues.
