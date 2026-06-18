# T12 — De-dupliquer `TitleReticleCache` entre les deux renderers

- **Gravite** : P2 (duplication exacte / risque de derive)
- **Modules** : `mfd_api/src/render/MfdRenderer.cpp`, `mfd_runtime_api/src/internal/OffscreenPageRenderer.cpp`
- **Vague CI** : 3
- **Statut** : CONFIRME

## Constat

La structure `struct TitleReticleCache` est **byte-identique** dans les deux fichiers :
- `MfdRenderer.cpp:193`
- `OffscreenPageRenderer.cpp:113`

(5 champs : `pageName`, `pageTitle`, `display`, `reticle`, `valid`.) Le helper de
dessin du title-reticle (`MfdRenderer.cpp:159-188` / `OffscreenPageRenderer.cpp:90-108`)
et le bloc d'invalidation de cache (`MfdRenderer.cpp:411-418`) sont egalement dupliques.

Note AGENTS.md : ces deux structs sont membres de leur `Impl` respectif (pas
strictement locales a une fonction), donc l'anti-pattern « struct locale dans une
fonction » n'est pas viole ici ; le probleme est la **duplication inter-fichiers**.

## Cause / impact

Toute correction du cache de title-reticle doit etre faite deux fois ; risque de
divergence silencieuse entre rendu live et rendu offscreen.

## Correction recommandee (minimale, comportement constant)

Extraire `TitleReticleCache` + le helper de dessin + la logique d'invalidation dans
un entete interne partage (ex. `mfd_api/.../render/TitleReticleCache.h`, zone interne)
et l'inclure depuis les deux renderers. Garder les details d'implementation internes
(regle AGENTS.md « conserver les details en .cpp ou types internes », pas d'expansion
d'API publique).

## Test de non-regression

`tests/` rendu : un cas couvrant page avec title-reticle, rendu via les deux chemins
(live + offscreen), produisant un resultat identique ; verifier l'invalidation quand
le nom/titre de page change.
