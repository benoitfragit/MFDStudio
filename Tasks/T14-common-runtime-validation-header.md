# T14 — Promouvoir la validation/budgets runtime dans `mfd_common_api`

- **Criticite** : P1 (duplication source de divergence de regles entre chemins)
- **Vague CI** : 2 (couche basse ; prealable utile a T02/T05)
- **Statut** : CONFIRME

## Description breve

`RuntimeValidation.h` (constantes de budget + predicats `IsFinite*`/`IsValid*` + validite
temporelle) vit dans `mfd_api`, donc `mfd_common_api` ne peut pas l'inclure : du coup
`CommandTypes.cpp` **re-implemente** tout (constantes, calendrier, finitude). Le chemin
commande et le chemin JSON/scene peuvent ainsi appliquer des limites differentes.

## Fichiers impactes

- `mfd_api/src/runtime/RuntimeValidation.h` -> a deplacer vers
  `mfd_common_api/include/mfd/model/RuntimeBudgets.h` (nouveau)
- `mfd_common_api/src/control/CommandTypes.cpp` (l. 32-45 constantes ; 52-55 `IsFiniteVec2` ;
  57-99 `Validate*` ; 126-190 validite temporelle) -> consommer l'entete, wrappers `throw` fins
- `mfd_api/src/render/Canvas2D.cpp` (l. 62-65 `IsFiniteVec2`) -> include
- `mfd_api/src/runtime/SceneRegistry.cpp` (deja consommateur ; ajuster l'include)
- `mfd_api/CMakeLists.txt` / `mfd_common_api/CMakeLists.txt` si chemin d'include a ajuster
- `tests/mfd_api/RuntimeValidationTests.cpp`, `tests/` command

## Contrainte de dev (AGENTS.md)

- « ne pas dupliquer les regles » ; section « C++ command maintenance rules » :
  « Command behavior must remain consistent across validation, serialization [...] »,
  « Prefer internal command helper functions or command traits when the same rule is
  needed in more than one implementation file »
- « decouper les fonctionnalites par modules coherents » ; respect du layering
  (l'entete ne depend que de `mfd/model/Types.h` + `mfd/model/Reticle.h`, deja dans common_api)

## Strategie de resolution detaillee

1. Deplacer `RuntimeValidation.h` dans `mfd_common_api/include` (ex. `RuntimeBudgets.h`),
   en exposant : les constantes de budget, `IsFiniteVec2`, `IsFiniteAbsWithin`,
   `IsPositiveFiniteWithin`, `IsValidVec2`, `IsValidScale`, `IsLeapYear`, `DaysInMonth`,
   `HasVisibleTimeField`, `IsValidTimeValue`.
2. `SceneRegistry.cpp`, `Canvas2D.cpp` incluent l'entete commun (supprimer copies).
3. `CommandTypes.cpp` : supprimer constantes/finitude/calendrier dupliques ; transformer
   `Validate*`/`ValidateTimeValue` en wrappers fins « predicat + message » appelant les
   predicats centralises. Conserver les constantes specifiques commande
   (`kMaxPatchEntryCount`, `kMaxDynamicReticlesPerBatch`) la ou elles vivent.
4. Aucune valeur de limite ni regle de validite modifiee (comportement constant).

## Strategie de test

- `RuntimeValidationTests` : doivent rester verts depuis le nouvel emplacement.
- Test de coherence : pour un echantillon de valeurs limites (coord/scale/angle/temps),
  verifier que le chemin commande (`Validate*`) et les predicats partages acceptent/
  rejettent **identiquement** (anti-divergence).

## Documentation impactee

`docs/architecture/` (cartographie modules / transport) : indiquer que les budgets et la
validation runtime sont desormais possedes par `mfd_common_api`.
