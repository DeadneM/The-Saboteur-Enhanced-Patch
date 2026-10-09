# ASI 0.49 Runtime Red-Prop Audit Result

Status: **DIAGNOSTIC SUCCESS / CAPTURE POINT TOO EARLY**

User-supplied runtime log validates the 0.49 laboratory itself:

- ASI 0.49 loaded successfully;
- RedPropRuntimeAudit=1;
- red-prop runtime hook installed at RVA 0x00092E03;
- F6/F7/F8/F9 hotkey thread started;
- clean ASI unload.

## Runtime observations

### Fallback-entry frequency

The first 20 fallback-entry events show:

- 19 events with a non-null owner but **template = 0**;
- only event #18 has a non-null template:
  - EBX 0x35B76550
  - owner 0x0C65C6C0
  - template 0x0B894280

Therefore the entry hook at RVA 0x00092E03 is a broad control-flow gate, not a
reliable proxy-creation event.

### F8/F9 snapshot

At verbose event #78:

- EBX = 0x5137F8E0
- owner = 0x0C6DAFF0
- template = 0
- real-prop table cell = 0

The owner snapshot begins with:
- +0x00: 0x00F9FB24
- +0x04: 0x00F9FB64
- +0x08: 1
- +0x0C: 0.3f
- +0x10: 1.3f
- +0x14: 0.8f

Then much of the following memory is 0xAAAAAAAA, consistent with
uninitialized/debug-filled storage. This reinforces that dereferencing +0x2DC
at fallback entry is often not describing a live proxy template.

## Conclusion

0.49 proved the runtime laboratory is functional, but the capture point is
**too early** to identify the red material state.

The next diagnostic must move downstream to the exact proxy creation site:

- native creation call around VA 0x00492F38 -> 0x00988B30;
- capture only when the fallback actually reaches creation;
- record the creation arguments;
- capture the returned proxy pointer after the call;
- dump the created proxy object and its renderer/material-related pointers;
- separately instrument the genuine WSCivilianProp creation path and compare
  proxy vs real object in the same run.

This replaces inference from fallback-entry owner/template pointers with direct
observation of the actual created object.

## Project status

- 0.45 remains canonical release behavior.
- 0.46 remains the clean-source diagnostic base.
- 0.47 remains rejected as final behavior.
- 0.48 remains superseded/unvalidated.
- 0.49 remains diagnostic-only and successfully validates the new runtime
  investigation method.
