# ASI 0.38 Canonical

Status: VALIDATED.

0.38 is the canonical baseline.

Validated delta over 0.37:
- Havok TOI event queue field 250 -> 1024
- exact owner VA 0x00AC53AD / runtime RVA 0x006C53AD
- exact signature C7 46 64 and retail immediate 250 are verified before write

Audit resolution:
- clean retail baseline is 250;
- historical 512 was an intermediate cumulative state;
- historical V262 then raised 512 -> 1024;
- the clean ASI path therefore uses the final target directly as 250 -> 1024.

Packaging rule retained:
- distributed ZIPs do not include BUILD_NOTES.txt.

All 0.37 canonical behavior remains cumulative.
Future functional builds start from 0.38.
