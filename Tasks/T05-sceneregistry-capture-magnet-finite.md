# T05 — Valider capture / magnet (chargement + restauration de snapshot)

- **Criticite** : P2 (robustesse / propagation NaN)
- **Vague CI** : 2
- **Statut** : PLAUSIBLE

## Description breve

Le chargement de document et `RestoreRuntimeSnapshot` ne revalident pas
`capture.radius`/`capture.size`/`magnet.radius` (contrairement aux setters live). Un
rayon `NaN`/negatif rend toute comparaison `<=` fausse (aucune capture silencieusement)
et propage du `NaN` dans la logique magnet.

## Fichiers impactes

- `mfd_api/src/runtime/SceneRegistry.cpp` — `IsInsideCaptureArea` (l. ~335-339),
  `RestoreRuntimeSnapshot` (l. ~1890-1896), chemins de load document
- `tests/mfd_api/SceneRegistryTests.cpp` — nouveau test

## Contrainte de dev (AGENTS.md)

- « verifier explicitement [...] les valeurs non finies » a **toutes** les frontieres
- « ne jamais ecarter un risque au motif que l'editeur ne devrait pas produire cette valeur »
- « centralized [...] helpers when behavior must stay consistent » (memes regles que les setters live)
- s'appuie sur les predicats centralises (cf. T14)

## Strategie de resolution detaillee

1. Factoriser la regle des setters live dans des helpers internes
   `SanitizeCaptureConfig` / `SanitizeMagnetConfig` (rayon/taille finis et >= 0,
   sinon clamp/desactivation deterministe).
2. Appeler ces helpers sur les chemins « load document » **et** dans `RestoreRuntimeSnapshot`.
3. Reutiliser `runtime_validation::IsFinite*` (centralise par T14) pour les tests de finitude.

## Strategie de test

- `SceneRegistryTests` : restaurer un snapshot avec rayon magnet/capture = `NaN` ->
  resultat deterministe (pas de capture), aucun `NaN` qui s'echappe vers
  `FindNearestStrobeMagnetTarget`/`StrobeMagnetSummary`.
- Non-regression : capture/magnet valides inchanges.

## Documentation impactee

`docs/CONCEPTS.md` ou doc runtime si le comportement de capture/magnet sur entree
invalide y est decrit (preciser le clamp/desactivation). Sinon aucune.
