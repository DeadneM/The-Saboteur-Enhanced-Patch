# ASI 0.1 runtime validation

Status: **validated in game**

Core executable:
- SHA-256: `83995ab6e04fce264309a37a243744d4f5929c5abb87c3f195fb50fef7f5f2a8`

Runtime components:
- `SaboteurEnhanced.asi` SHA-256: `22b4fc9737a2a1430d4967aacdde46c913d4742c2a045e7b31982476b4481ffd`
- `dinput8.dll` SHA-256: `70cd6d950dbca54bec034d16edc9cbb622f07bab045ad1018343e655784c70d2`

User validation log, with the local install path sanitized:

```text
[00:50:45.691] SaboteurEnhanced ASI 0.1
[00:50:45.691] Architecture: clean Core EXE + runtime ASI fixes
[00:50:45.691] Module base: 0x00400000
[00:50:45.697] INI: <game directory>\SaboteurEnhanced.ini
[00:50:45.697] WSModelFullRenderMask=1
[00:50:45.697] ModelInfoFullRenderSlice=1
[00:50:45.697] .text range: RVA 0x00001000, size 0x00B70000
[00:50:45.701] [OK] V310 WSModel auto RenderSlice full-mask fix applied at RVA 0x00239622.
[00:50:45.704] [OK] V311 explicit ModelInfo RenderSlice full-mask fix applied at RVA 0x002395AF.
[00:50:45.704] ASI initialization complete.
[00:52:17.573] ASI unload.
```

Interpretation:
- the local x86 `dinput8.dll` proxy loaded successfully;
- `SaboteurEnhanced.asi` initialized successfully;
- both default-on runtime fixes found their exact signatures and passed byte verification;
- V310 applied at runtime VA `0x00639622` (module base `0x00400000` + RVA `0x00239622`);
- V311 applied at runtime VA `0x006395AF` (module base `0x00400000` + RVA `0x002395AF`);
- the ASI unloaded cleanly when the game exited.

This validates the new architecture:
**clean Core EXE + dinput8 proxy + modular ASI fixes**.
