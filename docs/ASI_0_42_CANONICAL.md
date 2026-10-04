# ASI 0.42 Canonical

Status: VALIDATED.

0.42 is the canonical working baseline.

Base policy:
- 0.40 remains the requested retained development line;
- 0.41 is abandoned and excluded;
- the known cumulative 0.35 -> 0.40 performance regression remains accepted for now.

Validated delta:
- Havok broad-phase query size 1024 -> 2048;
- exact historical owner reconstructed from retail -> V200 byte manifest;
- runtime dword RVA 0x006C535F;
- fail-closed verification of exact retail 1024 before write.

All 0.40 cumulative settings remain retained, including
WSSphereActivatorMaxRadius=20.6.

Future builds start from 0.42.
