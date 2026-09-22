# V259A TEST - Off-screen objective marker edge clamp

**Status: pending in-game validation.**  
**Canonical base remains V258Y.**

## Goal

Fix the blue circular floating/off-screen objective marker being slightly clipped at the screen edge, without moving or rescaling the validated V258Y minimap.

## Static identification

The floating blue marker belongs to the objective-marker path:

- asset: `om_objective_blue_AB`
- asset load around VA `0x00791C56`
- marker asset selector around VA `0x00789B40`
- objective update path around VA `0x007911D0`
- on-screen test: call `0x00789070`
- off-screen rendering path: call `0x00790A20`

The separate circular clamp at `0x0078A9D0` using `0x01138348 = 95.0` was **not** changed because it is tied to minimap geometry.

## Existing cumulative edge-margin hook

At VA `0x00790B46`, the cumulative patch redirects the original marker half-width calculation to a cave at VA `0x00681000`.

That cave multiplies the marker edge allowance by the float at:

- VA `0x00681020`
- raw `0x00280220`

V258Y value:

- `0.700000`
- bytes `33 33 33 3F`

V259A test value:

- `0.800000`
- bytes `CD CC 4C 3F`

Only three actual bytes differ from V258Y.

## Candidate SHA-256

`760014fcd43ae9c78b6c6d63f56aedd3a480901cf4b2d0b8908ed397378c6ea6`

## Frozen minimap verification

Unchanged:

- Scaleform X = `33.333333333333336`
- Scaleform Y = `20`
- Native X = `100`
- Native Y = `60`

## Expected result

The off-screen objective marker should sit several pixels farther inside the viewport so the complete circular rim remains visible.

The marker graphic scale itself should remain visually unchanged.

## Promotion rule

If the user validates this candidate:

- promote to canonical V259;
- create `V258Y -> V259` delta manifest;
- create direct `Original -> V259` cumulative manifest;
- update hash ledger, build history and technical notebook;
- update the Windows patcher target to V259.

If rejected, V258Y remains canonical.
