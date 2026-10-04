# ASI 0.45 Release Candidate

Status: RELEASE CANDIDATE. 0.44 remains canonical until explicit validation.

Functional baseline:
- intended engine/render behavior identical to validated 0.44;
- no new quality, LOD, pool, streaming, Havok, HUD or PostFX values.

Release-path cleanup:
- rejected OdinChildVisibilityGate runtime read/log/apply path removed;
- rejected WSDamageableVariantSelector runtime read/log/apply path removed;
- historical Odin diagnostic INI reads removed;
- Odin diagnostic hook installation / marker start removed;
- Odin diagnostic shutdown thread-stop / summary removed.

Historical diagnostic functions remain compiled but unreachable for this RC.
This minimizes source-deletion risk before final release.

Retained:
- complete validated 0.44 profile;
- accepted 0.35->0.40 performance regression;
- 0.41 abandoned;
- Havok broad-phase 2048;
- WSPhGridObject 2000;
- WSSphereActivatorMaxRadius 20.6;
- HumanObjectQualityScale 5.0.

Packaging:
- SaboteurEnhanced.asi
- dinput8.dll
- SaboteurEnhanced.ini
- README.txt
- no BUILD_NOTES.txt.
