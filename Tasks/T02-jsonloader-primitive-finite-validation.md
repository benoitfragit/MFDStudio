# T02 — Valider la finitude des rayons avant arithmetique (ring / ellipse)

- **Gravite** : P2 (robustesse aux frontieres d'entree)
- **Module** : `mfd_api/src/io/JsonLoader.cpp`
- **Fonction** : `ParsePrimitive` (zones ring l. ~1516-1525, ellipse l. ~1543-1548)
- **Vague CI** : 1
- **Statut** : PLAUSIBLE

## Scenario minimal

```json
{ "type": "ring", "outerRadius": 1e30, "bandWidth": -1e30 }
```
ou une ellipse avec `rx`/`ry` non finis.

## Cause exacte

Les valeurs brutes `node.value()` (rayons, bande, rx/ry) sont combinees par
`std::max` / `std::min` / soustraction / `* 2.0f` **avant** toute validation de
finitude. Une entree enorme produit `Inf`, et `std::max(0.0f, NaN)` peut renvoyer
`NaN` (selon l'ordre des arguments). On fait donc de l'arithmetique fragile sur des
flottants non fiables.

## Impact

Limite : les controles aval `ValidateFiniteAbs` / `ValidateSegmentCount`
(l. ~263-277) rattrapent la valeur et levent une exception. Le risque est l'usage
d'arithmetique intermediaire sur des flottants non valides (NaN/Inf transitoires)
et un diagnostic moins clair pour l'utilisateur. Aucun UB demontre.

## Correction recommandee (minimale)

Faire passer `outerRadius`, `bandWidth`, `rx`, `ry` par une lecture validant la
finitude (helper local `ParseFiniteFloatField`) **avant** les combinaisons
`std::max`/`std::min`. Reutiliser le helper de finitude deja present plutot que
d'en introduire un nouveau.

## Test de non-regression

`JsonLoaderTests` : un ring avec `outerRadius` non fini (et une ellipse `rx` non fini)
levent une erreur de validation de finitude explicite ; les primitives valides restent
inchangees.
