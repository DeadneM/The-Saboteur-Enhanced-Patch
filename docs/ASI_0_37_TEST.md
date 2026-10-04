# ASI 0.37 Particle Capacity Pack Test

Status: TEST CANDIDATE. ASI 0.36 remains canonical.

0.37 keeps every validated 0.36 feature and adds one rollback-safe particle/physics capacity transaction.

WSPhysicsParticle:
- allocation 1000 -> 2000
- runtime ceiling 1000 -> 2000

WSParticleRender:
- main arena 4500 -> 9000
- medium arena 1000 -> 2000
- small arena 500 -> 1000
- main sort scratch follows 9000
- medium sort scratch follows 2000
- all matching runtime caps follow the raised capacities
- small-class N and N-1 checks remain coherent at 1000 / 999

Safety:
- all 14 owners are verified before the first write;
- if any write fails, every earlier site is restored;
- no streaming scheduler/job-pool change;
- no Havok TOI change in this build because the historical notebook records 512 -> 1024 while the current ASI path still expects 250. That discrepancy is reserved for a separate audit.

Packaging:
- no BUILD_NOTES.txt in distributed ZIPs.

0.36 remains the rollback/canonical baseline until explicit validation.
