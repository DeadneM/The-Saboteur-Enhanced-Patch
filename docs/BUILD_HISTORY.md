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

Current cumulative lineage:

`Original EXE -> validated pre-V200 work -> V200 -> V255A -> V257 -> V258G -> V258M -> V258P -> V258W -> V258Y`

The repository currently reproduces the exact byte chain from **V200 to V258Y**. V200 is the surviving re-audited cumulative baseline containing earlier validated work. An untouched retail EXE is still needed to generate and verify a formal `Original -> V200` manifest.

## Validated lineage

- **V200**: re-audited cumulative streaming/HUD baseline. Includes Async32, HUD keep-list extension and streaming FIFO queue-full fix plus previously retained validated work.
- **V255A**: V200 plus the validated sniper-scope exception. Recovered byte-for-byte from surviving artifacts.
- **V257**: validated Max Engine baseline. Conservative visibility/LOD increases with no new hook/cave.
- **V258G**: first sane ObjectiveTray correction rebuilt directly from V257.
- **V258M**: restores the previously validated V137 dual-layer minimap transform and starts final HUD edge cleanup.
- **V258P**: clean minimap baseline + refined tutorial/objective placement.
- **V258W**: clean absolute HUD rebuild from V258P, created specifically to end the additive micro-patch chain while preserving the validated cumulative state.
- **V258Y**: **current validated cumulative base**. Minimap final placement is user-validated and frozen.

## Frozen values in V258Y

### Minimap

The minimap must not be changed unless the user explicitly reopens it.

- Native/orange layer: **X = 100, Y = 60**
- Scaleform/black layer: **X = 33.333333333333336, Y = 20**
- Relationship: 1 HUD-space unit = approximately 3 native/screen units at 4K.

### V258W/V258Y HUD values retained

- Tutorial: X = 35, Y = 30
- ObjectiveTray local X magnitude = 18
- ObjectiveTray local Y magnitude = 49.666666666666664
- Inventory/ammo local X = +91.66666666666667
- Inventory/ammo local Y = +51.666666666666664

## Important rejected branches

- **V258A-E**: ObjectiveTray centering/top-right experiments. Diagnostic value only.
- **V258F**: useful discovery, ObjectiveTray flags `1 -> 0`, but not final by itself.
- **V258I/J**: **permanently rejected**. Global safe-frame rewrite broke multiple HUD systems. V258I also introduced a stack error in the minimap reroute; V258J fixed the stack issue but not the bad global-layout concept.
- **V258K/L**: rejected minimap local-translation approach. One minimap layer moved without the other.
- **V258N/O/Q/R/S/T/U/V/X**: iterative placement experiments. Useful measurements, not canonical bases.

## Key lessons

1. The latest validated build is always cumulative.
2. Prefer surgical changes over global HUD rewrites.
3. A Flash root coordinate is not necessarily the visible edge of the graphic because clips have different registration points/internal padding.
4. The minimap is dual-layer. The black Scaleform layer and native/orange map layer must move by equal **physical** amounts, not equal numeric values.
5. Rebuild from a clean validated base instead of stacking many micro-adjustments.
6. If rebuilding from an older milestone, restore every later validated change before promoting the result.
7. Every future build README is a technical notebook: base hash, exact values, addresses, formulas, user result, regressions, frozen systems, and next hypotheses.

## Current open issue

A blue floating/off-screen indicator at the left edge can be partly clipped. The next surgical task is to identify and reduce its maximum screen-edge clamp/radius without touching the frozen V258Y minimap.
