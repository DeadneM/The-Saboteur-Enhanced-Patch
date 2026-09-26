# V200 region-by-region retention audit

Historical source: compressed `Original -> V200` manifest.

- Retail SHA-256: `e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6`
- V200 SHA-256: `9d13022f1e889e5aeb16e97b6012c300ffb7acfa0b72b90971143fe7447fccaf`
- Historical V200 diff: **211 changed regions / 5,274 bytes**
- Clean-Core selection from this diff: **4,595 bytes**
- Removed from the V200 layer: **679 bytes**

The byte count is still large because HUD scalers, lookup strings and code caves
occupy substantial space. This does **not** mean the cleaned Core retains most
of the old engine tuning.

## Retain in Core 1

### Super RDX save/reload correctness fix

The old IPS label refers to `SuperRDX_Fix.ips`, not a forced FPS/VSync patch.

Retain:
- region 0: RAW `0x00002856`, 15 bytes
- region 1 **prefix only**: RAW `0x00002866..0x0000288C`, 39 bytes
- hooks:
  - region 82 RAW `0x0029878C`
  - region 83 RAW `0x00299268`
  - region 84 RAW `0x00299A4D`

The remainder of historical region 1 is deliberately **not** inherited.

### Hor+ FOV

Retain the surgical runtime Hor+ implementation around the Odin camera FOV path:

- region 2 RAW `0x00002923`
- region 3 RAW `0x00002965`
- region 149 RAW `0x00A04244` / VA approximately `0x00E05044`

Original VSync remains intact. No forced `D3DPRESENT_INTERVAL_IMMEDIATE` /
uncapped-FPS experiment is retained.

### Borderless / desktop window / display integration

Retain the proven native window-management path:

- region 15 RAW `0x00007B76`
- region 16 RAW `0x00007BC0`
- region 17 RAW `0x00007C10`
- region 18 RAW `0x00023E40`
- region 19 RAW `0x00023E54`
- region 22 RAW `0x00028BEE`
- region 23 RAW `0x00028FBD`
- region 24 RAW `0x00028FC7`
- region 49 RAW `0x00115386`
- region 50 RAW `0x00115510`
- region 51 RAW `0x001155C0`
- region 147 RAW `0x009F4BB5`
- region 148 RAW `0x009F4D30`

This family includes the validated D3D9 CreateDevice/Reset conversion, desktop
dimension handling, frameless window behavior, and display-mode integration.

Region 17 contains two retained helpers:
- desktop/system-metric handling near RAW `0x00007C10`
- `winmm.dll!timeBeginPeriod(1)` loader near RAW `0x00007C40`

The timer helper is reached by the retained hook:
- region 150 RAW `0x00A4B6D5`

### High-resolution HUD / menus

Retain:
- region 25 RAW `0x000E6E26` (button-prompt/high-resolution UI path)
- regions 62-71 RAW `0x00262800..0x0028027D`, excluding the unrelated
  streaming bytes split out below
- regions 73-75 RAW `0x00280300..0x00280627`
- regions 103-113 RAW `0x0038B28C..0x003A58EE`
- region 129 RAW `0x005BCEC0`
- region 130 RAW `0x005C1F1D`

The large scaler cave contains the named fields for:
- Tutorial
- Mail
- ObjectiveTray
- Inventory
- RaceHUD
- Pickup
- Minimap

Later V258Y deltas refine the final positions and are replayed separately.

### Caps Lock HUD toggle

Retain the object-level HUD toggle architecture:

- region 65 RAW `0x00262970`
- region 66 RAW `0x002629D0`
- region 71 toggle/whitelist table content
- region 77 RAW `0x00285BB4`
- related HUD hooks in regions 106 and 130

The final behavior protects required screens, including `WSHUDBladeScreen`,
while gameplay HUD objects such as `WSHUDSabotage` remain hideable.

### DirectInput / native Alt+F4

Retain:
- region 76 RAW `0x002859F8`: `0x05 -> 0x06`

