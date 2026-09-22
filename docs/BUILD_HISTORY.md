# Build history and validation state

This file records the branch decisions that matter for future work. It is intentionally explicit about rejected experiments so they are not accidentally reintroduced.

## Cumulative-build rule

**The newest validated build is always the complete cumulative patch.**

A version number describes the latest cumulative state, not an isolated feature patch.

Therefore:

- V258Y contains all validated changes retained from the original executable through the full validated lineage.
- Future builds must preserve everything already validated unless a specific change is intentionally reverted.
- Experimental/rejected changes are not carried forward.
- If a new candidate is rebuilt from an older clean milestone for safety, all validated changes after that milestone must be re-applied before that candidate can become the next canonical base.
- The newest validated build is the practical install/test target.

## Fully verified cumulative lineage

The untouched retail executable supplied on 2026-09-22 has SHA-256:

`e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`

The complete validated path is:

`Original -> V200 -> V255A -> V257 -> V258G -> V258M -> V258P -> V258W -> V258Y`

A direct `Original -> V258Y` manifest is also stored and was verified to produce the exact current V258Y executable:

`032889675706926c60c54ea2ced31cbb6703b5b4ac9872f4da27413cc9993f4e`

## Validated lineage

- **Original retail EXE**: canonical untouched source for this project.
- **V137**: historical HUD baseline where the dual-layer minimap transform was validated.
- **V138**: diagnostic swap proved the large garage world icon uses `om_garage_AB`; the minimap `mm_garage_AB` remained separate.
- **V139**: rejected for world-icon sizing. It targeted `0x00796949`, which was the minimap-side `mm_*` path.
- **V140A/B**: isolated the real world `om_*` sizing path `0x00797843 -> 0x00790B26 -> 0x00790B34 -> 0x00790B46`. The retained validated world-icon scale became **70%**.
- **V145A / V146A / V146B / V146D**: vertical world-marker anchor probes. Observations: 2.5 placed the garage marker near the foot; 1.0 near the head; 1.25 near the pelvis. **V146D = 0.50** was explicitly validated by the user as perfect.
- **V147**: historical validated base combining world `om_*` size **70%** and vertical world anchor **0.50**, with minimap kept separate.
- **V200**: re-audited cumulative streaming/HUD baseline carrying the retained V147 world-marker work forward, plus Async32, HUD keep-list extension, streaming FIFO queue-full fix, and other earlier validated work.
- **V255A**: V200 plus the validated sniper-scope exception.
- **V257**: validated Max Engine baseline.
- **V258G**: ObjectiveTray correction rebuilt directly from V257.
- **V258M**: restored the validated V137 dual-layer minimap transform.
- **V258P**: clean minimap baseline + refined tutorial/objective placement.
- **V258W**: clean absolute HUD rebuild.
- **V258Y**: **current validated cumulative base**. Minimap final placement is user-validated and frozen.

## V259 diagnostic probes rejected

- **V259A**: changed the existing world-marker 70% factor `0.70 -> 0.80` while it was temporarily misidentified as an objective-only edge margin. User reported the clipped blue item was unchanged. **Rejected.**
- **V259B**: changed shared constant `0x01138348: 95.0 -> 80.0`. User reported no visible change. **Rejected.**
- Canonical base remains **V258Y**.

## Frozen values in V258Y

### Minimap

The minimap must not be changed unless the user explicitly reopens it.

- Native/orange layer: **X = 100, Y = 60**
- Scaleform/black layer: **X = 33.333333333333336, Y = 20**

### World om_* markers retained from V147

- World icon scale: **70%**
- Vertical world-anchor correction: **0.50**
- Minimap `mm_*` assets remain on a separate path.

### Other V258W/V258Y HUD values retained

- Tutorial: X = 35, Y = 30
- ObjectiveTray local X magnitude = 18
- ObjectiveTray local Y magnitude = 49.666666666666664
- Inventory/ammo local X = +91.66666666666667
- Inventory/ammo local Y = +51.666666666666664

## Important rejected branches

- **V258A-E**: ObjectiveTray centering/top-right experiments.
- **V258I/J**: permanently rejected global safe-frame rewrite.
- **V258K/L**: rejected minimap local-translation approach.
- **V258N/O/Q/R/S/T/U/V/X**: iterative placement experiments, not canonical bases.
- **V259A/B**: rejected marker-clamp probes, no visible effect on the reported clipped blue item.

## Key lessons

1. The latest validated build is always cumulative.
2. For world shop/garage icons, distinguish **`om_*` world assets** from **`mm_*` minimap assets**.
3. Do not touch `0x00796949` for world-icon sizing; that historical path affected minimap markers.
4. The validated world-marker sizing path is `0x00797843 -> 0x00790B26 -> 0x00790B34 -> 0x00790B46`.
5. The validated vertical world-anchor correction is a separate surgical hook at `0x0079785D`.
6. Prefer surgical changes over global HUD rewrites.
7. Rebuild from a clean validated base instead of stacking experiments.

## Current open work

Restart from **V258Y** and audit the merchant world icon `om_shop_AB` specifically before any new patch. The frozen minimap must remain untouched.
