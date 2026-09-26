# SaboteurEnhanced ASI

Runtime half of the cleaned The Saboteur Enhanced Patch architecture.

## 0.1

Default-on fixes:
- V310 WSModel automatic RenderSlice full-mask correction
- V311 explicit ModelInfo RenderSlice full-mask correction

The ASI scans the executable .text section for exact instruction signatures, verifies target bytes, applies the patch with VirtualProtect, flushes the instruction cache, and writes SaboteurEnhanced.log.

The bundled dinput8.dll is an x86 proxy loader. It forwards the standard DirectInput8 exports to the real System32 dinput8.dll and loads SaboteurEnhanced.asi from the game directory.

No tuner.txt modifications are used.

Build target: Win32 / x86.


## Runtime validation

ASI 0.1 is validated in game on Core 1.

Validated behavior:
- dinput8 x86 proxy loads the real System32 DirectInput8 implementation;
- SaboteurEnhanced.asi loads and initializes;
- V310 runtime patch applies successfully;
- V311 runtime patch applies successfully;
- ASI unload is clean on game exit.

Detailed log: `../docs/ASI_0_1_VALIDATION.md`