This changes the keyboard cooperative mode from
exclusive/foreground to nonexclusive/foreground. Windows can then process
Alt+F4 normally. There is no separate WndProc/WM_CLOSE hack.

### Reticle scaler

Retain:
- regions 78-81 RAW `0x0028F2D6..0x0028F3FE`
- hook region 87 RAW `0x003739A2`

The historical V200 scaler uses 100/95/90/70% bands. The later V255A delta is
also retained because it exempts the actual sniper `scope` from that 4K
downscale and leaves it at native 100%.

### GFx / Stats UI 4K pool

Retain regions 88-102:

- RAW range `0x0037614F..0x00378931`
- logical slots 1024 -> 2048
- internal elements 3072 -> 6144
- pool 0x120000 -> 0x240000
- renderer object 0x1202B8 -> 0x2402B8
- trailing field moves from +0x1202B4 to +0x2402B4

This is the validated V75 Stats/UI 4K renderer fix, not a generic speculative
pool increase.

### Streaming FIFO correctness only

Retain only the drop-oldest correction for the native 64-entry ring:

- region 72 **subrange only**:
  RAW `0x00280284` onward, 21 bytes
  - loads queue index at +0x110
  - increments it
  - `and esi, 0x3F`
  - stores the wrapped index
- hook region 141 RAW `0x009B4C77`

Do **not** retain other streaming tuning merely because it existed in V200.

## Remove from Core 1

### Streaming performance tuning

Excluded:
- historical Async 16 -> 32 path, including the middle section of region 1
  beginning around RAW `0x0000288D`
- other historical streaming helper in the tail of region 1
- 32 MiB streaming-buffer increase at the `0x009B4EE9` immediate
  (changed bytes represented by region 142)
- regions 143-146 related to the old streaming/coalescing/async experiments
- region 128 RAW `0x00586C9A`: `Sleep(1) -> Sleep(0)`

The ASI will measure actual queue pressure and stalls before any of these return.

### Old WSModel / LOD experiments

Excluded:
- regions 4-14 except the retained FOV regions listed above
- regions 20-21
- regions 26-48
- regions 52-60
- regions 85-86

Important finding: many of the pointer redirections in
RAW `0x000F0970..0x0010A447` point into a local table containing
`20/40/56/80/120/160/240`. These are engine/object-distance data, not HUD.

The later V310/V311 fixes are cleaner and will live in the ASI.

### General engine / renderer tuning

Excluded from Core 1 unless later telemetry proves necessity:
- regions 114-127
- regions 132-139
- regions 151-210

This group contains old graphics/cache/quality/LOD/resource tuning accumulated
during the research branch.

Specific exclusions include:
- palettepack `500 -> 0` at region 131 RAW `0x005EE946`
- V200 StreamCoverage-style table changes
- old shadow/environment/cache/AF/CSM-related scalar tuning
- broad quality and distance table edits

### Unknown/unproven one-off V200 changes

Region 61 and any other unclassified scalar that lacks a demonstrated
user-facing dependency are excluded under the new rule:

> unknown is not equivalent to essential.

## Later retained deltas after the cleaned V200 layer

Core 1 then replays only:

1. **V200 -> V255A**  
   Retain the complete 16-byte sniper-scope exception delta.

2. **Skip V255A -> V257**  
   The complete Max Engine scalar delta is omitted.

3. **V257 -> V258G -> V258M -> V258P -> V258W -> V258Y**  
   Replay the HUD deltas. Static overlap verification shows these regions do
   not overlap any V257 Max Engine patch site, so they can be applied to the
   cleaned V255A-derived Core with per-region before-byte checks.

4. **V258Y -> V259**  
   Replay the validated world-marker corrections.

5. **Stop.**  
   V260 and later engine-capacity/distance work is not part of Core 1.

## Machine-readable reconstruction

See:

`patches/core1/core1_recipe.json`

The recipe contains exact source before/after bytes for the selected
Original->V200 regions, the two split subranges, and the retained later HUD
deltas.

Its target SHA-256 is intentionally not invented. The exact retail executable
bytes are required to apply the recipe and calculate the real Core 1 hash.
