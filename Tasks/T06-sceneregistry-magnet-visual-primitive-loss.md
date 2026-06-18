# T06 — Eviter la perte de primitives au cycle magnet visuel on/off

- **Gravite** : P2 (incoherence d'etat / perte de donnees potentielle)
- **Module** : `mfd_api/src/runtime/SceneRegistry.cpp`
- **Fonction** : `ApplyStrobeMagnetVisualShape` + setters d'edition de reticule (l. ~2713-2744)
- **Vague CI** : 2
- **Statut** : PLAUSIBLE (a confirmer par lecture ciblee du swap synthetique/authored)

## Scenario minimal

1. Activer le magnet visuel (ON) sur un reticule **sans** primitive authored.
2. Editer texte / espacement (route vers `authoredPrimitives`).
3. Desactiver le magnet visuel (OFF).

## Cause exacte

Le swap entre primitives synthetiques (forme magnet) et `authoredPrimitives` peut,
lorsque le snapshot authored etait vide a l'activation, laisser le groupe ne
contenant que la forme magnet apres l'OFF.

## Impact

Perte possible des primitives authored du reticule sur un cycle magnet on/off
(incoherence d'etat, pas de crash).

## Correction recommandee (minimale)

Garder le chemin de restauration OFF lorsque `authoredPrimitives` est vide
(ne pas ecraser), ou poser une assertion garantissant que le snapshot authored
precede tout remplacement synthetique. Correctif strictement local au swap.

## Test de non-regression

`SceneRegistryTests` : cycle magnet visuel on -> edition -> off sur un strobe sans
primitive authored ; verifier que les primitives sont preservees et que l'etat final
est coherent.
