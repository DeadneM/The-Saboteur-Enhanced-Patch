# Technical notebook

## Current canonical cumulative build

**V258Y** is the current validated cumulative base.

Original retail EXE SHA-256:
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V258Y EXE SHA-256:
`032889675706926c60c54ea2ced31cbb6703b5b4ac9872f4da27413cc9993f4e`

### Non-negotiable build rule

Every newest validated build includes every retained validated change from the original executable up to that point. Rejected probes are excluded.

The minimap placement is validated and frozen:

- Native/orange X = `100`
- Native/orange Y = `60`
- Scaleform/black X = `33.333333333333336`
- Scaleform/black Y = `20`

## Full repository reproducibility verification

Both the milestone route and the direct Original-to-V258Y manifest reconstruct the exact V258Y SHA-256.

## Merchant / garage world-marker audit recovered from historical work

### Asset families

The shop/garage system keeps world and minimap assets explicitly separate:

- `om_shop_AB`     = world merchant/shop icon
- `mm_shop_AB`     = minimap merchant/shop icon
- `om_garage_AB`   = world garage icon
- `mm_garage_AB`   = minimap garage icon
- `om_garage2_AB`  = second world garage variant
- `mm_garage2_AB`  = second minimap garage variant

Current V258Y string locations:

| Asset | String VA | Raw |
|---|---:|---:|
| om_shop_AB | 0x010483B8 | 0x00C475B8 |
| mm_shop_AB | 0x010483C4 | 0x00C475C4 |
| om_garage_AB | 0x010483D0 | 0x00C475D0 |
| mm_garage_AB | 0x010483E0 | 0x00C475E0 |
| om_garage2_AB | 0x010483F0 | 0x00C475F0 |
| mm_garage2_AB | 0x01048400 | 0x00C47600 |

### Loader proof

Loader routine around `0x009E1710` selects the pair by shop/garage type:

- garage2:
  - `0x009E179C` pushes `om_garage2_AB`
  - `0x009E17C1` pushes `mm_garage2_AB`
- garage:
  - `0x009E17D7` pushes `om_garage_AB`
  - `0x009E17FC` pushes `mm_garage_AB`
- shop/merchant:
  - `0x009E1812` pushes `om_shop_AB`
  - `0x009E1837` pushes `mm_shop_AB`

When the marker record is created:

- `0x009E188F: mov [eax], esi` stores the **world `om_*` asset**
- `0x009E189B: mov [eax+0x0C], edi` stores the **minimap `mm_*` asset**

This confirms the separation structurally, not just by string names.

Historical V138 independently proved this in-game: swapping `om_garage_AB -> om_shop_AB` changed the large world garage icon while the minimap garage icon remained unchanged.

### Validated world-marker size path

Historical V139 targeted `0x00796949` and altered minimap `mm_*` markers, so that path must not be reused for world icons.

The validated world `om_*` path is:

`0x00797843 -> 0x00790B26 -> 0x00790B34 -> 0x00790B46`

Relevant original logic:

- `0x00790B26` resolves the world marker asset
- `0x00790B34` reads the asset width at `[eax+0x18]`
- shifts it right once to get half width
- `0x00790B46` originally multiplies by the runtime marker scale

V258Y retains the historical world-marker hook:

`0x00790B46 -> 0x00681000`

Cave:

- `0x00681000: fmul [esp+0x2B4]`
- `0x00681007: fmul [0x00681020]`
- `0x0068100D: jmp 0x00790B4D`

Constant:

- VA `0x00681020`
- raw `0x00280220`
- value `0.70f`
- bytes `33 33 33 3F`

This is the validated **70% world `om_*` marker scale** carried from the V140/V147 line.

### Validated vertical world-marker anchor

Original code:

- `0x0079785D: fld [eax+0x1C]`
- `0x00797860: fstp [ecx+0x04]`

V258Y retains the historical vertical-anchor hook:

`0x0079785D -> 0x00681100`

Cave:

- `0x00681100: fld [eax+0x1C]`
- `0x00681103: fsub [0x00681130]`
- `0x00681109: fstp [ecx+0x04]`
- `0x0068110C: jmp 0x00797863`

Constant:

- VA `0x00681130`
- raw `0x00280330`
- value `0.50f`
- bytes `00 00 00 3F`

Historical in-game probes:

- V145A, 2.5: garage marker near the foot
- V146A, 1.0: near the head
- V146B, 1.25: near the pelvis
- V146D, 0.50: user verdict **"c'est parfait là"**

V147 therefore became the historical validated world-marker baseline with:

- world `om_*` scale = **70%**
- vertical anchor correction = **0.50**
- minimap kept separate

Those exact two corrections are still present in V258Y.

## V259A / V259B rejected

- V259A changed the existing `0.70` world-marker factor to `0.80` while it had been temporarily misidentified as an objective-only edge parameter. User reported the clipped blue item was unchanged.
- V259B changed `0x01138348: 95.0 -> 80.0`. User reported no visible change.

Neither is part of the canonical state. Restart point is V258Y.

## Current interpretation

The merchant/garage world-icon system itself is no longer unknown. Its asset selection, world/minimap split, 70% size hook, and 0.50 vertical anchor are all identified and already present in the canonical build.

Therefore the next merchant-specific test should not touch:

- minimap `mm_*`
- global HUD safe frame
- `0x01138348`
- the 70% size factor unless intentionally changing icon size

For a request to **raise** merchant/garage world markers, the surgical control is the existing vertical-anchor constant at `0x00681130`. Lowering the magnitude below 0.50 moves the anchor upward relative to the historical validated position.

The separate clipped blue indicator remains a different unresolved item until its exact asset/path is identified.
