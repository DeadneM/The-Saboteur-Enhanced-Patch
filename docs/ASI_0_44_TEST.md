# ASI 0.44 Clean Cumulative Baseline Test

Status: TEST CANDIDATE. Functional base is validated 0.43.

Purpose:
- consolidate the now-complete validated engine-limit migration;
- remove shipped configuration clutter from experiments that are explicitly
  rejected or dormant;
- preserve game behavior exactly from 0.43.

Functional changes:
- NONE intended.

Retained exactly:
- all canonical graphics/PostFX/HUD/distance fixes;
- 0.35-0.37 capacity packs;
- Havok TOI 1024;
- WSSphereActivatorMaxRadius 20.6;
- Havok broad-phase 2048;
- WSPhGridObject 2000 with WSHKCreationDataContainer still 1000;
- HumanObjectQualityScale=5.0, which already owns the historical
  25/50/70/100/150/200/300 family.

Cleanup:
- remove OdinChildVisibilityGate and WSDamageableVariantSelector from the
  distributed INI because both experiments were rejected and default disabled;
- remove the dormant [Diagnostics] section from the distributed profile;
- remove the temporary object-distance CI audit step;
- update README canonical lineage.

Source-side dormant diagnostic/rejected support is not forcibly deleted yet,
so this build minimizes risk. A later release cleanup can remove unreachable
code after final validation.

Packaging:
- exactly SaboteurEnhanced.asi, dinput8.dll, SaboteurEnhanced.ini, README.txt;
- no BUILD_NOTES.txt.
