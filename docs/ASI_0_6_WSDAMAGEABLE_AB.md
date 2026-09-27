# ASI 0.6 WSDamageablePart variant-selector A/B

Status: **test candidate**

Baseline:
- validated Core 1 EXE SHA-256:
  `83995ab6e04fce264309a37a243744d4f5929c5abb87c3f195fb50fef7f5f2a8`
- V310 runtime fix enabled
- V311 runtime fix enabled

## ASI 0.5 result

User result: **no visible change** at the target balcony/facade transition.

Therefore:
- VA `0x00667CC7` zero-result child rejection is not the final visual owner;
- 0.5 is rejected and disabled by default.

## Static follow-up

The next function in the same path is VA `0x006678E0`.

It writes the real child visibility bit at `child+0x4C & 1`.

Before that write, the engine selects between two child groups using:

- parent/state field `this+0x20`, compared to 0 and 1;
- child resource flag `resource+0x28 & 1`.

The surrounding allocation/constructor path is recovered as
`WSDamageable / WSDamageablePart`.

Native selector result:

```asm
mov eax, 1
...
xor eax, eax
and al, byte ptr [esp+24h]     ; VA 0x006679AE
mov cl, byte ptr [esi+4Ch]
...
xor byte ptr [esi+4Ch], cl
```

## A/B patch

ASI 0.6 changes only:

- VA `0x006679AE`
- RAW `0x00266BAE`
- `22 44 24 24 -> 8A 44 24 24`

Meaning:

```asm
and al,[esp+24h]
->
mov al,[esp+24h]
```

The incoming visibility decision is preserved while the local WSDamageablePart
0/1 variant-group selector is bypassed.

Potential A/B side effect:
- mutually exclusive damage/intact child groups may both follow the same
  visibility state, causing duplicate/overlapping geometry.

## Build

- ASI SHA-256:
  `87418a751d62444cfe42a93902038f7a16e355f6d0ed0aaf3258cd753305a0cb`
- dinput8 SHA-256:
  `84c17e0b05749afd61ab9e0711f893d126bda259467f5bf5b42d09618d3b23b1`
- five-file ZIP SHA-256:
  `996684a339308b83dfc064089e74e047d5a36f03eb207ab9866599a81f45ad39`

## Test

No diagnostic key.

Check:
- the known balcony/facade transition;
- destructible facades/buildings for duplicate or overlapping variants;
- obvious visual regressions.

If there is no effect, reject this selector and treat the earlier Odin
correlation as downstream scene-update activity rather than the balcony's
visibility owner.
