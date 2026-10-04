# ASI 0.45 Canonical

Status: VALIDATED / RELEASE-CANDIDATE BASELINE.

0.45 is the validated canonical baseline.

Behavior:
- identical intended engine/render behavior to 0.44;
- rejected Odin child-visibility and WSDamageable selector paths removed from runtime;
- historical Odin diagnostic runtime path removed;
- no new engine/render scalar was introduced.

Retained project decisions:
- 0.41 remains abandoned;
- the severe lag somewhere in the cumulative 0.35 -> 0.40 sequence remains a
  known accepted regression for this branch;
- WSSphereActivatorMaxRadius remains 20.6;
- Havok broad-phase remains 2048;
- WSPhGridObject remains 2000;
- HumanObjectQualityScale remains 5.0.

Next:
- source cleanup only, with no behavior change;
- then return to the unresolved red-texture / occupation-state rendering bug.
