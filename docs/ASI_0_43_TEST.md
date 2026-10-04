# ASI 0.43 WSPhGridObject 2000 Test

Status: TEST CANDIDATE. 0.42 remains canonical until explicit validation.

Base:
- strict cumulative continuation from validated ASI 0.42;
- 0.41 remains abandoned;
- known 0.35->0.40 performance regression remains accepted;
- WSSphereActivatorMaxRadius remains 20.6;
- Havok broad-phase remains 2048.

New delta:
- WSPhGridObject 1000 -> 2000.

Retail reconstruction from exact original EXE:
- object stride: 0x38 (56 bytes);
- capacity globals: VA 0x0132AF78 and 0x0132AF7C;
- active counter: VA 0x0132AF80;
- shared initializer source:
  VA 0x009AE665 / RVA 0x005AE665 = mov edi,1000;
- WSPhGridObject generic pool init call:
  VA 0x009AE6A6 / RVA 0x005AE6A6 -> VA 0x00E44D00;
- three active-cap checks:
  VA 0x006CC64C / RVA 0x002CC64C,
  VA 0x006CCB30 / RVA 0x002CCB30,
  VA 0x006CCDD2 / RVA 0x002CCDD2.

Important neighbor invariant:
- WSHKCreationDataContainer immediately follows and shares EDI;
- it must remain 1000;
- 0.43 therefore redirects only the WSPhGridObject generic init CALL through
  an ASI wrapper that restores EDI=1000 after the retail init returns.

Safety:
- all initializer/call/cap bytes are preflighted before first write;
- transaction rolls back load, CALL and active caps on any write failure;
- 16-bit index design remains safe at 2000;
- extra contiguous storage is ~56,000 bytes.

Packaging:
- no BUILD_NOTES.txt.
