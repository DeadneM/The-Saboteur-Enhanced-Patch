# ASI 0.4 Win32Mesh fingerprint diagnostic

Status: **test candidate**

Baseline:
- validated Core 1 EXE SHA-256:
  `83995ab6e04fce264309a37a243744d4f5929c5abb87c3f195fb50fef7f5f2a8`
- V310 runtime fix enabled
- V311 runtime fix enabled
- ASI 0.3 Odin sync correlation confirmed

## Objective

Convert the session-local Win32Mesh root addresses correlated by ASI 0.3 into a
stable structural/resource identity.

ASI 0.3 showed that the same root groups recur immediately before repeated
visual transitions. Every correlated root used vtable `0x0108198C`, recovered
as **Win32Mesh**.

## Runtime design

ASI 0.4 keeps the proven inline sync detour at RVA `0x00A14610` and the F9
visual marker.

It maintains a 256-entry ring of recent Odin synchronization mismatches.

When F9 is pressed, every unique root observed within the configured time window
is fingerprinted.

Default configuration:

```ini
FingerprintWindowMs=1200
FingerprintMaxRoots=16
```

### Fingerprint fields

For each root:
- root pointer;
- Win32Mesh vtable;
- raw FNV-1a hash over the first 0xC0 bytes;
- structural FNV-1a hash with readable pointer-valued dwords normalized by
  field position;
- last OdinMeshInstance pointer;
- caller batch;
- entry count;
- segment count;
- six rows of the first 0xC0 bytes as 32-bit values.

The diagnostic also probes direct pointers found in those root fields:
- printable ASCII strings are emitted as `[FPRINT-STR]`;
- a direct pointer to an `AHSM` mesh wrapper emits the mesh/resource name at
  header +0x14 as `[FPRINT-NAME]`.

No render, LOD, streaming or Odin state is changed.

## New log prefixes

- `[FPRINT]`
- `[FPRINT-DW]`
- `[FPRINT-STR]`
- `[FPRINT-NAME]`

## Build

GitHub Actions Win32/x86 build passed.

- `SaboteurEnhanced.asi` SHA-256:
  `0936d7534821db66c586148df6264b702caeb0decb60870208bf8aa00becc1a4`
- `dinput8.dll` SHA-256:
  `3d7435365b06dce7344cb239e7e2dd4424d6da856a2cdc074d7609472f24576d`
- complete five-file test ZIP SHA-256:
  `a633b844246a9bc15a464f37840ebeb487bd25c5d1b898f6188f92fe9400c726`

Both runtime binaries are PE machine `0x14C` / x86.

## Test protocol

At the same balcony/facade transition:

1. approach until the visible detail changes;
2. press F9 immediately;
3. move away until it changes back;
4. press F9 immediately;
5. repeat two or three cycles;
6. exit normally;
7. send `SaboteurEnhanced.log`.

Success criterion:
- the same structural fingerprint and/or asset/resource string occurs on the
  same transition direction repeatedly;
- ideally the same fingerprint survives a fresh game restart.

Once that happens, the next branch can target the exact owning Win32Mesh or its
resource-state gate instead of broad Odin/renderer behavior.
