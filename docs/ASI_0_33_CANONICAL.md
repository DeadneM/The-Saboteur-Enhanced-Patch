# ASI 0.33 Canonical

Status: VALIDATED.

ASI 0.33 is the current canonical baseline for The Saboteur Enhanced Patch.

Validated delta over 0.32:
- full-resolution WSSpotShadowZBuffer profile;
- SpotShadowResolutionScale = 1.0;
- the three exact spot-shadow creation consumers remain atomically verified before any write;
- all validated 0.32 visibility, DepthBlur, V310/V311, PCF5x5, AO, distance/LOD, UI, streaming and PostFX behavior is preserved.

Canonical CI:
- run: 215
- run ID: 37200406536
- head SHA: 191a48cacea1940abebeb5f33b0cc57f5922a7b9
- artifact ID: 11302593864
- artifact: SaboteurEnhanced_ASI_0.33_CANONICAL_x86
- ZIP SHA-256: 80fc0439abc416b9b11ed29e70793b1179e3da7b2825e4110bcde9cceaa2b64d
- SaboteurEnhanced.asi SHA-256: d371c19e012a07a04f603c012b83b88057f0f66429c9d0d94fbe490a0f2a2a7f
- dinput8.dll SHA-256: 84b0923bbc498b2c240e42a38e30c4209d5f211999056f4bdeb06e35b5f0003d
- both binaries verified PE machine x86 / 0x14C.

Future functional builds must start from 0.33.
