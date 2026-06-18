# T11 — Centraliser les generateurs d'identifiants uniques

- **Criticite** : P2 (duplication ; risque d'overflow theorique)
- **Vague CI** : 3
- **Statut** : CONFIRME

## Description breve

~9 boucles dupliquees du meme motif « incrementer un suffixe jusqu'a ce que le candidat
ne soit plus en conflit ». Compteur `int` increment sans borne (UB d'overflow signe
theorique). A factoriser dans un helper unique.

## Fichiers impactes

- `mfd_editor/include/internal/application/EditorApplicationAuthoringSupport.h` (l. 190, 335)
- `mfd_editor/include/internal/application/EditorApplicationInternal.h` (l. 200, 300)
- `mfd_editor/src/EditorApplication.cpp` (l. 8013, 8039, 8073)
- `mfd_editor/src/EditorPrimitiveClipboardService.cpp` (l. 51)
- `mfd_editor/src/EditorDesignExportService.cpp` (l. 298, 2055)
- nouveau helper interne (entete `internal` editeur)
- `tests/mfd_editor/` — tests de caracterisation

## Contrainte de dev (AGENTS.md)

- « ne pas dupliquer les regles » ; « centralized command traits or command helpers when
  behavior must stay consistent »
- « eviter les classes monolithiques » indirectement (reduit `EditorApplication.cpp`)

## Strategie de resolution detaillee

1. Ajouter un helper interne :
   ```cpp
   template <typename ExistsPredicate>
   std::string MakeUniqueCandidate(std::string_view base, std::string_view separator,
                                   int start, ExistsPredicate exists);
   ```
   bouclant `for (int n = start; ; ++n)` et renvoyant le premier candidat libre ;
   ajouter une borne/garde d'overflow au seul endroit centralise.
2. Remplacer chaque site, en conservant **strictement** le format de nom produit
   (separateur, casse, point de depart).
3. Laisser la variante `++order` (`NextPageDynamicOrderInLayer`, l. 689) telle quelle
   (motif distinct).

## Strategie de test

- `tests/mfd_editor` : tests de caracterisation par categorie (reticle id, strobe name,
  layer id, blink type, primitive paste id) verifiant que, sous collisions existantes, le
  helper renvoie **le meme** identifiant qu'avant centralisation.

## Documentation impactee

Aucune (helper interne ; identifiants produits inchanges).
