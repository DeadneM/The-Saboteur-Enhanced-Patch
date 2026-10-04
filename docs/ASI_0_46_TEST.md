# ASI 0.46 Clean Source Baseline

Status: TEST CANDIDATE. 0.45 is canonical until explicit validation.

Purpose:
- perform the source cleanup deferred from 0.45;
- preserve identical intended runtime behavior.

Deleted from source:
- rejected Odin child-visibility A/B patch function;
- rejected WSDamageable variant-selector A/B patch function;
- complete historical Odin diagnostic implementation, including:
  vtable wrappers, sync detour, snapshot/fingerprint state, F9 marker thread,
  install helpers and diagnostic globals.

Retained unchanged:
- every validated 0.45 runtime owner and setting;
- known accepted 0.35->0.40 performance regression;
- 0.41 remains abandoned;
- WSSphereActivatorMaxRadius 20.6;
- Havok TOI 1024;
- Havok broad-phase 2048;
- WSPhGridObject 2000;
- HumanObjectQualityScale 5.0;
- all validated rendering/HUD/PostFX/UI-cache/distance work.

No game-behavior change is intended in 0.46.
