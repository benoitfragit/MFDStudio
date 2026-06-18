# T01 — Rejeter les tokens numeriques non finis

- **Gravite** : P1 (bug runtime serieux — UB atteignable depuis une entree externe)
- **Module** : `mfd_api/src/io/JsonLoader.cpp`
- **Fonctions** : `ParseNumericToken` (l. 660-676), `ParseChannelToken` (l. 678-695)
- **Vague CI** : 1
- **Statut** : CONFIRME

## Scenario minimal

Une configuration de fenetre / page JSON contient une couleur du type
`"color": "rgb(1e999, 0, 0)"` (ou tout token numerique a magnitude enorme).

## Cause exacte

`ParseNumericToken` (l. 669) appelle `std::stod`. Pour `1e999`, `std::stod`
**ne leve pas** d'exception : il renvoie `HUGE_VAL` (= `+Inf`) et positionne
`processedCharacters == trimmed.size()`. Le garde-fou existant
`if (processedCharacters != trimmed.size())` (l. 670) **ne detecte donc pas**
le depassement. `Inf` est retourne tel quel.

`ParseChannelToken` consomme ensuite cette valeur :
```cpp
return ClampByte(static_cast<int>(std::lround(numericValue)));        // l. 694
// et l. 685 / 691 pour les variantes pourcentage / [0,1]
```
`std::lround(Inf)` a un resultat **non specifie** et peut lever `FE_INVALID`,
puis le `static_cast<int>` d'une valeur hors plage est un comportement indefini.
`ClampByte` intervient trop tard : il borne le `int` deja issu d'une conversion UB.

## Impact

Comportement indefini (resultat imprevisible, exception flottante possible)
declenchable **purement par une entree JSON** non fiable — viole la regle AGENTS.md
« pour chaque frontiere d'entree, verifier les valeurs non finies » et
« ne jamais ecarter un risque au motif que l'editeur ne devrait pas produire cette valeur ».

## Correction recommandee (minimale, defensive)

Dans `ParseNumericToken`, apres `std::stod`, rejeter toute valeur non finie avant
de la retourner :
```cpp
if (!std::isfinite(parsedValue))
{
    throw std::runtime_error("Numeric token is not finite");
}
```
Cela protege d'un seul coup les trois sites `std::lround` de `ParseChannelToken`
ainsi que tous les autres appelants de `ParseNumericToken`. Inclure `<cmath>`.

## Test de non-regression

- `JsonLoaderTests` : une couleur `"rgb(1e999,0,0)"` (et un token isole `"1e999"`)
  doit lever une erreur « not finite / invalid token » et **ne jamais** atteindre
  `std::lround`.
- Verifier qu'une couleur valide (`"rgb(255,0,0)"`, `"rgb(50%,0,0)"`, `"rgb(0.5,0,0)"`)
  reste acceptee a l'identique.
