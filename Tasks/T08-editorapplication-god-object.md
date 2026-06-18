# T08 — Decomposer le god object `EditorApplication`

- **Gravite** : P2 (violation « eviter les classes monolithiques » de AGENTS.md ;
  chantier de fond)
- **Module** : `mfd_editor` (`EditorApplication.{h,cpp}`, `application/*`)
- **Vague CI** : 5 (phase en plusieurs PR internes)
- **Statut** : CONFIRME

## Constat

`EditorApplication` (`include/EditorApplication.h`) declare **~190 methodes privees**
et porte les etats `documentState_`, `previewState_`, `workflowState_`,
`clipboardState_`, `interactionState_`, `layoutState_`, `services_`. Bien qu'il delegue
deja a des services `editor::*`, la classe reste le hub central et melange **9 clusters
de responsabilites** repartis sur `EditorApplication.cpp` (8080 l.),
`EditorApplicationInspectors.cpp` (4202 l.) et `EditorApplicationShell.cpp`.

Clusters identifies (avec evidence de methodes / lignes) :

1. **Document / persistance** (~15) : `LoadWindowConfiguration` (1506), `SaveAll` (1546),
   `WriteRecoverySnapshot` (1569), `RecoverPreviousSession` (1605),
   `CollectWatchedAssetFiles` (1643), `RearmAssetWatcher` (1665). -> `EditorDocumentSession`.
2. **Undo/redo** (~6) : `Undo`/`Redo` (1671/1684), `CaptureCurrentSnapshot` (1697),
   `RestoreSnapshot` (1707), `PushUndoSnapshot` x2 (1728/1733). -> `EditorUndoCoordinator`.
3. **Modele de selection** (~25) : `Select*` (5742-5963), accesseurs `Selected*`/`Active*`
   (5964-6113). -> `EditorSelectionModel`.
4. **Presse-papier** (~9) : `Copy/Cut/PasteSelectedPageReticles` (6164-6346), variantes
   library/primitive. -> completer `EditorPrimitiveClipboardService`.
5. **Preview GPU / texture / police** (~17) : `EnsurePreviewTexture`,
   `RenderLayerPreviewThumbnail`, `Apply/Ensure/ReleasePreviewFont`,
   `MeasurePreviewTextWidthLogical` (2499-2810). -> `EditorPreviewRenderer`.
6. **Hit-testing / manipulation directe** (~15) : `HandlePreviewInteraction` (4503),
   `ApplyMouseTransform` (7214), `Collect/FindNearestPageReticle` (7013/7074),
   `Compute*ScreenBounds`/`*HitDistancePixels` (6441-6806).
   -> `EditorViewportInteractionController`.
7. **Rendu preview / overlays** (~15) : `DrawPagePreview`, `DrawPreviewOverlays`,
   `DrawPagePreviewMinimap`/`Gizmos`, `DrawProblemsPanel` (3026-4105). -> `EditorPreviewView`.
8. **Inspecteurs** (12 grandes fns, tout `EditorApplicationInspectors.cpp`) :
   `DrawWindowInspector` (289), `DrawPageInspector` (577), `DrawPageStrobeInspector` (1072),
   `DrawPageReticleInspector` (1959), `DrawLibraryPrimitiveInspector` (3511)...
   -> classes d'inspecteur par cible.
9. **Popups / workflows de creation** (~25) : `DrawPopups` (7387), `CreateNewPage/Window`,
   `Open*Popup`, `Build*Request`, `Execute*Plan`, `Browse*`. -> `EditorWorkflowController`.

## Impact

Classe ingerable : surface enorme, etats partages, couplage fort entre rendu, etat et
metier. Tout changement risque une regression a distance. Viole frontalement
AGENTS.md (« une classe = une responsabilite principale », « eviter les classes
monolithiques », « limiter la surface publique »).

## Correction recommandee (phasee, comportement constant)

Refactor **incremental**, un service extrait par PR, en commencant par les clusters
les moins couples afin que chaque PR reste verte independamment (et consomme un seul
cycle CI). Ordre suggere :

1. T09 + T10 d'abord (vague 4) : extraire helpers et lambdas -> reduit la surface a
   deplacer.
2. `EditorUndoCoordinator` (cluster 2) puis `EditorSelectionModel` (cluster 3) :
   dependances claires, fort effet de levier.
3. `EditorPreviewRenderer` (5) + `EditorPreviewView` (7).
4. `EditorViewportInteractionController` (6).
5. `EditorWorkflowController` (9) + decoupage des inspecteurs (8).

Chaque extraction : deplacer l'etat + les methodes dans le nouveau type, `EditorApplication`
ne garde que la composition, **aucun** changement de comportement, tests existants verts.

## Test de non-regression

A chaque phase : conserver les tests `tests/mfd_editor` existants verts et ajouter des
tests cibles sur le service extrait (undo, selection, interaction) en isolation de
`EditorApplication`.
