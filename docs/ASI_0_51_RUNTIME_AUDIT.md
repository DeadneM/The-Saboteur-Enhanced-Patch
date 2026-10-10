# ASI 0.51 - Native proxy input/output mutation audit (TEST)

## Evidence from user ASI 0.50 runtime log, 10 October 2026

- Correct ASI initialization; [OK] hook RVA 0x00092F38 targeting RVA 0x00588B30.
- Native calls #1..#8 all returned EAX=00000000; returned EAX is **not established** as object pointer.
- Last call #8 was 22:16:48.572; user pressed F8 at 22:24:02.440, so no detailed post-F8 event was captured.
- F9 at 22:24:11.808 replayed existing #8, not a new event.
- Input #8: ECX=0AAE3D80, ESI/ARG0=0B83DA50, EDX/ARG1=01B3F5A8, EDI=0AAE6E40.
- This is compatible with native side-effects, output parameters, or null returns. **Do not claim creation succeeds or material cause from these facts.**
- ASI unloaded normally. This log alone does not verify in-game visuals or performance.

## 0.51 scope

The 0.50 CALL shim and its strict opcode/target check are retained without alteration.
No global scene/LOD/texture/engine-limit/EXE change. No fallback suppression.

Instrumentation: copy the first 256 bytes from the four observed input pointers
(ECX, ARG0, ARG1, EDI) **before and after** the genuine callee executes,
with guarded reads on both sides. Summarize changed dword offsets, print up to
20 differences per candidate, and mark when memory is unreadable. This tests
an output-parameter / side-effect hypothesis rather than guessing EAX object.

**AUTO captures the first 16 completed calls from startup**, as 0.50's manual
F8 arrived after all eight calls; F6 disables auto and budget, F7 next 16,
F8 next 64, F9 latest snapshot + total observed call count. Also logs read-only
first 32 bytes following original CALL and first 64 bytes of real callee for
later static disassembly. The normal WSCivilianProp callsite is still unknown
and is not detoured. 0.51 is a diagnostic, not a red texture fix.

Accept only if ASI installs and game remains stable, red/normal props do not
vanish, and log shows before/after deltas for actual early events.
If no deltas: use callee disassembly and expand only grounded owner offsets,
not guessed render/material fields. Rollback to the 0.45 canonical ASI on crash.

## Log and history integrity

The 0.50 runtime log is not proof that EAX is a null object creation;
zero could be a status or a void-return residue. No on-disk original
Saboteur.exe was available in this audit. No unverified disassembly claims.
