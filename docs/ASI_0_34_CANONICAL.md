# ASI 0.34 Canonical

Status: VALIDATED.

0.34 is the validated WSDecal baseline and the base for 0.35.

Validated delta over 0.33:
- WSDecal pool 400 -> 800;
- WSDecal active-list ceiling 400 -> 800;
- both owners are preflight-verified before any write;
- if the second write fails, the pool is restored to 400.

All 0.33 canonical behavior remains cumulative.
