# ASI 0.36 Canonical

Status: VALIDATED.

0.36 is the canonical baseline.

Validated cumulative delta over 0.35:
- WSParkingSpace 32 -> 64
- WSParticleInfoData 1400 -> 2800
- WSActivateSphere 256 -> 512
- WallPoint 50 -> 100
- WallSegment 50 -> 100

The five owners are preflight-verified and applied as one rollback-safe transaction.

Packaging rule retained:
- distributed ZIPs do not include BUILD_NOTES.txt.

All 0.35 canonical behavior remains cumulative.
Future functional builds start from 0.36.
