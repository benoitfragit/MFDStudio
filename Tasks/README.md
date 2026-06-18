# Tasks — Revue de code AGENTS.md

Revue de code conduite selon les regles de `AGENTS.md` (section « Revue de code »).
Chaque fiche est basee sur du code **reellement lu**, cite les lignes, et suit le
format impose : gravite, fichier, fonction, scenario minimal, cause exacte, impact,
correction recommandee, test de non-regression.

## Perimetre inspecte

Frontieres d'entree et runtime :
- `mfd_api/src/io/JsonLoader.cpp`
- `mfd_api/src/control/UdpRuntimeBridge.cpp`, `CommandProcessor.cpp`
- `mfd_api/src/runtime/SceneRegistry.cpp`
- `mfd_api/src/render/Canvas2D.cpp`, `PolygonTriangulation.cpp`
- `mfd_client_api/src/LatestBatchPublisher.cpp`

Architecture / maintenabilite :
- `mfd_editor/src/EditorApplication.cpp` (8080 lignes)
- `mfd_editor/src/application/EditorApplicationInspectors.cpp` (4202 lignes)
- `mfd_editor/src/application/EditorApplicationShell.cpp`
- `mfd_api/src/render/MfdRenderer.cpp`, `mfd_runtime_api/src/internal/OffscreenPageRenderer.cpp`
- generateurs d'identifiants uniques de l'editeur (entetes `internal/application/*`)

Zones **non lues** (a auditer ulterieurement, non couvertes par cette revue) :
`mfd_client_api/src/Animation.cpp`, `mfd_editor/src/EditorDesignExportService.cpp`
(au-dela des boucles de collision), `mfd_window/src/WindowLauncher.cpp` (au-dela des casts),
l'essentiel de `examples/` et de `tests/`.

## Resultat global

- **Aucun finding P0 prouve** (aucun freeze / crash / corruption demontre par lecture).
  Les boucles de rendu (`Canvas2D`, `PolygonTriangulation`), les threads worker
  (`UdpRuntimeBridge`, `LatestBatchPublisher`) et les boucles de drain runtime
  possedent tous un garde-fou d'iterations ou une condition de progression verifiee.
- **1 finding P1** confirme : propagation d'`Inf` depuis le JSON jusqu'a `std::lround` (UB).
- **6 findings P2** de robustesse aux frontieres d'entree (JSON / runtime / threads).
- **5 chantiers de maintenabilite** : god object, regle UI-mutation, lambdas-helpers,
  duplication de logique. Ils n'introduisent pas de bug runtime mais violent
  directement les regles AGENTS.md et degradent la maintenabilite.
- Casts (`reinterpret_cast`), `new`/`delete` : **propres**, tous encapsules a une
  frontiere d'API C externe (WGL/GL/Win32/sockets/protobuf). Rien a corriger.

## Echelle de gravite (AGENTS.md)

| Gravite | Definition |
|---------|------------|
| **P0** | freeze / crash / corruption d'etat |
| **P1** | bug runtime serieux (UB, NaN/Inf, desynchronisation, perte de donnees) |
| **P2** | robustesse / performance / trou de test / dette de maintenabilite |

## Liste des taches

| ID | Titre | Gravite | Module | Vague CI |
|----|-------|---------|--------|----------|
| [T01](T01-jsonloader-non-finite-numeric.md) | Rejeter les tokens numeriques non finis (`std::stod` -> `Inf` -> `lround` UB) | **P1** | mfd_api/io | 1 |
| [T02](T02-jsonloader-primitive-finite-validation.md) | Valider la finitude des rayons avant arithmetique (ring/ellipse) | P2 | mfd_api/io | 1 |
| [T03](T03-jsonloader-boolean-guards.md) | Garder les `get<bool>()` derriere `is_boolean()` | P2 | mfd_api/io | 1 |
| [T04](T04-jsonloader-orderinlayer-range.md) | Borner `orderInLayer` (int64 + range check) | P2 | mfd_api/io | 1 |
| [T05](T05-sceneregistry-capture-magnet-finite.md) | Valider capture/magnet (load + restore snapshot) | P2 | mfd_api/runtime | 2 |
| [T06](T06-sceneregistry-magnet-visual-primitive-loss.md) | Eviter la perte de primitives au cycle magnet visuel on/off | P2 | mfd_api/runtime | 2 |
| [T07](T07-udpbridge-lock-ordering.md) | Supprimer l'imbrication `stateMutex` sous les mutex de file | P2 | mfd_api/control | 2 |
| [T11](T11-centralize-unique-id-generators.md) | Centraliser les generateurs d'identifiants uniques | P2 | mfd_editor | 3 |
| [T12](T12-dedup-titlereticlecache.md) | De-dupliquer `TitleReticleCache` entre 2 renderers | P2 | mfd_api/render | 3 |
| [T09](T09-editor-ui-mutation-helpers.md) | Extraire les mutations metier des callbacks ImGui | P2 | mfd_editor | 4 |
| [T10](T10-editor-hidden-helper-lambdas.md) | Promouvoir les lambdas-helpers en fonctions nommees | P2 | mfd_editor | 4 |
| [T08](T08-editorapplication-god-object.md) | Decomposer le god object `EditorApplication` | P2 | mfd_editor | 5 |
| [T13](T13-ci-quality-gate.md) | Garde-fou CI (clang-tidy + grep AGENTS.md) | P2 | build/CI | 0 |

## Ordre de resolution et reduction des cycles CI (« build go »)

La CI (`.github/workflows/ci.yml`) lance **4 builds Windows** (x64 + win32, debug + release)
a chaque PR. Pour minimiser le nombre de cycles CI, les taches sont regroupees en
**vagues** : chaque vague forme **une seule PR**, qui compile et passe les tests **du premier coup**,
et touche des unites de compilation disjointes pour eviter les rebuilds en cascade.

- **Vague 0 — garde-fou CI (T13).** A faire en premier : ajoute le filet de securite
  qui empeche la reintroduction des anti-patterns et fait echouer tot, localement,
  avant de consommer un cycle CI. Independante du reste.
- **Vague 1 — durcissement JSON (T01-T04).** Concentre toutes les corrections d'entree
  JSON dans `JsonLoader.cpp` + un seul fichier de test. **1 cycle CI** pour 4 findings,
  dont le seul P1. A livrer en priorite.
- **Vague 2 — durcissement runtime (T05-T07).** `SceneRegistry.cpp` + `UdpRuntimeBridge.cpp`.
  Independante de la vague 1.
- **Vague 3 — de-duplication a comportement constant (T11, T12).** Refactors surs,
  sans changement de comportement, faciles a relire. Reduisent la dette avant les
  gros chantiers editeur.
- **Vague 4 — regle UI-mutation et lambdas (T09, T10).** Extractions a comportement
  constant dans l'editeur. A faire avant T08 car elles preparent le terrain.
- **Vague 5 — decomposition du god object (T08).** Le plus gros chantier ; phase en
  plusieurs PR internes (une par service extrait), chacune verte independamment.

Dependances : T09/T10 facilitent T08 (vague 4 avant vague 5). Toutes les autres vagues
sont independantes et peuvent etre menees en parallele par des branches distinctes.

## Definition of Done par tache

Chaque tache n'est terminee que lorsque : correctif minimal et defensif applique,
test de non-regression ajoute, documentation impactee synchronisee, build Win32 Debug
vert localement (`cmake --build --preset debug-win32` puis `ctest --preset test-debug-win32`),
diff limite au perimetre utile.
