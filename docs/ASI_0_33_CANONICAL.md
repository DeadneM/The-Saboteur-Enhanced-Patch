# ASI 0.33 Canonical

Status: VALIDATED.

ASI 0.33 is the current canonical baseline for The Saboteur Enhanced Patch.

Validated delta over 0.32:
- full-resolution WSSpotShadowZBuffer profile;
- SpotShadowResolutionScale = 1.0;
- the three exact spot-shadow creation consumers are atomically verified before any write;
- all validated 0.32 visibility, DepthBlur, V310/V311, PCF5x5, AO, distance/LOD, UI, streaming and PostFX behavior is preserved.

Future functional builds must start from 0.33.

Build-specific hashes are recorded separately in docs/ASI_0_33_BUILD.md.
