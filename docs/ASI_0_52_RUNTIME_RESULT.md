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
