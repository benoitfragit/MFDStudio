# T06 — Eviter la perte de primitives au cycle magnet visuel on/off

- **Criticite** : P2 (incoherence d'etat / perte de donnees potentielle)
- **Vague CI** : 2
- **Statut** : PLAUSIBLE (a confirmer par lecture ciblee du swap synthetique/authored)

## Description breve

Le swap entre primitives synthetiques (forme magnet) et `authoredPrimitives` peut, sur
un reticule sans primitive authored au moment de l'activation, laisser le groupe ne
contenant que la forme magnet apres desactivation. Scenario : magnet visuel ON ->
edition texte/espacement (route vers `authoredPrimitives`) -> magnet visuel OFF.

## Fichiers impactes

- `mfd_api/src/runtime/SceneRegistry.cpp` — `ApplyStrobeMagnetVisualShape` + setters
  d'edition de reticule (l. ~2713-2744)
- `tests/mfd_api/SceneRegistryTests.cpp` — nouveau test

## Contrainte de dev (AGENTS.md)

- « encapsuler les invariants dans les types »
- « corruptions d'etat » = cible prioritaire de la revue
- « toute correction de bug doit avoir un test de non-regression »

## Strategie de resolution detaillee

1. Confirmer le scenario par lecture du swap (l. ~2713-2744) : verifier l'invariant
   « le snapshot authored precede tout remplacement synthetique ».
2. Garder le chemin de restauration OFF lorsque `authoredPrimitives` est vide (ne pas
   ecraser les primitives reelles), ou poser une assertion + correction locale.
3. Correctif strictement local au swap, comportement inchange dans le cas nominal.

## Strategie de test

- `SceneRegistryTests` : cycle magnet visuel on -> edition -> off sur un strobe **sans**
  primitive authored ; verifier la preservation des primitives et la coherence finale.
- Non-regression : cycle on/off sur un reticule avec primitives authored inchange.

## Documentation impactee

Aucune (comportement interne ; corrige une incoherence non documentee).
