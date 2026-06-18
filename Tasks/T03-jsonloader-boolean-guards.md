# T03 — Garder les `get<bool>()` derriere `is_boolean()`

- **Gravite** : P2 (robustesse / coherence des diagnostics)
- **Module** : `mfd_api/src/io/JsonLoader.cpp`
- **Sites** : l. ~874, ~879, ~994, ~1072, ~1932 (et similaires)
- **Vague CI** : 1
- **Statut** : PLAUSIBLE

## Scenario minimal

```json
{ "visible": 1 }      // entier la ou un booleen est attendu
```

## Cause exacte

Plusieurs sites appellent `node.get<bool>()` **sans** le pre-controle `is_boolean()`
que le code voisin applique pourtant deja (ex. l. ~990, ~1120, ~2177). nlohmann
leve alors un `type_error` generique au lieu d'un diagnostic specifique au champ.

## Impact

Pas d'UB ni de corruption (l'exception est attrapee au chargement de page,
l. ~3324/3329). Incoherence de robustesse et message d'erreur peu exploitable
face a une entree malformee. Viole l'exigence AGENTS.md de verifier explicitement
les types a chaque frontiere d'entree.

## Correction recommandee (minimale)

Pour chaque `get<bool>()` liste, ajouter le garde homogene au reste du fichier :
```cpp
if (!node.is_boolean())
{
    throw std::runtime_error("Field '<name>' must be a boolean");
}
```
Centraliser idealement dans un helper interne `ReadBooleanField(node, name)` pour
eviter la duplication du motif (coherent avec la regle « command/rule helpers »).

## Test de non-regression

`JsonLoaderTests` : `"visible": 1` (et un champ booleen non booleen par site)
produit une erreur « must be a boolean » claire ; les valeurs `true`/`false`
restent acceptees.
