# The Saboteur ASI 0.52: Runtime structure context audit

**TEST ONLY.** Branch `dev/asi-0.52-runtime-structure-observation`
from 0.51 source commit `dee95d70e6d6b2e815d339608e028ed667973a75`.
Core 1 + ASI 0.45 remains the canonical validated release baseline.

## Evidence from in-game ASI 0.51 log
User log 2026-10-11:
- Native direct CALL verified at RVA 0x00092F38 to RVA 0x00588B30.
- Twenty-two calls counted, first 16 detailed automatically.
- EAX was 0 on all 16 logged calls, **not proof of no prop**.
- ARG0/ESI +0x48 increased once per captured call (16/16).
- ECX +0x18 changed in 12 calls, +0x1C changed in 7.
- ECX +0x04 / +0x08 also changed in some calls.
- ARG1/EDX and EDI showed no modifications within first 256 bytes.
- F9 at 22:55:36 showed total 22, replaying #16 rather than capturing #22.
- ASI unloaded normally. **No confirmation of whether red bug remained visible.**

## New 0.52 passive probes
Keep the same exact-verified original CALL shim and whole 0.51 engine/HUD
feature set. Read only and snapshot, before and after each sampled native call:
- `ECX+0x207C`, 64-byte region. **Candidate offset**, not an established
  renderer/material owner. Guarded memory checks avoid invalid dereference.
- pointer candidates stored at `ECX+0x08` and `ECX+0x1C`; snapshot each
  referenced region (64 bytes) and check whether the pointer itself changes.
  If pointer identity changes, do not compare two objects as one object.
- EBX actor pointer, owner `[EBX+0x1384]`, template
  `[[EBX+0x1384]+0x2DC]`, with safe read-only owner/template prefix
  snapshots. Do not infer material pointers when fields are null/uninitialized.
- Retain ECX/ARG0/ARG1/EDI 256-byte before/after comparisons and
  first-16-events automatic logging from 0.51.

F6 = quiet; F7 = next 16 complete native calls; F8 = next 64;
F9 = latest saved event and live total. No suppression, recoloring,
shader replacement, general LOD changes or EXE changes.

## Acceptance and rollback
Use a save with occupied-zone red accessory bug. Load, wait for the automatic
16 events, move through the known bug area, press F8, then F9.
Send `SaboteurEnhanced.log` and report red objects yes/no. Do not confuse
startup calls with proven proxy-created events. Any instability: roll back to
canonical 0.45 ASI + corresponding INI.

## Build and release
Windows x86, GitHub Actions workflow `build-core-asi.yml`.
Flat ZIP root: `dinput8.dll`, `SaboteurEnhanced.asi`,
`SaboteurEnhanced.ini`, `README.txt`.
Published only as GitHub **prerelease** `ASI-0.52-TEST` from test branch.
Promotion to main is forbidden pending game validation.


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
