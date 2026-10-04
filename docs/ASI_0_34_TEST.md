# ASI 0.34 WSDecal 800 Test

Status: TEST CANDIDATE. ASI 0.33 remains canonical.

0.34 changes one coherent engine-limit owner only:
- WSDecal pool capacity: 400 -> 800;
- WSDecal active-list ceiling: 400 -> 800.

Historical lineage:
- 800 was validated in the earlier V260 branch.
- The clean ASI owner now patches both exact runtime sites together instead of
  changing a shared/global constant.

Safety:
- both retail signatures and both 400 immediates are verified before any write;
- the pool is written first;
- if the active-ceiling write fails, the pool is restored to 400;
- no streaming, physics, shadow, LOD, PostFX, HUD or visibility value changes.

Test target:
- heavy combat / explosions / bullet impacts;
- check whether decals persist more reliably under load;
- reject on crashes, corruption, excessive stale decals or abnormal memory use.

ASI 0.33 remains the rollback/canonical baseline until explicit validation.
