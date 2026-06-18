# Tasks — Revue de code AGENTS.md

Revue de code conduite selon les regles de `AGENTS.md` (section « Revue de code »).
Chaque fiche est basee sur du code **reellement lu** et cite les lignes.

## Gabarit de fiche (une fiche par tache)

Chaque fichier `T??-*.md` suit le meme gabarit, pense pour une correction facile :

1. **Criticite** (P0/P1/P2) + vague CI + statut
2. **Description breve**
3. **Fichiers impactes**
4. **Contrainte de dev (AGENTS.md)** — la/les regles concernees.
   > Exception explicite pour le god object (T08) : AGENTS.md autorise a **ne pas** tout
   > decouper — « favors controlled, local improvements over broad rewrites » et interdit
   > les « broad rewrites that do not reduce concrete maintenance risk ». On n'extrait que
   > les clusters ou cela reduit un risque concret ; un noyau d'orchestration peut rester gros.
5. **Strategie de resolution detaillee**
6. **Strategie de test**
7. **Documentation impactee**

## Perimetre inspecte

- **Frontieres d'entree / runtime** : `JsonLoader.cpp`, `UdpRuntimeBridge.cpp`,
  `CommandProcessor.cpp`, `SceneRegistry.cpp`, `Canvas2D.cpp`, `PolygonTriangulation.cpp`,
  `LatestBatchPublisher.cpp`
- **Architecture / maintenabilite** : `EditorApplication.cpp` (8080 l.),
  `EditorApplicationInspectors.cpp` (4202 l.), `EditorApplicationShell.cpp`, renderers
- **Duplication couches basses** : `mfd_common_api/` (`CommandTypes.cpp`, `PageName.cpp`,
  `ipc/UdpChannel.cpp`), `mfd_api/` bas niveau (`runtime/RuntimeValidation.h`, `io/json/*`,
  `render/*`), `mfd_runtime_api/`
- **Generateurs d'id uniques** de l'editeur

Zones **non lues** (a auditer ulterieurement) : `mfd_client_api/src/Animation.cpp`,
gros de `EditorDesignExportService.cpp`, `WindowLauncher.cpp` (au-dela des casts),
l'essentiel de `examples/` et `tests/`.

## Resultat global

- **Aucun finding P0 prouve** : boucles de rendu, threads worker et drains runtime ont
  tous un garde-fou d'iterations ou une progression verifiee.
- **2 findings P1** : T01 (`Inf` JSON -> `std::lround`, UB) et T14 (regles de budget/
  validation dupliquees entre couches -> divergence possible commande vs JSON/scene).
- **Robustesse P2** : T02-T07 (frontieres JSON/runtime/threads).
- **Maintenabilite P2** : T08-T12 (god object cible, regle UI-mutation, lambdas, id uniques,
  `TitleReticleCache`).
- **Duplication couches basses P2** : T15 (`Trim`/`Lowercase`), T16 (`SanitizeSegmentCount`).
- Casts (`reinterpret_cast`), `new`/`delete` : **propres**, encapsules aux frontieres C
  externes (WGL/GL/Win32/sockets/protobuf). Rien a corriger.

## Echelle de criticite (AGENTS.md)

| Criticite | Definition |
|-----------|------------|
| **P0** | freeze / crash / corruption d'etat |
| **P1** | bug runtime serieux (UB, NaN/Inf, desynchronisation, divergence de regles, perte de donnees) |
| **P2** | robustesse / performance / trou de test / dette de maintenabilite |

## Liste des taches

