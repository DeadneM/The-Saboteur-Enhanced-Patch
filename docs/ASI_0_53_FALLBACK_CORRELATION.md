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
