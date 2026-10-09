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
