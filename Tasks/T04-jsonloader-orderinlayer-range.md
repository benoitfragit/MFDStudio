# T04 — Borner `orderInLayer` (int64 + controle de plage)

- **Gravite** : P2 (robustesse / erreur logique silencieuse)
- **Module** : `mfd_api/src/io/JsonLoader.cpp`
- **Fonction** : `ParseDynamicReticleLayerBindings` (l. ~2224-2232)
- **Vague CI** : 1
- **Statut** : PLAUSIBLE

## Scenario minimal

```json
{ "orderInLayer": 99999999999999999 }
```

## Cause exacte

`is_number_integer()` est verifie (l. ~2224), mais `get<int>()` (l. ~2232) reduit
une valeur pouvant depasser `int`, avec **troncature silencieuse** (wrap). La valeur
tronquee alimente ensuite la detection de doublons `bindingOrdersByLayer`.

## Impact

Erreur logique (detection de conflit d'ordre faussee), pas d'insecurite memoire.
Viole la regle AGENTS.md « verifier explicitement les bornes et valeurs des ids/enums
a chaque frontiere ».

## Correction recommandee (minimale)

Lire en `std::int64_t` puis valider la plage avant affectation :
```cpp
const std::int64_t raw = node.get<std::int64_t>();
if (raw < std::numeric_limits<int>::min() || raw > std::numeric_limits<int>::max())
{
    throw std::runtime_error("Field 'orderInLayer' is out of range");
}
const int orderInLayer = static_cast<int>(raw);
```

## Test de non-regression

`JsonLoaderTests` : un binding avec `orderInLayer = 2^40` leve une erreur de plage
au lieu d'une troncature silencieuse ; les ordres valides restent inchanges.
