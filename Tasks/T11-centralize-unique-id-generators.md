# T11 — Centraliser les generateurs d'identifiants uniques

- **Gravite** : P2 (dette de maintenabilite / duplication ; risque overflow theorique)
- **Module** : `mfd_editor` (entetes internes + `EditorApplication.cpp`)
- **Vague CI** : 3
- **Statut** : CONFIRME (duplication)

## Constat

~9 boucles dupliquees du meme motif « incrementer un suffixe jusqu'a ce que le
candidat ne soit plus en conflit » :

- `include/internal/application/EditorApplicationAuthoringSupport.h:190` (`SuggestPageStrobeDraftName`), `:335` (`MakeUniqueBlinkTypeName`)
- `include/internal/application/EditorApplicationInternal.h:200` (`MakeUniqueLibraryReticleId`), `:300` (`MakeUniquePageReticleInstanceId`)
- `src/EditorApplication.cpp:8013` (`MakeUniqueReticleId`), `:8039` (`MakeUniqueStrobeName`), `:8073` (`MakeUniqueLayerId`)
- `src/EditorPrimitiveClipboardService.cpp:51` (`MakeUniquePrimitivePasteId`)
- `src/EditorDesignExportService.cpp:298` (collision de dossier `_N`), `:2055` (dedup de stem `do/while`)

Variante apparentee (scan de conflit `++order`, a ne pas forcer dans le meme moule) :
`EditorApplicationAuthoringSupport.h:689` (`NextPageDynamicOrderInLayer`).

## Cause / impact

Logique dupliquee a maintenir en N endroits (viole AGENTS.md « ne pas dupliquer les
regles »). De plus le compteur `int suffix` est increment sans borne : depassement
signe = UB **theorique** (necessiterait ~2^31 noms en conflit, donc P2 en pratique).

## Correction recommandee (minimale, comportement constant)

Introduire **un** helper interne, ex. dans un entete `internal` de l'editeur :
```cpp
// Renvoie le premier "base + sep + n" (n a partir de start) tel que !exists(candidate).
template <typename ExistsPredicate>
std::string MakeUniqueCandidate(std::string_view base,
                                std::string_view separator,
                                int start,
                                ExistsPredicate exists);
```
Remplacer les sites par des appels a ce helper. Centraliser donne aussi **un seul**
endroit pour ajouter une borne / garde d'overflow. Conserver strictement le format de
nom produit par chaque site (pas de changement de comportement observable).

## Test de non-regression

`tests/mfd_editor` : pour chaque categorie (reticle id, strobe name, layer id, blink
type, primitive paste id), verifier qu'avec des collisions existantes le helper renvoie
le meme identifiant qu'avant la centralisation (tests de caracterisation).
