# ASI 0.40 WSSphereActivator x10 Diagnostic

Status: TEST CANDIDATE. ASI 0.39 remains canonical.

Purpose:
- continue the same isolated owner validated in 0.39;
- determine whether the remaining near-pop/activation behavior continues to scale with WSSphereActivator radius.

Native behavior:
- sphere-create routine VA 0x0068EF80;
- max-radius scalar VA 0x00FCD834 / RVA 0x00BCD834;
- native clamp 2.06;
- native growth multiplier 1.05 remains untouched.

0.40 changes only:
- WSSphereActivatorMaxRadius 8.24 -> 20.6;
- equivalently native 2.06 -> 20.6 (x10).

Reason for x10:
- 0.39 x4 is explicitly validated;
- project testing strategy prefers a strong next step to reveal whether the owner truly matters, then reduction only if instability appears.

Watch for:
- further reduction of short-range object/activation pop;
- trigger activation happening too early;
- AI/physics/query behavior changes;
- CPU spikes or abnormal world activity.

Safety:
- exact clean-retail 2.06f scalar is still verified before write;
- no other distance, pool, render, streaming, HUD, shadow, PostFX or Havok value changes.

Packaging:
- no BUILD_NOTES.txt in distributed ZIPs.

0.39 remains the rollback/canonical baseline until explicit validation.
