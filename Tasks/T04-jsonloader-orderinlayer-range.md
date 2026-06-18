# T04 — Borner `orderInLayer` (int64 + controle de plage)

- **Criticite** : P2 (robustesse / erreur logique silencieuse)
- **Vague CI** : 1
- **Statut** : PLAUSIBLE

## Description breve

`ParseDynamicReticleLayerBindings` verifie `is_number_integer()` mais lit avec
`get<int>()`, ce qui tronque silencieusement une valeur depassant `int`
(ex. `"orderInLayer": 99999999999999999`). La valeur tronquee fausse la detection de
doublons `bindingOrdersByLayer`.

## Fichiers impactes

- `mfd_api/src/io/JsonLoader.cpp` — `ParseDynamicReticleLayerBindings` (l. ~2224-2232)
- `tests/mfd_api/JsonLoaderTests.cpp` — nouveau cas
- include : `<limits>`

## Contrainte de dev (AGENTS.md)

- « pour chaque frontiere d'entree [...] verifier explicitement les bornes [...] ids, enums »
- « encapsuler les invariants dans les types »

## Strategie de resolution detaillee

1. Lire en `std::int64_t` puis controler la plage avant affectation :
   ```cpp
   const std::int64_t raw = node.get<std::int64_t>();
   if (raw < std::numeric_limits<int>::min() || raw > std::numeric_limits<int>::max())
   {
       throw std::runtime_error("Field 'orderInLayer' is out of range");
   }
   const int orderInLayer = static_cast<int>(raw);
   ```
2. Verifier l'include `<limits>`.

## Strategie de test

- `JsonLoaderTests` : binding avec `orderInLayer = 2^40` -> erreur de plage (pas de troncature).
- Non-regression : ordres valides (negatifs/positifs dans `int`) inchanges, detection de
  doublon correcte.

## Documentation impactee

Aucune.
