# ASI 0.55 - Normal WSCivilianProp lookup vs fallback (diagnostic)

## 0.54 actual player result, 14 October 2026
**Red bug STILL visible.** Loaded ASI 0.54 and hooks:
native CALL RVA 00092F38 -> RVA 00588B30, fallback RVA 00092E03.
User log `SaboteurEnhanced(5).log` ended in normal ASI unload.
15 native calls detailed, 15/15 actor+0xD78 and
ARG0==gate-template relationships verified, all 15 owner/template
identities stable before/after. All first 256-byte owner blocks
showed 0 modifications. Every one of the 15 template blocks
changed only `+0x48` by incrementing a small counter
(15/15), consistent with a use/reference counter, NOT
identified as color, shader or material.

Exactly two distinct snapshot templates (in that single session):
- 0x0B8E2520, owner 0x0C6AA960, five calls.
- 0x0B8CDA50, owner 0x0C5B65A0, ten calls.
Addresses are process-specific, not universal game offsets.
Their 256-byte prefixes contain `!Is_LowWtf` and
either `!Is_Raining` or `Is_Raining`.
These appear to be conditional flags in data; neither
is a proven material field or evidence of root cause.
No F9 keypress appears in this log; do not invent F9 stats.

## Why 0.55, not another speculative material modification
The 0.47 proxy bypass removed red proxies but also
pedestrian hand props. Previous direct calls and template
comparisons proved identity but did not distinguish
a missing normal-prop lookup from an incorrectly displayed
fallback. The next decisive evidence is lookup success/failure.

## New strictly checked real-lookup hook
- Native RVA 0x00092DD5; verify 11 original bytes:
  `8B CD 85 C9 74 28 83 39 00 74 23`.
- Original 6-byte instruction sequence:
  `mov ecx,ebp; test ecx,ecx; je RVA 0x00092E03`.
  Hook replays exactly these semantics and returns to
  `0x00092DDB` on non-null EBP. No replacement data.
  If signature mismatch, skip the hook, do not patch blindly.
- Observe EBP result node and read `[EBP]` guarded.
  Classify result as 0=null, 1=unreadable,
  2=empty, 3=populated. For readable populated
  node, optionally inspect the first 64 bytes of
  its target pointer, with read safety and logging caps.
- Compare the latest SAME-THREAD, SAME-ACTOR (EBX)
  lookup with following fallback gate (RVA 0x00092E03),
  counting matching vs other fallback visits.
  This correlation is evidence of control flow, not a
  material fix. No global data/renderer changes.
- Retain old verified native call and owner/template
  diagnostics on a limited 16-first-call sample. F8
  captures next 64 native calls; F9 dumps counters for
  normal lookups, gate and native calls.
- Hook optional and fail-open when byte signature differs.

## Excluded
No shader, color, lighting, rain, LOD, pool-limit,
culling, actor or prop overwrite. No EXE changes.
Main and Core1 + canonical ASI 0.45 remain unchanged.

## Runtime test
Reproduce known red accessory in black/white/red occupied
district; launch and move between distant and near civilians.
Press F8 **before** new civilians are spawned; then F9;
send log and report whether ordinary held props remain.
If game crash/regression, rollback to canonical 0.45 with
matching INI. 0.55 is a *test*, not a claimed visual fix.
