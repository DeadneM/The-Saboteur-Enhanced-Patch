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
