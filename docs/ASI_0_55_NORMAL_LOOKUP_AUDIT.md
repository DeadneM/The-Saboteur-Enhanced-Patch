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


## ASI 0.54 verified player log / 0.55 x86 published prerelease (2026-10-14)

Player confirmed **red textures still visible** using 0.54. User log
`SaboteurEnhanced(5).log`: 15 distinct native completions,
15/15 gate+0xD78 matches, 15/15 template equals native ARG0,
15/15 valid/unchanged owner and template pointer identity.
Owner first 256 bytes: **0/15 modified**. Template first
256 bytes: **only +0x48 incremented, all 15/15**.
Two process-local template pointers, family A 0x0B8E2520
(5 events) and B 0x0B8CDA50 (10 events). Templates contain
literal strings `!Is_LowWtf` and `!Is_Raining` /
`Is_Raining`. Their meaning here is UNPROVEN, and these
are not discovered red-material offsets.
Normal ASI unload at 23:04:32.176; no F9 entry in this log.
Nothing in 0.54 modifies rendering, so persistent red is
consistent with being an observational build, not surprising
evidence of incorrect 0.54 instrumentation.

**0.55 pivots to a DIFFERENT original native path**:
`RVA 0x00092DD5` where original read-only runtime bytes
were `8B CD 85 C9 74 28 83 39 00 74 23`.
Verified hook uses 6 bytes (E9 rel32 + NOP) and faithfully
replays `mov ecx,ebp; test ecx,ecx; je 0x92E03`;
non-null falls through to original `cmp [ecx],0` at
`RVA 0x00092DDB`. Logs whether the **normal** table
lookup is null/unreadable/empty/populated, copies a
populated resource header when safely readable, and
correlates fallback gate entries with last same-thread,
same-actor lookup. F9 summarizes normal lookup classes
plus old fallback/native counts. No fixes, material edits,
false skipping of props or changes to graphics, EXE,
LOD, HUD or engine limits.

0.55 build Windows MSVC x86, GitHub Actions #257 SUCCESS:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/38637887900
Build source commit `8d17f253c21c32798e72909b739a311588c4cbc8`.
ASI SHA-256:
`3B2CD2B026902C0177F339BF6F3CF300704F7562091CE4A2F1028174206568AE`.
Loader SHA-256:
`595F0D21B0186EC64D03BABC377F66B628C7E19001811C53451D8AD9B6D6F694`.
Experimental prerelease:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/tag/ASI-0.55-TEST
Direct root-level ZIP:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/download/ASI-0.55-TEST/SaboteurEnhanced_ASI_0.55_NORMAL_LOOKUP_VS_FALLBACK_TEST_x86.zip
Size 190090 bytes; SHA-256
`b80930a2c6f35cc255aaf2780b3c7fcad501f6efcbd3a9927ef09c53150de982`.
Artifact ID 11928048765, sha256
`eed5c4b94833b1cd49c07828375f0fd2a430710d80410866322abfd49684c26c`.
CI verified flat ZIP exactly dinput8.dll, SaboteurEnhanced.asi,
SaboteurEnhanced.ini, README.txt in root.
Version 0.55 is untested in-game; Core1 + 0.45
remains validated baseline, and **main untouched**.
Test: red district, F8 BEFORE new props load, F9 for
normal-vs-fallback totals, send log; confirm visible red
and normal held prop behavior, rollback 0.45 on issue.
