# ASI 0.36 Simple Engine Limits Pack Test

Status: TEST CANDIDATE. ASI 0.35 remains canonical.

0.36 keeps every validated 0.35 feature and adds one rollback-safe pack:

- WSParkingSpace: 32 -> 64
- WSParticleInfoData: 1400 -> 2800
- WSActivateSphere: 256 -> 512
- WallPoint: 50 -> 100
- WallSegment: 50 -> 100

All five MOV EAX,imm32 owners are verified before the first write.
If any write fails, all earlier writes are restored.

No render, shadow, LOD, HUD, PostFX, streaming scheduler or physics parameter changes.

Packaging rule from 0.36 onward:
- BUILD_NOTES.txt is not included in distributed ZIPs.
- Technical notes remain in the GitHub docs folder only.

0.35 remains the rollback/canonical baseline until explicit validation.
