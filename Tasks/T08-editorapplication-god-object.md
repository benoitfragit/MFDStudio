# T08 — Decomposer (de maniere ciblee) le god object `EditorApplication`

- **Criticite** : P2 (maintenabilite ; chantier de fond, phase)
- **Vague CI** : 5 (plusieurs PR internes)
- **Statut** : CONFIRME

## Description breve

`EditorApplication` declare ~190 methodes privees et porte 7 blocs d'etat ; il melange
9 clusters de responsabilites sur 3 gros `.cpp` (8080 + 4202 + 1004 lignes). Decoupage
**incremental et cible** en services cohesifs, sans reecriture massive.

## Fichiers impactes

- `mfd_editor/include/EditorApplication.h`
- `mfd_editor/src/EditorApplication.cpp` (8080 l.)
- `mfd_editor/src/application/EditorApplicationInspectors.cpp` (4202 l.)
- `mfd_editor/src/application/EditorApplicationShell.cpp`
- nouveaux entetes/sources de services extraits (ex. `EditorUndoCoordinator`,
  `EditorSelectionModel`, `EditorPreviewRenderer`, `EditorViewportInteractionController`,
  `EditorWorkflowController`)
- `tests/mfd_editor/` — tests des services extraits

## Contrainte de dev (AGENTS.md) — avec la nuance god object

- « une classe = une responsabilite principale » ; « eviter les classes monolithiques » ;
  « limiter la surface publique »
- **MAIS** regle explicite qui permet de s'affranchir d'un decoupage total :
  « This project favors controlled, local improvements over broad rewrites » et
  « broad rewrites that do not reduce concrete maintenance risk » sont **interdites**.
  => On ne casse PAS `EditorApplication` pour le principe : on extrait **uniquement** les
  clusters ou l'extraction reduit un risque de maintenance concret, par PR independantes.
  Un noyau de composition/orchestration peut legitimement rester volumineux.

## Strategie de resolution detaillee

Faire T09 et T10 d'abord (reduisent la surface a deplacer). Puis, **un service par PR**,
du moins couple au plus couple, chacune verte independamment (1 cycle CI / PR) :

1. `EditorUndoCoordinator` (Undo/Redo/Capture/Restore/PushUndoSnapshot, l. ~1671-1733).
2. `EditorSelectionModel` (Select*/accesseurs Selected*/Active*, l. ~5742-6113).
3. `EditorPreviewRenderer` (textures/police/thumbnails, l. ~2499-2810)
   + `EditorPreviewView` (DrawPagePreview/Overlays/Minimap/Gizmos, l. ~3026-4105).
4. `EditorViewportInteractionController` (hit-testing/ApplyMouseTransform, l. ~4503-7214).
5. `EditorWorkflowController` (popups/creation, l. ~7387+) + inspecteurs par cible.

Chaque extraction : deplacer etat + methodes dans le nouveau type, `EditorApplication`
ne garde que la composition. Aucun changement de comportement.

## Strategie de test

- Conserver verts les tests `tests/mfd_editor` existants a chaque phase.
- Ajouter des tests cibles par service extrait (undo, selection, interaction) en
  isolation de `EditorApplication`.

## Documentation impactee

`docs/architecture/editor_workspace_layout.md` (ou doc archi editeur) : refleter la
nouvelle decomposition en services au fur et a mesure des PR.
