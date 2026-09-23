# Build history and validation state

## Cumulative-build rule

**The newest validated build is always the complete cumulative patch.**

Current canonical cumulative build: **V259**

Original retail EXE SHA-256:
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V259 EXE SHA-256:
`8f9883883abab91ae029b078326e93751664204e75ebb2f83044ae014d13fc0b`

## Validated lineage

- **V137**: dual-layer minimap transform validated.
- **V138**: proved world `om_*` and minimap `mm_*` shop/garage assets are separate.
- **V140/V147**: validated world `om_*` scale at 70%.
- **V146D/V147**: historical world vertical anchor 0.50 validated.
- **V200**: cumulative streaming/HUD baseline carrying validated earlier work forward.
- **V255A**: V200 + native 100% sniper-scope exception.
- **V257**: Max Engine baseline.
- **V258G/M/P/W/Y**: ObjectiveTray, dual-layer minimap and final HUD placement lineage.
- **V259**: promotes the validated V259E behavior:
  - general world-marker vertical anchor `0.50 -> 0.25`
  - world scale remains `0.70`
  - dedicated merchant/shop pistol `om_shop_AB` +0.25 Y correction
  - dedicated Resistance HQ `om_HQ_AB` +0.25 Y correction
  - frozen V258Y minimap retained exactly

## V259 candidate history

- **V259A**: rejected. Changed the existing world-marker scale `0.70 -> 0.80`; did not solve the reported clipped blue edge indicator.
- **V259B**: rejected. `0x01138348: 95.0 -> 80.0`; no visible effect.
- **V259C**: successful intermediate test. General world markers were reported otherwise perfect after vertical anchor `0.50 -> 0.25`, but shop pistol and HQ cross still needed dedicated handling.
- **V259D**: rejected/superseded diagnostic.
- **V259E**: validated. Added explicit shop/pistol and HQ/Cross corrections. User verdict: **"oui tres bien on valide"**.
- **V259**: canonical release state identical in behavior to validated V259E.

## Frozen V259 minimap

- Native/orange: X = 100, Y = 60
- Scaleform/black: X = 33.333333333333336, Y = 20

## Validated V259 world-marker values

### Generic world om_* path

- size factor: `0.70`
- vertical-anchor constant:
  - VA `0x00681130`
  - `0.50 -> 0.25`

### Merchant / shop pistol

- asset: `om_shop_AB`
- loader hook: `0x009E18A1 -> 0x00681486`
- extra correction: `Y += 0.25`

### Resistance HQ / Cross of Lorraine

- asset: `om_HQ_AB`
- HQ render hook: `0x007979A8 -> 0x00681400`
- extra correction: `Y += 0.25`

### Shared extra constant

- VA `0x006814A4`
- value `0.25f`

## Current open issue

The separate small blue edge indicator that can be clipped at the screen boundary remains unresolved. It is no longer to be conflated with the validated shop/garage/HQ world-marker paths.

Future work starts from **V259**.
