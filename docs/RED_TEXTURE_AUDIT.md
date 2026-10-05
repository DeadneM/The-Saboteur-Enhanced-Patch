# Red Civilian Prop / Texture Bug Audit

## Symptom

In occupied black/white/red zones, some distant civilian-held metallic props can
appear as conspicuous red proxy rectangles/objects before later resolving to the
normal accessory.

## Proven ownership

The affected family was isolated to `PGA_HHProp_*`.

The decisive runtime proof is the WSHumanSpore fallback:
`rnd civilian prop(%d)`.

Normal path:
- table lookup at VA 0x00492D95..0x00492DCF;
- valid entry is consumed by VA 0x00492DD5..0x00492DFE.

Fallback path on lookup miss:
- VA 0x00492E03 / RVA 0x00092E03;
- fallback template `[ebx+0x1384] -> [+0x2DC]`;
- string `rnd civilian prop(%d)`;
- creation around VA 0x00492F38 -> 0x00988B30.

V243A proved that disabling only this fallback changes the distant red proxy to
no proxy, while the genuine normal prop still appears once available.

## Rejected explanations / fixes

Do not repeat:
- global HHProp shader recoloring or whitening;
- flag 0x10 / 0x40 removal as final fix;
- generic LOD/distance increases;
- WSCivilianProp 125 -> 500/1000;
- resource-bank eviction suppression;
- Into/Outo transition suppression;
- WSWillToFightGrid 1024;
- generic WTF function suppression;
- SliceQuality changes that hide red by breaking scenery.

## 0.47 surgical candidate

0.47 suppresses only the incorrect fallback proxy.

RVA 0x00092E03:
- expected `85 DB 0F 84 3B 01 00 00`
- patch `E9 3E 01 00 00 90 90 90`
- native destination VA 0x00492F46.

The real WSCivilianProp path is unchanged.

Validation target:
1. same occupied-zone route where red proxy was reproducible;
2. distant red proxy absent;
3. genuine accessory still appears when close/available;
4. no pedestrian, save/load, zone-transition or crash regression.

If validated, this becomes the release behavior. If the temporary no-prop
interval is considered unacceptable, the next research branch is to accelerate
population of the native civilian-prop table rather than recolor the proxy.

## 0.47 in-game result

The proxy bypass removed the red bug but also removed pedestrian hand props entirely. Therefore suppression of `rnd civilian prop(%d)` is diagnostic only, not a shippable fix.

## 0.48 branch

Restore the native proxy and test the historical CivilianProp gate redirect:
- table VA 0x00B85C80 / RVA 0x00785C80;
- retail pointer 0x00474A20;
- alternate native pointer 0x0048B560.

This branch preserves the prop system and tests whether earlier access to the normal CivilianProp handler can prevent the red proxy phase without deleting accessories.


## 0.49 runtime audit architecture

0.48 is superseded without validation. The modern investigation now uses one
runtime laboratory rather than static A/B rebuilds.

Base behavior: clean 0.46.

Hook:
- RVA `0x00092E03`;
- expected bytes `85 DB 0F 84 3B 01 00 00`;
- pass-through trampoline reproduces the native `test ebx,ebx / je` semantics.

Hotkeys:
- F6 native proxy;
- F7 suppress proxy;
- F8 verbose native;
- F9 snapshot latest fallback context.

Captured chain:
- `EBX`;
- `[EBX+0x1384]`;
- `[[EBX+0x1384]+0x2DC]`;
- actor/owner/template dword snapshots;
- real-prop table cell at module base + `0x00E129E0`.

Goal: identify the proxy-specific runtime state responsible for the red
appearance, then patch that property only. The proxy is no longer deleted as a
proposed final fix.
