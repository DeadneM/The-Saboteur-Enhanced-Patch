# ASI 0.41 Performance Isolation - WSSphereActivator Native

Status: TEST CANDIDATE.

Reason:
- 0.40 functionally validated;
- user reports severe lag somewhere in cumulative 0.35 -> 0.40.

0.41 keeps every cumulative 0.35 -> 0.38 change and reverts ONLY:
- WSSphereActivatorMaxRadius 20.6 -> 2.06 native.

Purpose:
- determine whether the severe lag is caused by the x4/x10 activation/query radius expansion.

If performance recovers:
- WSSphereActivator expansion is the culprit;
- keep native 2.06 or retest a smaller bounded value later.

If performance does not recover:
- next rollback target is Havok TOI 1024 -> 250 while preserving all earlier packs.

No render, HUD, shadow, PostFX, streaming, particle-capacity, or engine-limit changes beyond this single rollback.

Packaging:
- no BUILD_NOTES.txt in distributed ZIPs.
