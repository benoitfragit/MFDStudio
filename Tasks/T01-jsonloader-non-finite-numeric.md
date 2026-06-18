# T01 — Rejeter les tokens numeriques non finis

- **Criticite** : P1 (bug runtime serieux — UB atteignable depuis une entree externe)
- **Vague CI** : 1
- **Statut** : CONFIRME

## Description breve

`std::stod` renvoie `HUGE_VAL` (`+Inf`) sans lever d'exception sur un token JSON enorme
(ex. couleur `"rgb(1e999,0,0)"`). L'`Inf` atteint `std::lround` puis un `static_cast<int>`
hors plage = comportement indefini, declenchable purement par une entree JSON.

## Fichiers impactes

- `mfd_api/src/io/JsonLoader.cpp` — `ParseNumericToken` (l. 660-676), `ParseChannelToken` (l. 678-695)
- `tests/mfd_api/JsonLoaderTests.cpp` — nouveau test
- include : `<cmath>`

## Contrainte de dev (AGENTS.md)

- « pour chaque frontiere d'entree (JSON, UDP, Protobuf, plugins, editeur, API),
  verifier explicitement [...] les valeurs non finies »
- « ne jamais ecarter un risque au motif que l'editeur ne devrait pas produire cette valeur »
- « toute correction de bug doit avoir un test de non-regression »

## Strategie de resolution detaillee

1. Dans `ParseNumericToken`, juste apres `std::stod` (l. 669) et le controle
   `processedCharacters != trimmed.size()`, ajouter :
   ```cpp
   if (!std::isfinite(parsedValue))
   {
       throw std::runtime_error("Numeric token is not finite");
   }
   ```
2. Ne **pas** modifier `ParseChannelToken` : le garde en amont protege ses trois sites
   `std::lround` (l. 685, 691, 694) et tous les autres appelants de `ParseNumericToken`.
3. Verifier l'include `<cmath>` (sinon l'ajouter).
4. Diff limite a ces ~4 lignes.

## Strategie de test

- Ajouter dans `JsonLoaderTests` : couleur `"rgb(1e999,0,0)"` et token isole `"1e999"`
  -> doivent lever (`not finite / invalid token`) et ne jamais atteindre `std::lround`.
- Cas de non-regression positifs : `"rgb(255,0,0)"`, `"rgb(50%,0,0)"`, `"rgb(0.5,0,0)"`
  restent acceptes a l'identique.

## Documentation impactee

Aucune (comportement interne de parsing ; aucune API publique ni flux utilisateur change).
