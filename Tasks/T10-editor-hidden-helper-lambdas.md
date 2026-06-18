# T10 — Promouvoir les lambdas-helpers en fonctions nommees

- **Criticite** : P2 (violation « lambdas used as hidden helper functions »)
- **Vague CI** : 4
- **Statut** : CONFIRME

## Description breve

Plusieurs lambdas locales de `EditorApplication.cpp` servent de helpers metier/rendu
(rendu de menus, mutation de selection, init d'interaction, reset d'etat) au lieu de
simples predicats STL. AGENTS.md reserve les lambdas aux predicats STL directs.

## Fichiers impactes

- `mfd_editor/src/EditorApplication.cpp` — `drawClipItemsForTarget` (l. 5025-5086),
  `collectClipTargetsForReticle`/`drawReticleContextContent` (l. 5145-5211),
  `initializeInteraction` (l. 4767-4798), `cancelPreviewInteraction`/
  `cancelLibraryPreviewInteraction` (l. 4513-4519, 5260), `toScreenPoint`/`drawHandle`
  et helpers geometrie (l. 3495-3500, 5508/5522/6543/6550/6573)
- `tests/mfd_editor/` — tests des helpers extraits

## Contrainte de dev (AGENTS.md)

- « ne pas introduire de lambda locale pour des helpers UI ou metier si elle n'est pas
  utilisee directement par un algorithme (`find_if`, `sort`, `visit`...) ; preferer une
  fonction nommee, locale au `.cpp` si possible »
- « lambdas used as hidden helper functions » -> interdit

## Strategie de resolution detaillee

Promouvoir chaque lambda-helper en :
- **membre prive** quand l'etat `this` est requis (`DrawReticleClipMenuItems`,
  `BeginReticleHandleInteraction`, `ResetPreviewInteractionState`) ;
- **fonction libre locale au `.cpp`** sinon (`toScreenPoint`, `drawHandle`, geometrie).
Aucun changement de comportement. (Aucune capture large `[&]`/`[=]` ni struct locale dans
ce fichier : ces points sont deja propres.)

## Strategie de test

- `tests/mfd_editor` : couverture cible sur clipping de reticule et reset d'interaction ;
  comportement de selection/interaction inchange.

## Documentation impactee

Aucune (refactor interne).
