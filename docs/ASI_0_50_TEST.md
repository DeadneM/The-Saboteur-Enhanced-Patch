# ASI 0.50 - Native proxy creation return capture (diagnostic)

Status: TEST ONLY. Core 1 + ASI 0.45 stays canonical.
Base: main commit e429156833691e5a68b79f49bef3005149a97c61.

0.49 proved fallback-entry RVA 0x00092E03 is too early. 0.50 does not install that old hook.
Instead, it verifies a direct E8 CALL at RVA 0x00092F38 resolving to native target RVA 0x00588B30.
If either opcode or target differs, it logs SKIP and leaves game code untouched.

The x86 naked call shim preserves input registers and EFLAGS; forwards original
caller stack arguments untouched to the native callee; and captures EAX after
the native callee returns. A per-thread bounded pending stack pairs nested calls.
Copies of register/argument candidates and object memory are taken immediately,
with guarded reads. EAX is a *candidate pointer*, not yet a proven prop object.
F9 prints captured data, never reads a stale pointer.

F6 = native quiet; F7 = next 16 completed creates; F8 = next 64; F9 = last snapshot.
All modes keep native prop behavior. F7 is no longer a suppression toggle.

The genuine WSCivilianProp successful-lookup path at RVA 0x00092DD5 is logged
read-only (first 16 bytes). The normal-object creation site remains UNPROVEN,
so no second inline hook is installed yet. Pairwise material/renderer field
comparison is a subsequent stage after these facts are validated, not a claim
of an already working automatic red-material fix.

Test: load known occupied-zone route, confirm OK hook/creation events in
SaboteurEnhanced.log, press F8 then F9, check the red appearance and that
pedestrian hand accessories remain. Report any SKIP/crash/performance regression.
Rollback: reinstall validated 0.45 ASI (with matching INI) if needed.

No other renderer/graphics/HUD/pool/LOD/streaming/EXE changes.
Build archive must be flat (ASI, dinput8.dll, ini, README), with no source dir.


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
