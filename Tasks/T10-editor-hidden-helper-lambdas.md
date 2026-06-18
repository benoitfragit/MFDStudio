# T10 — Promouvoir les lambdas-helpers en fonctions nommees

- **Gravite** : P2 (violation « lambdas used as hidden helper functions » de AGENTS.md)
- **Module** : `mfd_editor/src/EditorApplication.cpp`
- **Vague CI** : 4
- **Statut** : CONFIRME

## Constat

AGENTS.md interdit « lambdas used as hidden helper functions » et reserve les lambdas
aux predicats STL directs (`find_if`, `sort`, `visit`...). Sites a corriger :

- `EditorApplication.cpp:5025-5086` — `drawClipItemsForTarget` : rend des items de menu
  ET appelle `ApplyPageReticleClipping` + pilote la completion du tutoriel.
  -> membre prive `DrawReticleClipMenuItems(page, reticleIndex, primitiveIndex)`.
- `EditorApplication.cpp:5145-5156` / `5158-5211` — `collectClipTargetsForReticle` et
  `drawReticleContextContent` (qui capture deux autres lambdas, rend, et mute la
  selection via `SelectPageReticle` / `TogglePageReticleSelection`).
  -> deux membres prives.
- `EditorApplication.cpp:4767-4798` — `initializeInteraction` : 30 lignes mutant
  `interactionState_` + `PushUndoSnapshot()`. -> `BeginReticleHandleInteraction(...)`.
- `EditorApplication.cpp:4513-4519` / `5260` — `cancelPreviewInteraction` /
  `cancelLibraryPreviewInteraction` : reinitialisent 4 champs.
  -> `ResetPreviewInteractionState()`.
- `EditorApplication.cpp:3495-3500` et `5508/5522/6573/6543/6550` — `toScreenPoint`,
  `drawHandle` et helpers de geometrie/rendu multi-lignes reutilises (l'exception
  « petit predicat STL » ne s'applique pas). -> fonctions libres locales au `.cpp`
  ou membres.

Aucune capture large `[&]`/`[=]` ni struct locale dans une fonction trouvee dans ce
fichier : ces anti-patterns sont propres ici.

## Impact

Helpers metier caches dans des lambdas locales : difficiles a tester, a nommer dans
les traces, et a relire. Aggrave le god object (cf. T08).

## Correction recommandee (incrementale, comportement constant)

Promouvoir chaque lambda-helper en fonction nommee (membre prive ou fonction libre
locale au `.cpp` quand l'etat `this` n'est pas requis). Aucun changement de
comportement.

## Test de non-regression

Couverture ciblee sur les nouveaux helpers extraits (clipping de reticule, reset
d'interaction) ; verification que le comportement de selection/interaction est
inchange.
