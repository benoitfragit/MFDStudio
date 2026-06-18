# T03 — Garder les `get<bool>()` derriere `is_boolean()`

- **Criticite** : P2 (robustesse / coherence des diagnostics)
- **Vague CI** : 1
- **Statut** : PLAUSIBLE

## Description breve

Plusieurs `node.get<bool>()` ne sont pas precedes du controle `is_boolean()` que le
code voisin applique pourtant deja. Une entree `{"visible": 1}` produit un `type_error`
nlohmann generique au lieu d'un diagnostic specifique au champ.

## Fichiers impactes

- `mfd_api/src/io/JsonLoader.cpp` — sites l. ~874, ~879, ~994, ~1072, ~1932 (et similaires ;
  sites deja gardes a l. ~990, ~1120, ~2177 servent de modele)
- `tests/mfd_api/JsonLoaderTests.cpp` — nouveaux cas

## Contrainte de dev (AGENTS.md)

- « pour chaque frontiere d'entree [...] verifier explicitement les [...] types »
- « centralized command traits or command helpers when behavior must stay consistent »
  -> introduire un helper interne `ReadBooleanField` plutot que dupliquer le motif

## Strategie de resolution detaillee

1. Ajouter un helper interne (anonyme/`static`) au `.cpp` :
   ```cpp
   bool ReadBooleanField(const nlohmann::json& node, std::string_view name)
   {
       if (!node.is_boolean())
       {
           throw std::runtime_error("Field '" + std::string(name) + "' must be a boolean");
       }
       return node.get<bool>();
   }
   ```
2. Remplacer chaque `get<bool>()` non garde par un appel a ce helper (centralise le motif,
   evite la duplication des gardes existants).
3. Diff cantonne aux sites listes.

## Strategie de test

- `JsonLoaderTests` : `"visible": 1` (et un champ booleen non booleen par site corrige)
  -> erreur « must be a boolean » claire.
- Non-regression : `true`/`false` toujours acceptes.

## Documentation impactee

Aucune (le format JSON documente exige deja des booleens ; on ameliore seulement le diagnostic).
