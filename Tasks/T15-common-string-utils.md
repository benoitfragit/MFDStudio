# T15 — Centraliser `TrimAsciiWhitespace` et `AsciiLowercase`

- **Criticite** : P2 (duplication inter-couches de helpers de chaines)
- **Vague CI** : 3
- **Statut** : CONFIRME

## Description breve

`TrimAsciiWhitespace` est byte-identique entre `mfd_api` et `mfd_common_api` ; la boucle
de mise en minuscule / normalisation est repetee dans les memes couches. A regrouper dans
un petit entete utilitaire de `mfd_common_api`.

## Fichiers impactes

- `mfd_common_api/include/mfd/core/StringUtils.h` (nouveau) + `.cpp` si besoin
- `mfd_api/src/io/json/JsonValueHelpers.cpp` (l. 20-37 trim ; 39-50 `Lowercase` ;
  59-67 `CanonicalToken`)
- `mfd_common_api/src/model/PageName.cpp` (l. 19-36 trim ; 46-49 normalisation)
- `tests/` couvrant trim/normalisation
- (couche haute, hors perimetre mais a aligner ensuite : `EditorPageRenameService.cpp:40,59`,
  `EditorAssetPathService.cpp:22-29`)

## Contrainte de dev (AGENTS.md)

- « ne pas dupliquer les regles » ; « centralized [...] helpers when behavior must stay consistent »
- « limiter la surface publique au strict necessaire » -> exposer seulement
  `TrimAsciiWhitespace` et `AsciiLowercase`
- « optimiser les allocations » -> garder `std::string_view` en entree, `noexcept` pour le trim

## Strategie de resolution detaillee

1. Ajouter dans `mfd_common_api` : `std::string_view mfd::TrimAsciiWhitespace(std::string_view) noexcept`
   et `std::string mfd::AsciiLowercase(std::string_view)`.
2. `PageName.cpp` : `NormalizePageName(v)` devient `AsciiLowercase(TrimAsciiWhitespace(v))`.
3. `JsonValueHelpers.cpp` : `Lowercase` devient un alias de `AsciiLowercase` ; le trim local
   est supprime au profit du helper commun ; `CanonicalToken` reutilise `AsciiLowercase`.
4. Comportement identique (memes regles ASCII), aucune dependance nouvelle hors STL.

## Strategie de test

- Tests unitaires du helper : trim des deux cotes, chaines vides/espaces seuls, casse
  ASCII, non-ASCII inchanges.
- Non-regression : `NormalizePageName` et la canonicalisation JSON produisent les memes
  resultats qu'avant.

## Documentation impactee

Aucune (helpers internes a la lib ; comportement inchange).
