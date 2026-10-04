# ASI 0.44 Canonical

Status: VALIDATED.

0.44 is the canonical working baseline.

0.44 intentionally introduced no new engine/render behavior over validated 0.43.
It is the cleaned cumulative profile:
- all validated 0.43 behavior retained;
- rejected Odin/WSDamageable toggles removed from the distributed INI;
- dormant diagnostics section removed from the distributed INI;
- temporary CI audit removed;
- README canonical lineage refreshed.

Important retained project decision:
- the severe performance regression somewhere in the cumulative 0.35 -> 0.40
  sequence remains accepted for now;
- 0.41 remains abandoned;
- future builds continue from 0.44 unless explicitly changed.

Future build direction:
- release-candidate cleanup only;
- no new engine-value stacking before release readiness is established.
