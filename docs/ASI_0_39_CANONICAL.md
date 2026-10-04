# ASI 0.39 Canonical

Status: VALIDATED.

0.39 is the canonical baseline.

Validated delta over 0.38:
- WSSphereActivator max-radius clamp 2.06 -> 8.24 (x4)
- exact clean-retail scalar VA 0x00FCD834 / RVA 0x00BCD834
- native 1.05 growth multiplier unchanged
- exact 2.06f retail value verified before write

This confirms that the x4 WSSphereActivator radius increase is stable enough to retain.
It does not by itself prove exclusive ownership of static scenery rendering, but the
owner is now validated in the cumulative ASI lineage.

Packaging rule retained:
- distributed ZIPs do not include BUILD_NOTES.txt.

All 0.38 canonical behavior remains cumulative.
Future functional builds start from 0.39.
