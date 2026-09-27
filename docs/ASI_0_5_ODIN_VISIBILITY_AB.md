# ASI 0.5 Odin child visibility A/B

Status: **test candidate**

## Why this replaces the fingerprint branch

ASI 0.3 already established repeatable correlation between the balcony/facade
transition and the Odin/Win32Mesh path.

Further static reverse engineering found the actual child visibility gate, so
additional mesh fingerprinting is not required for the next A/B.

## Values now proven on the path

The broad High SliceQuality table is not the remaining balcony limiter.

Core/retail High table starts at VA `0x01120AD8`:

- record 0: 0.1 -> 4
- record 1: 4 -> 20
- record 2: 20 -> 50
- record 3: 50 -> 100
- record 4: 80 -> 500

V302 had already expanded the effective chain to approximately:
- 24.9765625
- 124.8828125
- 312.20703125
- 1873.2421875
- 9366.2109375

The balcony/facade transition was still present, therefore this family is
excluded as the remaining gate.

## Actual child gate

The repeat/instancing path reaches VA `0x00667C40`.

At VA `0x00667C59` it calls a virtual method returning a float.

At VA `0x00667C60` that value is compared with the exact `0.0f` constant at
VA `0x010A45B0`.

The resulting zero/non-zero state is propagated as a child visibility boolean
through VA `0x006678E0`, then into the Odin/Win32Mesh update path and
OdinMeshInstance state at `this+0x35`.

Relevant branch:

```asm
cmp byte ptr [esp+20h], 0
je  invisible
cmp byte ptr [esp+24h], 0
jne invisible              ; VA 0x00667CC7 = 75 04
mov al, 1
jmp done
invisible:
xor al, al
```

## A/B patch

ASI 0.5 changes only:

- VA `0x00667CC7`
- `75 04 -> 90 90`

Effect:
- preserve the parent-visible condition;
- bypass only the child rejection caused by the exact zero-result gate;
- no global always-visible policy;
- no Odin diagnostic hooks enabled by default;
- no F9 marker or fingerprint logging.

## Build

- `SaboteurEnhanced.asi` SHA-256:
  `ee829fcae89d7e63de2fc3f6c4f8f39f4d9c3526bc0571ca719fd4cd103e160c`
- `dinput8.dll` SHA-256:
  `04cd92ebfcb35843c911c4c34f614bd19fce6906a025c43e7e9470f8b3a724fd`
- five-file test ZIP SHA-256:
  `704ba0ec8aba667dfe7ac7b959f87add799472991ea730eece1ac9c1028dfe93`

## Test

Reproduce the balcony/facade transition normally.

No special key or logging procedure is required.

Observe:
- whether the late appearance/disappearance is removed or moved farther away;
- repeated facade/window/balcony geometry;
- performance or obvious over-retention regressions.

If successful, the next task is to identify the upstream float producer and
replace this A/B with a proper distance/LOD policy if necessary.
