# Technical notebook

## Current canonical build

**V258Y** is the current validated base.

Executable SHA-256:
`032889675706926c60c54ea2ced31cbb6703b5b4ac9872f4da27413cc9993f4e`

The minimap placement is validated and **frozen**:

- Native/orange X = `100`
- Native/orange Y = `60`
- Scaleform/black X = `33.333333333333336`
- Scaleform/black Y = `20`

Do not alter these in future builds unless explicitly requested.

## Engine baseline: V257 Max Engine

V257 is built directly from V255A and changes only 22 bytes. Key values:

- StreamCoverage: `6000/1200/1000 -> 8000/1600/1250`
- Object LOD: `20/40/56/80/120/160/240 -> 25/50/70/100/150/200/300`
- Foliage: `200 -> 250`
- Shadow-caster floors: `180 -> 240`
- Particle LOD: `100 -> 150`
- FarScene x3: `250 -> 320`
- Decal visibility squared: `14400 -> 25600`

Intentionally retained:

- streaming buffer 128 MiB
- async reads 32
- merged read 8 MiB
- coalescing 384 KiB
- V200 queue-full fix
- palettepack 0
- shadow maps 4096
- environment maps 2048
- AF16
- CSM 5
- native 100% sniper scope exception from V255A

Do not blindly increase the following without a new audit:

- streaming buffer above 128 MiB
- async above 32
- envmaps above 2048
- shadow maps above 4096
- thread worker count
- RenderSlice globally
- generic/simple caps without reachability proof
- population/crowd limits without isolation

Future engine audit targets after HUD cleanup include real internal caps/pools/drawlists/queues such as OdinDrawlist, DetailObjects Drawlist, Foliage, Decals Drawlist, ParticleDrawlist, WSSimpleRenderObject, WSSceneBox, WSStreamingManager and scene-submission caps.

## HUD architecture discoveries

### Generic high-resolution scaler

The active custom HUD scaler is around VA `0x006809C4`.

Descriptor fields:

- `+0x00` xscale
- `+0x04` yscale
- `+0x08` x
- `+0x0C` y
- `+0x10` width
- `+0x14` height
- `+0x18` anchor flags

Anchor flags:

- bit 0 = right
- bit 1 = bottom

Known descriptors:

- Tutorial raw `0x0027FD28`, flags 0
- Mail raw `0x0027FD44`, flags 2
- ObjectiveTray raw `0x0027FD60`, flags location `0x0027FD78`
- Inventory raw `0x0027FD7C`, flags 3
- RaceHUD raw `0x0027FD98`, flags 3
- Pickup raw `0x0027FDB4`, flags 3

At 4K, the scaler selects 70%. Width thresholds observed in the active path around VA `0x00681380`:

- `<1000`: 100%
- `1000-1399`: 90%
- `1400-1999`: 80%
- `>=2000`: 70%

### ObjectiveTray

Useful discovery from V258F:

- raw `0x0027FD78`: flags `1 -> 0`

This stops the custom width-based post-layout right-edge repositioning while preserving scale.

V258G also neutralized an earlier WSHUDManager X offset at:

- VA `0x009BDAEE`
- raw `0x005BCCEE`

### Minimap

The minimap is not a normal generic HUD root. It has two synchronized layers:

1. Scaleform/black outer HUD layer
2. Native/orange map layer

Historical V137 established the correct strategy: transform both around the same bottom-left reference. Moving only one layer creates the characteristic bug where the orange map no longer follows the outer black circle.

Current V258Y uses the 1:3 coordinate-space relationship described above.

## Current next task: blue floating indicator

Observed symptom: a blue floating/off-screen marker at the left edge is slightly clipped by the screen boundary.

Desired approach:

- surgical only
- identify the actual screen-edge clamp / maximum radial distance for this marker
- reduce that maximum slightly so the full circular graphic stays inside the viewport
- do **not** move or rescale the frozen minimap
- do **not** apply a global HUD safe-frame rewrite

Any new candidate must document the exact code path, constants, old/new values, and test result before it becomes a base.
