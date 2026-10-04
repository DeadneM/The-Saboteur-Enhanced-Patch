# ASI 0.37 Canonical

Status: VALIDATED.

0.37 is the canonical baseline.

Validated cumulative delta over 0.36:
- WSPhysicsParticle allocation/runtime ceiling 1000 -> 2000
- WSParticleRender main 4500 -> 9000
- WSParticleRender medium 1000 -> 2000
- WSParticleRender small 500 -> 1000
- matching sort scratch buffers expanded coherently
- all matched runtime caps expanded coherently

All 14 owners are preflight-verified and applied as one rollback-safe transaction.

Packaging rule retained:
- distributed ZIPs do not include BUILD_NOTES.txt.

All 0.36 canonical behavior remains cumulative.
Future functional builds start from 0.37.
