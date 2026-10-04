# ASI 0.35 Canonical

Status: VALIDATED.

0.35 is the canonical baseline.

Validated cumulative engine-limit pack over 0.34:
- WSLuaCall 20 -> 40
- WSDamageSphere 512 -> 1024
- WSInventoryStateStow 32 -> 64
- PblCRCTreeNode 40000 -> 60000

All four owners are preflight-verified and applied as one rollback-safe transaction.

All 0.34 canonical behavior remains cumulative.

Future functional builds start from 0.35.
