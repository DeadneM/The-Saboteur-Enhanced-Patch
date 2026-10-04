# ASI 0.39 WSSphereActivator x4 Diagnostic

Status: TEST CANDIDATE. ASI 0.38 remains canonical.

Goal:
- return to the near-pop / object-activation problem after the validated engine-capacity migration;
- test a real short-range activation/query clamp rather than another pool size.

Recovered native owner:
- sphere-create routine: VA 0x0068EF80;
- max-radius scalar: VA 0x00FCD834 / RVA 0x00BCD834 = 2.06f;
- growth multiplier: VA 0x00FCD838 = 1.05f;
- native behavior: effective_radius = min(requested_radius * 1.05, 2.06).

0.39 changes only:
- WSSphereActivatorMaxRadius 2.06 -> 8.24 (x4).

The already validated WSActivateSphere pool remains 512 from canonical 0.36.

Important scope warning:
- this is a broader activation/spatial-query system;
- it is not yet proven to own static scenery rendering;
- callers include gameplay/engine paths and several derive radius from object/model +0x58 size;
- watch for altered triggers, AI activation, physics/query behavior or abnormal CPU cost.

Safety:
- exact clean-retail 2.06f scalar bytes are verified before write;
- fail closed on mismatch;
- no RenderSlice, WSModel A8/AC, streaming, shadows, PostFX, HUD, Havok or pool-capacity changes.

Packaging:
- no BUILD_NOTES.txt in distributed ZIPs.

0.38 remains the rollback/canonical baseline until explicit validation.
