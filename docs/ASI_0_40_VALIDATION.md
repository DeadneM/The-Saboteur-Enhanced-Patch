# ASI 0.40 Validation

Status: FUNCTIONALLY VALIDATED, PERFORMANCE REGRESSION REPORTED.

Validated behavior:
- WSSphereActivator max-radius clamp 2.06 -> 20.6 (x10) is functionally stable in the user's test.

Important regression:
- the user reports severe lag somewhere in the cumulative 0.35 -> 0.40 sequence;
- therefore 0.40 must NOT be treated as a release-ready performance baseline yet.

Immediate isolation order:
1. revert only WSSphereActivator 20.6 -> 2.06 while retaining 0.35-0.38;
2. if lag remains, revert Havok TOI 1024 -> 250;
3. if lag remains, revert particle-capacity pack 0.37;
4. if lag remains, revert simple-engine-limits 0.36;
5. if lag remains, revert 0.35 engine-limit pack.

This provides a deterministic rollback ladder without disturbing validated render/UI work.

Packaging rule retained:
- no BUILD_NOTES.txt in distributed ZIPs.
