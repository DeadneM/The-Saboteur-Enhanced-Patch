# Red Civilian Prop / Texture Bug Audit

## Symptom

In occupied black/white/red zones, some distant civilian-held metallic props can
appear as conspicuous red proxy rectangles/objects before later resolving to the
normal accessory.

## Proven ownership

The affected family was isolated to `PGA_HHProp_*`.

The decisive runtime proof is the WSHumanSpore fallback:
`rnd civilian prop(%d)`.

Normal path:
- table lookup at VA 0x00492D95..0x00492DCF;
- valid entry is consumed by VA 0x00492DD5..0x00492DFE.

Fallback path on lookup miss:
- VA 0x00492E03 / RVA 0x00092E03;
- fallback template `[ebx+0x1384] -> [+0x2DC]`;
- string `rnd civilian prop(%d)`;
- creation around VA 0x00492F38 -> 0x00988B30.

V243A proved that disabling only this fallback changes the distant red proxy to
no proxy, while the genuine normal prop still appears once available.

## Rejected explanations / fixes

Do not repeat:
- global HHProp shader recoloring or whitening;
- flag 0x10 / 0x40 removal as final fix;
- generic LOD/distance increases;
- WSCivilianProp 125 -> 500/1000;
- resource-bank eviction suppression;
- Into/Outo transition suppression;
- WSWillToFightGrid 1024;
- generic WTF function suppression;
- SliceQuality changes that hide red by breaking scenery.

## 0.47 surgical candidate

0.47 suppresses only the incorrect fallback proxy.

RVA 0x00092E03:
- expected `85 DB 0F 84 3B 01 00 00`
- patch `E9 3E 01 00 00 90 90 90`
- native destination VA 0x00492F46.

The real WSCivilianProp path is unchanged.

Validation target:
1. same occupied-zone route where red proxy was reproducible;
2. distant red proxy absent;
3. genuine accessory still appears when close/available;
4. no pedestrian, save/load, zone-transition or crash regression.

If validated, this becomes the release behavior. If the temporary no-prop
interval is considered unacceptable, the next research branch is to accelerate
population of the native civilian-prop table rather than recolor the proxy.

## 0.47 in-game result

The proxy bypass removed the red bug but also removed pedestrian hand props entirely. Therefore suppression of `rnd civilian prop(%d)` is diagnostic only, not a shippable fix.

## 0.48 branch

Restore the native proxy and test the historical CivilianProp gate redirect:
- table VA 0x00B85C80 / RVA 0x00785C80;
- retail pointer 0x00474A20;
- alternate native pointer 0x0048B560.

This branch preserves the prop system and tests whether earlier access to the normal CivilianProp handler can prevent the red proxy phase without deleting accessories.


## 0.49 runtime audit architecture

0.48 is superseded without validation. The modern investigation now uses one
runtime laboratory rather than static A/B rebuilds.

Base behavior: clean 0.46.

Hook:
- RVA `0x00092E03`;
- expected bytes `85 DB 0F 84 3B 01 00 00`;
- pass-through trampoline reproduces the native `test ebx,ebx / je` semantics.

Hotkeys:
- F6 native proxy;
- F7 suppress proxy;
- F8 verbose native;
- F9 snapshot latest fallback context.

Captured chain:
- `EBX`;
- `[EBX+0x1384]`;
- `[[EBX+0x1384]+0x2DC]`;
- actor/owner/template dword snapshots;
- real-prop table cell at module base + `0x00E129E0`.

Goal: identify the proxy-specific runtime state responsible for the red
appearance, then patch that property only. The proxy is no longer deleted as a
proposed final fix.


## 0.49 in-game runtime result

The runtime laboratory works, but the fallback-entry capture point is too early.

Observed:
- first 20 fallback-entry events: 19 template=NULL, 1 non-null template;
- verbose event #78: owner valid, template=NULL;
- manual F9 snapshot: real-prop table cell sampled as NULL.

This means entry at RVA 0x00092E03 does not imply that a proxy is actually
created. Many calls leave before a usable template exists.

Next owner:
- hook the real proxy-creation call around VA 0x00492F38 -> 0x00988B30;
- capture arguments + return value;
- dump the created proxy object itself;
- compare against the real WSCivilianProp object path.

Do not derive material state from [owner+0x2DC] at the early entry anymore.


## ASI 0.50 native proxy-creation return probe (09 Oct 2026)

**TEST ONLY; never promote above canonical 0.45 before in-game evidence.**

Base source: main e429156 (0.49 runtime result), isolated on branch
dev/asi-0.50-proxy-creation-capture. No EXE or normal render/LOD/HUD/INI quality
changes. The old 0.49 broad-entry fallback hook is NOT installed in 0.50.

The new passive x86 CALL interception targets site RVA 0x00092F38
(VA 0x00492F38 at standard image base) and expects an E8 direct call to
RVA 0x00588B30 (VA 0x00988B30). It validates the opcode and decoded
rel32 call target and skips patching if either differs. The shim tail-jumps
to the genuine native target with entry GPRs, EFLAGS and argument stack
preserved, and its return thunk observes EAX after the native call finishes.
Per-thread 8-deep pre/post frames avoid correlating unrelated calls.
Sampled diagnostics include raw stack arguments, GPRs, returned EAX and
immediately copied candidate object, ECX and arg0 memory (guarded reads).
EAX is NOT yet proven to be a true prop pointer or material handle.

Hotkeys are deliberately changed: F6 quiet native, F7 capture next 16
completed creates (not proxy suppression!), F8 capture next 64, F9 replay
latest captured snapshot without dereferencing stale pointers. Real
WSCivilianProp successful-lookup path RVA 0x00092DD5 is read-only:
its original bytes are logged as groundwork, not patched. The paired
normal-prop creation hook still requires static owner verification.

