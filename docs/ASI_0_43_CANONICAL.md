# ASI 0.43 Canonical

Status: VALIDATED.

0.43 is the canonical working baseline.

Validated delta over 0.42:
- WSPhGridObject capacity 1000 -> 2000;
- matching three active-count guards 1000 -> 2000;
- adjacent WSHKCreationDataContainer preserved at 1000;
- ASI wrapper restores EDI=1000 after WSPhGridObject generic initialization;
- fail-closed preflight and rollback retained.

All cumulative 0.42 settings remain retained, including:
- Havok TOI 1024;
- Havok broad-phase query 2048;
- WSSphereActivatorMaxRadius 20.6;
- all validated 0.35-0.37 engine/particle capacity changes.

Known performance regression in the 0.35 -> 0.40 range remains accepted for now by explicit project decision.

Future builds start from 0.43.
