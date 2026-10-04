# ASI 0.38 Havok TOI 1024 Test

Status: TEST CANDIDATE. ASI 0.37 remains canonical.

Audit resolution:
- exact clean-retail owner: VA 0x00AC53AD / runtime RVA 0x006C53AD;
- opcode: C7 46 64 <imm32>;
- clean retail value: 250;
- the historical V262 note "512 -> 1024" described a cumulative branch where an earlier stage had already raised 250 -> 512;
- therefore the clean ASI path can safely test the final historical target directly as 250 -> 1024.

0.38 changes only:
- Havok SizeOfToiEventQueue-like field: 250 -> 1024.

Safety:
- exact C7 46 64 signature verified;
- exact retail immediate 250 verified before write;
- fail closed on any mismatch;
- no streaming, render, HUD, LOD, PostFX, particle-capacity or shadow change.

Packaging:
- no BUILD_NOTES.txt in distributed ZIPs.

0.37 remains the rollback/canonical baseline until explicit validation.