CI build: Windows x86 SUCCESS (run 251, 2026-10-09).
ASI SHA-256: bdf6e043923234b73e44bb8d9f7fae1cb054e4a6cafdff7325fa713412b28696
Loader SHA-256: b741567a9ef766262cae9cab26a817d1204f705aab81e8d2894760b016216292
GitHub artifact ID: 11631392340
Artifact digest SHA-256: 2201b5f1efb7317523e4c9248b9f38e539ed23b872afa5b671551423b24aa441
Build artifact: https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/37960412042/artifacts/11631392340

Next test: known occupied-zone route, confirm [OK] PROXY50 hook (else
record [SKIP] observed bytes/target), press F8 and then F9, send
SaboteurEnhanced.log; report whether red proxies and pedestrian accessories
remain intact. Runtime behavior/stack integrity are not yet in-game validated.
Rollback to canonical 0.45 ASI + matching INI if any instability.


## ASI 0.50 runtime RESULT -> 0.51 out-parameter memory diff (10 October 2026)

**Validated by actual user log:** 0.50 loaded and hooked a real CALL
RVA 0x00092F38 -> RVA 0x00588B30. Native CALL completion events #1..#8
all returned EAX=00000000. This **does not prove** a missing proxy;
the function may use return status, a void return, or an out-parameter.
Input from #8: ECX=0AAE3D80, ESI/first argument=0B83DA50,
EDX/second argument=01B3F5A8, EDI=0AAE6E40.
F8 was enabled at 22:24:02 **after** last event #8 at 22:16:48;
F9 at 22:24:11 therefore replayed stale capture #8, not a new one.
The ASI unloaded normally; visual/gameplay state was not recorded in log.

**0.51 research approach:** Keep same strict checked native E8 CALL and
no-effects forwarding hook, copy candidate ECX/ARG0/ARG1/EDI pointee memory
256 bytes before/after each native call, log changed dword offsets, and
automatically capture the first 16 calls from startup to avoid missing
events before hotkeys. F6 cancels auto/quiet, F7 records next 16,
F8 next 64, F9 prints latest captured snapshot + total call count.
Read-only callee prologue and caller continuation bytes also logged.
No material/prop overrides, no target EXE changes, no speculative normal
WSCivilianProp hook, no LOD/quality experiments. This version is a
**diagnostic candidate**, not a red-prop correction.

Source branch: dev/asi-0.51-proxy-outparams-diff; initial source commit
829a711400ef7805bb3ccbdfa4101cd2080ecdff. Preserve canonical 0.45.
Detailed test and rollback procedure: docs/ASI_0_51_RUNTIME_AUDIT.md.


## ASI 0.52 TEST: native context follow-up from user-validated 0.51 log

**State (14 October 2026): COMPILED + PUBLISHED AS PRERELEASE, awaiting in-game validation.**

The supplied 0.51 log proved that the x86 CALL at RVA 0x00092F38
reached native target RVA 0x00588B30. It recorded 22 completed calls,
including 16 automatic full captures, and a clean ASI unload.
All captured EAX values were zero. That is not proof of a null
material/object: zero might be status/void residue. The actual
memory mutations occurred in ECX (+04,+08,+18,+1C,+34) and ARG0 (+48).
ARG1 and EDI did not change in the first 256 bytes.

0.52 keeps the 0.51 fast-path instrumentation and native behavior,
adding *read-only, guarded* 64-byte before/after samples around:
- ECX+0x207C deep candidate field, with no assumption of its meaning;
- ECX[+0x08] and ECX[+0x1C] child pointers, with distinct-object
  transitions identified (never compare two allocations as same object);
- EBX -> [EBX+0x1384] owner -> [owner+0x2DC] template, including
  snapshots of the same owner/template pointers before/after the call.

No change to EXE, shaders, materials, props, graphics, HUD, pool
capacities, LOD or streaming. The previous broad fallback suppression
hook is **not enabled**. All the first 16 calls are captured without F8;
F6 quiet; F7 next 16; F8 next 64; F9 latest + total.

Code source SHA: 7b9cc6d4e7f74d92f9e80c6b7f749601cfef29f8.
Core 1 + ASI 0.45 remains the canonical validated baseline.
0.52 publication: https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/tag/ASI-0.52-TEST
Download: https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/releases/download/ASI-0.52-TEST/SaboteurEnhanced_ASI_0.52_NATIVE_STRUCTURE_CONTEXT_TEST_x86.zip

Windows x86 GitHub Actions run #254 SUCCESS:
https://github.com/DeadneM/The-Saboteur-Enhanced-Patch/actions/runs/38542535779
ASI SHA-256 DAF2F41645C6DB7B6A0F1D4DB232E704EDC21863CBF3D4B1E9936CBE46FE14E4.
dinput8.dll SHA-256 393F19DFBEE81E66027CBA6402D93388CEA4D4D3BD9432679E6863D832CF7506.
Prerelease ZIP 186042 bytes; SHA-256
4bfe9c14d072f8fae5ec34ed918576c7ad9ff0d191cb6ab5bc38359cc1c47208.
Flat ZIP with exactly ASI, dinput8.dll, INI and README.
GitHub Actions artifact ID 11883181080, SHA-256
f88932e25e6fb79391fde023c884f3f57e576e512a74f9356c477cd7e84eb581.

Next test: reproduce red prop and F8/F9, send SaboteurEnhanced.log;
report red visibility and accessory retention. Do not declare visual
fix until controlled in-game testing proves it.


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
