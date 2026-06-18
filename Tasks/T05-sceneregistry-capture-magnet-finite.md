# T05 — Valider capture / magnet (chargement + restauration de snapshot)

- **Gravite** : P2 (robustesse / propagation NaN)
- **Module** : `mfd_api/src/runtime/SceneRegistry.cpp`
- **Fonctions** : `IsInsideCaptureArea` (l. ~335-339), `RestoreRuntimeSnapshot` (l. ~1890-1896)
- **Vague CI** : 2
- **Statut** : PLAUSIBLE

## Scenario minimal

Un strobe charge depuis un document (ou restaure depuis un snapshot) dont
`capture.radius` / `capture.size` ou `magnet.radius` est `NaN` ou negatif.
Contrairement aux setters live, le chargement de document et la restauration de
snapshot **ne revalident pas** ces flottants.

## Cause exacte

Les comparaisons `radius * radius` et `size.x * 0.5f` avec `<=` renvoient toujours
`false` sur `NaN` (toute comparaison avec NaN est fausse). Un rayon de magnet non
fini se propage ensuite dans `FindNearestStrobeMagnetTarget` / `StrobeMagnetSummary`.

## Impact

Comportement silencieux et faux (aucune capture la ou elle devrait avoir lieu),
propagation de NaN dans la logique magnet. Pas de crash demontre. Viole l'exigence
AGENTS.md de revalider les entrees a **toutes** les frontieres, y compris load/restore.

## Correction recommandee (minimale)

Appliquer aux chemins « load document » et `RestoreRuntimeSnapshot` la **meme**
validation/clamp que les setters live (rayon/taille finis et >= 0). Factoriser dans
un helper interne `SanitizeCaptureConfig` / `SanitizeMagnetConfig` reutilise par les
setters et par load/restore, pour eviter la divergence de regles (regle AGENTS.md
« command/rule helpers »).

## Test de non-regression

`SceneRegistryTests` : restaurer un snapshot avec rayon magnet / capture = NaN
donne un resultat deterministe et sur (pas de capture, pas de NaN qui s'echappe).
