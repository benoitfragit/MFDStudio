# LHLD Visual References

This folder keeps the visual references used while shaping the LHLD page
symbology (Radar/FCR, SMS, NAV/HSD and A-G). They are reference-only files and
are **not** runtime assets.

Runtime MFD assets remain under `examples/lhld/assets`. Do not wire files
from this folder into `lhld_window.json`, generated UI code or packaging
scripts.

## Reference Notes

The demo Radar/FCR B-scope, SMS stores page, NAV/HSD compass rose and A-G bomb
profile are modeled on Falcon BMS Block 50-style reference material. Start from the local
datapackage, especially the public reference crops under
`public_references\views`, so the demo can be visually audited without mixing
reference material into runtime assets.

The HSD center-symbol pass used
`C:\Users\33761\Documents\Projets\F16-BMS\datapackage\public_references\views\hsd_reference.png`
as the local source image.

## Suggested files

| File | Used For |
|---|---|
| `bms_sms_ag.png` | BMS-like SMS A-G page: color, text density, profile fields |
| `dash34_sms_ag_page413.png` | Dash-34 SMS A-G crop: CCIP/A-G layout proportions |
| `dash34_sms_sj_page394.png` | Dash-34 SMS S-J crop: jettison-state vocabulary and station emphasis |
| `bms_hsd.png` | BMS-like NAV/HSD compass rose, ownship, steerpoints and bullseye |
| `hsd_ownship_reference.png` | Local datapackage HSD crop used to audit the center ownship symbol |
| `dash34_hsd_page83.png` | Dash-34 HSD page crop: labels, range rings and OSB-adjacent captions |
| `planform_trace.png` | Planform trace used to keep the SMS aircraft silhouette symmetric |

Keep any future screenshots here with a source document/page note.