| ID | Criticite | Titre | Module | Vague CI |
|----|-----------|-------|--------|----------|
| [T01](T01-jsonloader-non-finite-numeric.md) | **P1** | Rejeter les tokens numeriques non finis (`stod`->`Inf`->`lround` UB) | mfd_api/io | 1 |
| [T02](T02-jsonloader-primitive-finite-validation.md) | P2 | Valider la finitude des rayons (ring/ellipse) | mfd_api/io | 1 |
| [T03](T03-jsonloader-boolean-guards.md) | P2 | Garder les `get<bool>()` derriere `is_boolean()` | mfd_api/io | 1 |
| [T04](T04-jsonloader-orderinlayer-range.md) | P2 | Borner `orderInLayer` (int64 + range) | mfd_api/io | 1 |
| [T14](T14-common-runtime-validation-header.md) | **P1** | Promouvoir validation/budgets runtime dans common_api | mfd_common_api | 2 |
| [T05](T05-sceneregistry-capture-magnet-finite.md) | P2 | Valider capture/magnet (load + restore) | mfd_api/runtime | 2 |
| [T06](T06-sceneregistry-magnet-visual-primitive-loss.md) | P2 | Perte de primitives au cycle magnet on/off | mfd_api/runtime | 2 |
| [T07](T07-udpbridge-lock-ordering.md) | P2 | Ordre de verrouillage `stateMutex` | mfd_api/control | 2 |
| [T11](T11-centralize-unique-id-generators.md) | P2 | Centraliser les generateurs d'id uniques | mfd_editor | 3 |
| [T12](T12-dedup-titlereticlecache.md) | P2 | De-dupliquer `TitleReticleCache` (2 renderers) | mfd_api/render | 3 |
| [T15](T15-common-string-utils.md) | P2 | Centraliser `Trim`/`Lowercase` (couches basses) | mfd_common_api | 3 |
| [T16](T16-dedup-sanitize-segment-count.md) | P2 | De-dupliquer `SanitizeSegmentCount` | mfd_api/render | 3 |
| [T09](T09-editor-ui-mutation-helpers.md) | P2 | Extraire les mutations metier des callbacks ImGui | mfd_editor | 4 |
| [T10](T10-editor-hidden-helper-lambdas.md) | P2 | Promouvoir les lambdas-helpers en fonctions nommees | mfd_editor | 4 |
| [T08](T08-editorapplication-god-object.md) | P2 | Decomposer (cible) le god object `EditorApplication` | mfd_editor | 5 |
| [T13](T13-ci-quality-gate.md) | P2 | Garde-fou CI (clang-tidy + grep AGENTS.md) | build/CI | 0 |

## Ordre de resolution et reduction des cycles CI (« build go »)

La CI (`.github/workflows/ci.yml`) lance **4 builds Windows** (x64 + win32, debug + release)
par PR. Pour minimiser les cycles, les taches sont regroupees en **vagues** : chaque vague
= **une PR**, verte du premier coup, sur des unites de compilation disjointes.

- **Vague 0 — garde-fou CI (T13)** : en premier ; echoue tot, localement, avant de
  consommer un cycle. Independant.
- **Vague 1 — durcissement JSON (T01-T04)** : tout dans `JsonLoader.cpp` + un fichier de
  test. **1 cycle CI pour 4 findings**, dont un P1. Priorite.
- **Vague 2 — runtime + socle commun (T14, T05-T07)** : T14 d'abord (deplace le socle de
  validation vers `mfd_common_api`), puis T05/T06/T07 qui le reutilisent.
- **Vague 3 — de-duplication a comportement constant (T11, T12, T15, T16)** : refactors
  surs, faciles a relire.
- **Vague 4 — regle UI-mutation et lambdas (T09, T10)** : extractions a comportement
  constant ; preparent T08.
- **Vague 5 — decomposition cible du god object (T08)** : phase en plusieurs PR internes,
  une par service extrait, chacune verte independamment.

Dependances : T14 avant T02/T05 (predicats partages) ; T09/T10 avant T08. Les autres vagues
sont independantes et parallelisables sur des branches distinctes.

## Definition of Done par tache

Correctif minimal et defensif applique, test de non-regression ajoute, documentation
impactee synchronisee, build Win32 Debug vert localement
(`cmake --build --preset debug-win32` puis `ctest --preset test-debug-win32`),
diff limite au perimetre utile.
