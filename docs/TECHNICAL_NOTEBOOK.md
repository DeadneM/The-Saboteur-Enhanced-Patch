# Technical notebook

## Current canonical cumulative build

**V259**

Original retail EXE SHA-256:
`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

V259 EXE SHA-256:
`8f9883883abab91ae029b078326e93751664204e75ebb2f83044ae014d13fc0b`

V259 promotes the in-game validated V259E behavior unchanged.

## Cumulative rule

Every future candidate starts from V259, or must reproduce V259 exactly before adding any experiment.

## Frozen minimap

- Native/orange X = `100`
- Native/orange Y = `60`
- Scaleform/black X = `33.333333333333336`
- Scaleform/black Y = `20`

## World-marker system

World and minimap shop/garage assets are separate:

- `om_shop_AB` / `mm_shop_AB`
- `om_garage_AB` / `mm_garage_AB`
- `om_garage2_AB` / `mm_garage2_AB`
- `om_HQ_AB` / `mm_HQ_AB`

### Generic world-marker scale

Validated path:
`0x00797843 -> 0x00790B26 -> 0x00790B34 -> 0x00790B46`

Hook:
`0x00790B46 -> 0x00681000`

Scale constant:
- VA `0x00681020`
- value `0.70f`

### Generic world-marker vertical anchor

Hook:
`0x0079785D -> 0x00681100`

V259 value:
- VA `0x00681130`
- raw `0x00280330`
- `0.25f`

This replaces the former historical `0.50f` anchor.

### Merchant / shop pistol

Asset:
`om_shop_AB`

Loader identifies type 0 as shop.

V259 hook:
`0x009E18A1 -> 0x00681486`

Behavior:
- only type 0 receives extra `Y += 0.25`
- garage types 1 and 2 do not receive this extra shop-only correction

### Resistance HQ / Cross of Lorraine

World asset:
`om_HQ_AB`

Minimap asset:
`mm_HQ_AB`

HQ manager global:
`0x0143CB64`

V259 hook:
`0x007979A8 -> 0x00681400`

Behavior:
`Y += 0.25`

### Shared extra constant

- VA `0x006814A4`
- value `0.25f`

## V259 validation sequence

- V259C fixed the general world-marker height and was reported otherwise perfect.
- Two blue icon families remained: shop/pistol and HQ/Cross.
- V259E added explicit handling for both.
- User validated V259E.
- V259 is identical in behavior to V259E and is now canonical.

## Rejected recent probes

- V259A: world-marker scale 0.70 -> 0.80, rejected.
- V259B: 95.0 -> 80.0 at 0x01138348, rejected.
- V259D: superseded diagnostic.

## Reproducibility

`V258Y -> V259` manifest:
- changed bytes: 69
- regions: 6

Direct `Original -> V259` manifest generated and locally verified:
- changed bytes: 5,754
- regions: 221
- target SHA-256 exactly matches canonical V259

## Open issue

The separate clipped blue edge indicator remains unresolved and must be audited independently. Do not reuse the shop/garage/HQ marker assumptions for it without proof.
