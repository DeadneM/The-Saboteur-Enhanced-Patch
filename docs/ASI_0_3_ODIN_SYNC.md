# ASI 0.3 Odin sync correlation diagnostic

Status: **test candidate**

Baseline:
- validated Core 1 EXE SHA-256:
  `83995ab6e04fce264309a37a243744d4f5929c5abb87c3f195fb50fef7f5f2a8`
- V310 runtime fix remains enabled
- V311 runtime fix remains enabled

## Why 0.3 exists

ASI 0.2 proved that broad OdinMeshInstance lifecycle tracing is active but too
noisy to correlate the remaining balcony/facade transition with one object.

The actual synchronization routine is VA `0x00E14610`.

Its control flow is now statically established:
- GetRoot through OdinMeshInstance virtual slot 3
- query virtual slot 2 on the root
- compare root state against instance `this+0x44`
- call ReInstance through virtual slot 1 only on mismatch

The function returns with `ret 8` and is called from exactly three identified
sites:
- VA `0x0066BA4A`, return RVA `0x0026BA4F` = batch-A
- VA `0x0066BAC8`, return RVA `0x0026BACD` = batch-B
- VA `0x0066C36F`, return RVA `0x0026C374` = batch-C

## Hook design

ASI 0.3 installs one inline pass-through detour at RVA `0x00A14610`.

Verified original 9-byte prologue:
```text
83 EC 0C 56 57 8B 7C 24 1C
```

The detour uses a 9-byte absolute x86 `push imm32 ; ret` transfer, avoiding
rel32 range dependence. A trampoline replays all 9 original bytes and returns to
the untouched original function body.

No Odin rendering decision is changed.

## Logged mismatch data

Only root/instance state mismatches are logged:
- OdinMeshInstance pointer
- batch-A / batch-B / batch-C caller
- entry list pointer and count
- root pointer
- root vtable pointer
- root virtual-slot-2 state
- instance loaded state at `this+0x44`
- `this+0x2A`
- `this+0x35`
- segment count at `this+0x28`
- state after the original synchronization routine runs

## F9 visual marker

ASI 0.3 adds a diagnostic-only F9 marker thread.

Press F9 when the target balcony/facade detail visibly appears or disappears.

The log receives:
```text
[MARK] F9 #1
```

This provides a human visual timestamp that can be correlated directly against
nearby Odin sync mismatches.

## Build

GitHub Actions Win32/x86 build passed.

- `SaboteurEnhanced.asi` SHA-256:
  `7916b14753327fea5a2397d3654e6826c4ea004b47cb0edf3dc47107bc2c2191`
- `dinput8.dll` SHA-256:
  `60fed9a57205819722686fc851beb03c86e3a79559f0387e1ef384181b03a11f`
- test ZIP SHA-256:
  `76f950e4aee986c59eb71fbd125e1500e22839842c6843abca7318fdcb65da2c`

Both compiled binaries are PE machine 0x14C / x86.

## Test protocol

At the same balcony location:
1. approach until the detail visibly changes;
2. press F9;
3. move away until it changes back;
4. press F9;
5. repeat at least three times;
6. exit normally;
7. send `SaboteurEnhanced.log`.

Decision:
- mismatch bursts tightly surrounding the F9 markers -> identify repeated
  object/root pairs and continue into the owning high-resolution state gate;
- no mismatch correlation -> reject OdinMeshInstance sync as the visual owner
  and move to the next renderer layer.
