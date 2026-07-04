# HUD Visual References

This folder keeps the visual references used while shaping the demo HUD
symbology. They are reference-only files and are not runtime assets.

Runtime HUD assets remain under `examples/hud/assets`. Do not wire files from
this folder into `demo_hud_window.json`, generated UI code or packaging scripts.

## Source Documents

- Falcon BMS Dash-34:
  <https://cdn.falcon-bms.com/docs/4.37/TO%201F-16CMAM-34-1-1%20BMS.pdf>
- Falcon BMS Training Manual:
  <https://cdn.falcon-bms.com/docs/4.37/BMS-Training-Manual.pdf>

The copied PNG files were taken from the local public reference set at:

```text
C:\Users\33761\Documents\Projets\F16-BMS\datapackage\public_references\views
```

## Files

| File | Used For |
|---|---|
| `hud_aa_reference.png` | Air-to-air EEGS / pipper / TD-circle proportions |
| `bms_public_page_398_sms_gun.png` | SMS gun context and gunnery mode reference |
| `hud_ag_strf_reference.png` | STRF reticle composition reference |
| `page410_hud_strafe.png` | Dash-34 STRF page crop used for the 50 mR / 40 mR reticle and in-range cue |
| `hud_ag_ccip_reference.png` | CCIP pipper, bomb-fall line and cue reference |
| `page413_hud_ccip.png` | Dash-34 CCIP page crop used for fall-line and solution-cue placement |

Keep any future screenshots here with a source document/page note so the demo
HUD can be visually audited without mixing reference material into runtime
assets.
