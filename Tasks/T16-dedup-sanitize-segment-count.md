# T16 — De-dupliquer `SanitizeSegmentCount` (couche rendu)

- **Criticite** : P2 (petite duplication exacte)
- **Vague CI** : 3
- **Statut** : CONFIRME

## Description breve

`SanitizeSegmentCount` (`std::clamp(requested, minimum, kMaxPrimitiveSegments)`) est
byte-identique dans deux unites de la couche rendu.

## Fichiers impactes

- `mfd_api/src/render/BezierPolylineCache.cpp` (l. 28-31)
- `mfd_api/src/render/Canvas2D.cpp` (l. 67-70)
- entete interne rendu partage (ou l'entete budgets de T14, a cote de `kMaxPrimitiveSegments`)

## Contrainte de dev (AGENTS.md)

- « ne pas dupliquer les regles » ; « centralized [...] helpers when behavior must stay consistent »
- « conserver les details d'implementation [...] dans des types internes »

## Strategie de resolution detaillee

1. Definir un seul `SanitizeSegmentCount` inline dans un entete interne de la couche rendu
   (ou aupres de `kMaxPrimitiveSegments` si T14 a centralise la constante).
2. Inclure depuis `BezierPolylineCache.cpp` et `Canvas2D.cpp`, supprimer les copies.
3. Depend de T14 si la constante est deplacee (sinon autonome).

## Strategie de test

- Test unitaire : clamp aux bornes (sous le minimum, au-dessus de `kMaxPrimitiveSegments`,
  valeur nominale).
- Non-regression : segmentation des primitives bezier/Canvas inchangee.

## Documentation impactee

Aucune.
