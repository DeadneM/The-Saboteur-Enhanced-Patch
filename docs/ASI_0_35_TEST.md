# ASI 0.35 Validated Engine Limits Pack Test

Status: TEST CANDIDATE. ASI 0.34 remains canonical.

0.35 keeps every validated 0.34 feature and adds one atomic engine-capacity pack:

- WSLuaCall: 20 -> 40
- WSDamageSphere: 512 -> 1024
- WSInventoryStateStow: 32 -> 64
- PblCRCTreeNode: 40000 -> 60000

These values were each validated historically in the mature EXE lineage.

Safety:
- all four retail/Core1 owners are verified before the first write;
- if any later write fails, every earlier write is restored;
- PblCRCTreeNode remains below the WORD/0xFFFF structural ceiling;
- no rendering, HUD, LOD, shadow, PostFX, physics or streaming scheduler setting changes.

0.34 remains the rollback/canonical baseline until explicit validation.
