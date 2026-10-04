# ASI 0.42 Havok Broad-Phase 2048 Test

Status: TEST CANDIDATE.

Base:
- explicit canonical working base is ASI 0.40;
- 0.41 is abandoned and not inherited;
- WSSphereActivatorMaxRadius remains 20.6 exactly as in 0.40;
- the known 0.35->0.40 severe-lag regression is intentionally retained for now.

New 0.42 delta:
- Havok broad-phase query size 1024 -> 2048.

Owner reconstruction:
- historical retail->V200 manifest exact changed byte:
  RAW 0x006C4560: 04 -> 08;
- this is byte +1 of little-endian 1024 / 2048;
- corresponding runtime dword:
  RVA 0x006C535F: 0x00000400 -> 0x00000800;
- adjacent TOI manifest delta:
  RAW 0x006C45B0: FA00 -> 0002,
  independently matching the audited Havok TOI constructor block.

Safety:
- exact dword 1024 is verified before write;
- fail closed on mismatch;
- no streaming, particle, HUD, render, PostFX, shadow, LOD or sphere-radius delta beyond the retained 0.40 baseline.

WSPhGridObject:
- still pending clean ASI reconstruction;
- old V264 used a 43-byte trampoline and will not be copied blindly.

Packaging:
- no BUILD_NOTES.txt.
