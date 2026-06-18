# T12 — De-dupliquer `TitleReticleCache` entre les deux renderers

- **Criticite** : P2 (duplication exacte / risque de derive)
- **Vague CI** : 3
- **Statut** : CONFIRME

## Description breve

`struct TitleReticleCache` est byte-identique dans les deux renderers, ainsi que le
helper de dessin du title-reticle et la logique d'invalidation de cache. Toute correction
doit etre faite deux fois (risque de divergence live vs offscreen).

## Fichiers impactes

- `mfd_api/src/render/MfdRenderer.cpp` (struct l. 193 ; helper l. 159-188 ; invalidation l. 411-418)
- `mfd_runtime_api/src/internal/OffscreenPageRenderer.cpp` (struct l. 113 ; helper l. 90-108)
- nouvel entete interne partage (ex. `mfd_api/.../render/TitleReticleCache.h`, zone interne)
- `tests/` rendu

## Contrainte de dev (AGENTS.md)

- « ne pas dupliquer les regles » ; « centralized [...] helpers when behavior must stay consistent »
- « conserver les details d'implementation en `.cpp` ou dans des types internes » ->
  l'entete partage reste interne, pas d'expansion d'API publique

## Strategie de resolution detaillee

1. Extraire `TitleReticleCache` + le helper de dessin + la logique d'invalidation dans un
   entete interne partage.
2. Inclure depuis `MfdRenderer.cpp` et `OffscreenPageRenderer.cpp`, supprimer les copies.
3. Verifier le respect du layering (l'entete ne doit dependre que de types deja accessibles
   aux deux modules).

## Strategie de test

- `tests/` rendu : un cas page avec title-reticle rendu via les deux chemins (live +
  offscreen) produisant un resultat identique ; verifier l'invalidation quand le
  nom/titre de page change.

## Documentation impactee

Aucune (type interne ; aucun changement d'API publique ni de rendu observable).
