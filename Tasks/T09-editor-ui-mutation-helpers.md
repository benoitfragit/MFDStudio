# T09 — Extraire les mutations metier des callbacks ImGui

- **Criticite** : P2 (violation directe « C++ UI mutation rules »)
- **Vague CI** : 4
- **Statut** : CONFIRME

## Description breve

40+ callbacks ImGui inline melent capture undo + mutation multi-champs du domaine +
validation (grep `PushUndoSnapshot`). AGENTS.md impose que le callback ne collecte que
l'intention et delegue la mutation a une fonction nommee.

## Fichiers impactes

- `mfd_editor/src/application/EditorApplicationInspectors.cpp` — cas representatifs :
  l. 1319-1334 (rename strobe), 1344-1369 (remove strobe), 1409-1438 (change template),
  309-409 (`DrawWindowInspector`), 398-408 (toggle UDP)
- `mfd_editor/src/EditorApplication.cpp` — l. 4796, 4838-4840 (`HandlePreviewInteraction`)
- `tests/mfd_editor/` — tests des helpers extraits

## Contrainte de dev (AGENTS.md)

- section « C++ UI mutation rules » : « business mutation must stay in named helpers » ;
  « Avoid inline ImGui callbacks that directly mutate runtime/domain objects, perform
  rollback, or update several related fields » (avec le patron `ApplySelectedReticleNudge`)
- « small ImGui callbacks that delegate business logic to named functions »

## Strategie de resolution detaillee

Par petits lots relisibles, extraire chaque mutation dans une fonction nommee (membre
prive), le callback ne gardant que la collecte d'intention :
- `RenameSelectedPageStrobe(page, strobe, requestedName)`
- `RemovePageStrobe(page, index)` (renvoie un resultat ; le `break` reste dans l'UI)
- `ChangeSelectedStrobeTemplate(page, editedStrobe, templateId)`
- `ApplyWindow<Field>Edit(...)` / `SetWindowCommandUdpEnabled(bool)`
- `BeginReticleDrag(...)` pour l'interaction preview

Chaque lot conserve exactement le comportement (undo inclus).

## Strategie de test

- `tests/mfd_editor` : tests cibles sur chaque helper (rename/remove/change template/
  toggle UDP) verifiant l'effet sur le modele **et** sur la pile undo, sans ImGui.
- Non-regression : comportement editeur inchange.

## Documentation impactee

Aucune (refactor interne) ; mentionner le patron dans la doc archi editeur si elle
decrit la couche UI.
