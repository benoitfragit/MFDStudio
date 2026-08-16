# HUD Visual References

This folder keeps the visual references used while shaping the HUD
symbology. They are reference-only files and are not runtime assets.

Runtime HUD assets remain under `examples/hud/assets`. Do not wire files from
this folder into `hud_window.json`, generated UI code or packaging scripts.

## Source Documents

- NO-FunnelGunSight algorithm notes used as an external EEGS implementation
  cross-check:
  <https://github.com/mosdef31/NO-FunnelGunSight>
- Turkish F-16 HUD capture used only as a visual shape reference for the EEGS
  funnel corridor:
  <https://www.reddit.com/r/WarplanePorn/comments/ikcmps/turkish_f16_hud_during_interception_of_greek_f16/>

## Files

| File | Used For |
|---|---|
| `hud_aa_reference.png` | Air-to-air EEGS / pipper / TD-circle proportions |
| `page224_hud_ir_missile.png` | HUD IR missile diamond, caged/uncaged size, AIM-9 DLZ and seeker FOV/range marks |
| `page225_hud_ir_tll.png` | HUD target-locator line arrowhead behavior for caged/uncaged correlated AIM-9 |
| `page301_aim9_bore_slave.png` | AIM-9 BORE/SLAVE behavior and HUD/HMCS display rule |
| `page302_hmcs_tll.png` | HMCS A-A target locator line diagrams and HUD example |
| `page304_aim9_caged_uncaged_examples.png` | TGP-tracking examples for caged versus uncaged correlated AIM-9 |
| `hud_ag_strf_reference.png` | STRF reticle composition reference |
| `page410_hud_strafe.png` | Dash-34 STRF page crop used for the 50 mR / 40 mR reticle and in-range cue |
| `hud_ag_ccip_reference.png` | CCIP pipper, bomb-fall line and cue reference |
| `page413_hud_ccip.png` | Dash-34 CCIP page crop used for fall-line and solution-cue placement |

Keep any future screenshots here with a source document/page note so the HUD
HUD can be visually audited without mixing reference material into runtime
assets.
