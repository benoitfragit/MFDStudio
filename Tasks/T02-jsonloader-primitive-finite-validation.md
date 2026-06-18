# T02 — Valider la finitude des rayons avant arithmetique (ring / ellipse)

- **Criticite** : P2 (robustesse aux frontieres d'entree)
- **Vague CI** : 1
- **Statut** : PLAUSIBLE

## Description breve

Dans `ParsePrimitive`, les rayons bruts (`outerRadius`, `bandWidth`, `rx`, `ry`) sont
combines par `std::max`/`std::min`/soustraction/`* 2.0f` **avant** toute validation de
finitude. Une entree enorme produit `Inf` et `std::max(0.0f, NaN)` peut renvoyer `NaN`.

## Fichiers impactes

- `mfd_api/src/io/JsonLoader.cpp` — `ParsePrimitive` (zones ring l. ~1516-1525, ellipse l. ~1543-1548)
- `tests/mfd_api/JsonLoaderTests.cpp` — nouveaux cas

## Contrainte de dev (AGENTS.md)

- « pour chaque frontiere d'entree [...] verifier explicitement les bornes [...] et valeurs non finies »
- « optimiser les allocations, les copies » -> reutiliser le helper de finitude existant
  plutot que d'en introduire un nouveau (cf. T14, `IsFiniteVec2`/predicats centralises)

## Strategie de resolution detaillee

1. Lire chaque rayon via un helper de finitude (idealement celui centralise par T14,
   sinon `ParseFiniteFloatField` local au `.cpp`) **avant** les combinaisons
   `std::max`/`std::min`.
2. En cas de valeur non finie, lever une erreur de validation explicite citant le champ.
3. Conserver l'ordre des operations existant pour les valeurs valides (comportement constant).

## Strategie de test

- `JsonLoaderTests` : ring avec `outerRadius` non fini et ellipse `rx` non fini -> erreur
  de finitude claire.
- Non-regression : primitives ring/ellipse valides inchangees (memes dimensions calculees).

## Documentation impactee

Aucune (validation interne ; aucun changement d'API ni de format JSON documente).
