# T09 — Extraire les mutations metier des callbacks ImGui

- **Gravite** : P2 (violation directe « C++ UI mutation rules » de AGENTS.md)
- **Module** : `mfd_editor/src/application/EditorApplicationInspectors.cpp` (et `EditorApplication.cpp`)
- **Vague CI** : 4
- **Statut** : CONFIRME

## Constat

AGENTS.md impose : « UI code may collect user intent, but business mutation must stay
in named helpers. Avoid inline ImGui callbacks that directly mutate runtime/domain
objects, perform rollback, or update several related fields. »

Le grep `PushUndoSnapshot` revele 40+ callbacks inline melant capture undo + mutation
multi-champs + validation dans ce seul fichier. Cas representatifs :

- `EditorApplicationInspectors.cpp:1319-1334` — `ImGui::InputText("Name##strobe_name")` :
  calcule `previousNormalizedName`, normalise/uniquifie, ecrit `strobe.name` +
  `strobe.normalizedName`, et appelle conditionnellement `SetActivePageStrobe`.
  -> extraire `RenameSelectedPageStrobe(page, strobe, requestedName)`.
- `EditorApplicationInspectors.cpp:1344-1369` — bouton « Remove strobe » : efface dans
  `page.strobes`, vide `activeStrobeName`/`normalizedActiveStrobeName`, reselectionne,
  reconstruit le statut. -> extraire `RemovePageStrobe(page, index)`.
- `EditorApplicationInspectors.cpp:1409-1438` — `ImGui::Selectable(templateId)` :
  `PushUndoSnapshot()`, snapshot `previousStrobe`, reaffecte
  `MakePageStrobeFromTemplate(...)`, rafraichit le binding de blink, pose le statut
  (= exactement l'anti-pattern « MutateWithRollback inline » de AGENTS.md).
  -> extraire `ChangeSelectedStrobeTemplate(page, editedStrobe, templateId)`.
- `EditorApplicationInspectors.cpp:309-409` (`DrawWindowInspector`) — motif repete
  `IsItemActivated() -> PushUndoSnapshot()` + ecriture de champ + effet de bord
  (`ApplyPreviewFontFile`). -> helpers `ApplyWindow<Field>Edit(...)`.
- `EditorApplicationInspectors.cpp:398-408` — `Checkbox("Expose command UDP")` :
  construit/reset un `optional` du domaine inline. -> extraire
  `SetWindowCommandUdpEnabled(bool)`.
- `EditorApplication.cpp:4796, 4838-4840` — `HandlePreviewInteraction` : `PushUndoSnapshot()`
  + mutation de `interactionState_` en plein callback. -> `BeginReticleDrag(...)`.

## Impact

Fonctions UI illisibles melant rendu / undo / validation / mutation ; difficiles a
tester et a faire evoluer sans regression. Forte source de bugs d'etat.

## Correction recommandee (incrementale, comportement constant)

Pour chaque callback, deplacer la mutation metier dans une fonction nommee
(membre prive ou helper interne), le callback ne gardant que la collecte d'intention,
selon le patron AGENTS.md :
```cpp
if (ImGui::Button("Nudge right"))
{
    ApplySelectedReticleNudge(liveScene, displayScene, Vec2 {kStep, 0.0f});
}
```
Avancer par petits lots relisibles ; chaque lot doit conserver exactement le
comportement (undo inclus).

## Test de non-regression

`tests/mfd_editor` : tests cibles sur les nouveaux helpers (rename strobe, remove
strobe, change template, toggle UDP) verifiant l'effet sur le modele **et** la pile
undo, independamment de ImGui.
