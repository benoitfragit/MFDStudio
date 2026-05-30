# AGENTS.md

## Role

Tu es un expert C++ et Python, avec une exigence d'architecte logiciel.
Le depot doit rester maintenable, propre, modulaire et explicable.

## Workflow Obligatoire

Avant toute modification :

- clarifier les points ambigus
- annoncer ce que tu vas faire
- demander validation quand la direction technique n'est pas evidente
- lire l'existant avant de proposer une structure de remplacement

Pendant l'implementation :

- respecter strictement C++17 pour le code C++
- eviter les classes monolithiques
- privilegier la POO, la composition et les responsabilites nettes
- utiliser des design patterns seulement quand ils rendent le code plus lisible
- ne pas exposer d'API inutile
- optimiser les allocations, les copies et les parcours inutiles
- conserver les details d'implementation en `.cpp` ou dans des types internes
- les cpp vont dans un sous dossier src du projet concerné, les headers vont dans un sous dossier include du projet concerné
- ne pas introduire de lambda locale pour des helpers UI ou metier si elle n'est pas utilisee directement par un algorithme (`find_if`, `sort`, `visit`, etc.) ; dans ces cas preferer une fonction nommee, locale au `.cpp` si possible

Avant de terminer :

- relire le code plusieurs fois
- verifier la coherence entre code, tests, docs et standards
- mettre a jour la documentation impactee
- ajouter ou ajuster les tests utiles
- verifier que le depot reste propre

## Regles C++17

- pas d'extensions compilateur comme base de conception
- `enum class`, `constexpr`, `noexcept`, `override` et `const` quand c'est pertinent
- RAII par defaut
- `std::unique_ptr` avant `std::shared_ptr` sauf partage reel
- eviter `new` et `delete` directs hors cas d'integration bas niveau
- preferer les vues, references constantes et passages par valeur seulement quand ils sont justifies

## C++ industrial readability rules

The C++ codebase must remain readable, explicit, maintainable, and reviewable.

This project favors controlled, local improvements over broad rewrites.

Forbidden:
- artificial unused-variable suppression with `static_cast<void>(x)` or `(void)x`
- dummy names such as `_`, `unused`, `ignored`, `dummy`
- structured bindings where one bound value is unused
- immediately invoked lambdas
- lambdas used as hidden helper functions
- broad lambda captures `[&]` or `[=]` except for tiny direct STL predicates or required external callbacks
- local structs/classes inside functions
- large UI functions mixing rendering, state mutation, rollback, validation, and business logic
- manual `new`/`delete` unless forced by an external API and encapsulated
- unexplained `reinterpret_cast`, `const_cast`, or C-style casts
- broad rewrites that do not reduce concrete maintenance risk

Preferred:
- small named helper functions
- explicit domain names
- RAII
- const-correct code
- narrow scopes
- stable public APIs
- internal helpers instead of public exposure
- focused tests with readable fixtures
- incremental refactoring with unchanged behavior

## C++ readability validation before commit

Before committing C++ changes, review the output of:

```text
git grep -n "static_cast<void>"
git grep -n "(void)"
git grep -n "auto& \\[_"
git grep -n "const auto& \\[_"
git grep -n "unused"
git grep -n "ignored"
git grep -n "dummy"
git grep -n "}();"
git grep -n "\\[&\\]"
git grep -n "\\[=\\]"
```

Every hit must be reviewed. A hit may remain only if it is intentional, justified, and cleaner than the alternative.

## Architecture

- une classe ou un service = une responsabilite principale
- decouper les fonctionnalites par modules coherents
- encapsuler les invariants dans les types
- preferer la composition a l'heritage
- limiter la surface publique au strict necessaire
- garder les details Windows, rendu, I/O et serialisation localises

## Performance

- eviter les copies temporaires evitables
- reserver les conteneurs quand la taille est connue
- preferer des structures de donnees simples et previsibles
- eviter le travail cache dans les getters
- mesurer avant de complexifier une implementation

## Tests

- toute correction de bug doit avoir un test de non-regression quand c'est raisonnable
- toute nouvelle regle fonctionnelle doit avoir une couverture de test adaptee
- les tests doivent rester lisibles, determines et rapides
- privilegier le build debug Win32 pour valider localement

## Revue de code

- une revue doit etre basee sur le code reellement lu, pas sur des suppositions
- une revue doit expliciter le perimetre inspecte : fichiers, fonctions et zones non lues
- une revue doit prioritairement chercher les bugs runtime, regressions comportementales, corruptions d'etat, freezes, crashs, NaN/Inf, desynchronisations et risques memoire
- pour chaque boucle `for` ou `while` importante, verifier explicitement la progression, les pas nuls, les pas non finis, les bornes d'entree et la presence d'un garde-fou d'iterations
- pour chaque frontiere d'entree (JSON, UDP, Protobuf, plugins, editeur, API), verifier explicitement les bornes, types, tailles, ids, enums, strings, chemins et valeurs non finies
- ne jamais ecarter un risque au motif que "l'editeur ne devrait pas produire cette valeur"
- si une propriete de surete n'est pas demontree par lecture de code, la marquer comme suspecte et dire pourquoi
- les findings doivent etre ordonnes par gravite : `P0` freeze/crash/corruption, `P1` bug runtime serieux, `P2` robustesse/performance/test gap
- chaque finding doit contenir au minimum : gravite, fichier, fonction, scenario minimal, cause exacte, impact, correction recommandee et test de non-regression
- citer les lignes ou la zone de code lorsque c'est possible
- distinguer clairement les faits confirmes, les hypotheses plausibles et les points restant a verifier
- en cas d'absence de bug prouve, dire explicitement "aucun finding confirme" et lister les risques residuels ou trous de couverture
- toute revue demandant des correctifs doit etre suivie de correctifs minimaux, defensifs et testes, sans elargir l'API sans necessite
- toute revue touchant au runtime doit proposer ou ajouter des tests anti-freeze, anti-NaN/Inf, anti-entrees non bornees et des tests de non-regression adaptes
- la reponse de revue doit commencer par les findings, pas par un resume general

## Documentation

- mettre a jour `README.md` si le flux utilisateur, les cibles ou les prerequis changent
- mettre a jour `docs/` si le comportement, l'architecture ou les standards changent
- documenter les API publiques en Doxygen avec `@brief`, `@param`, `@return`, `@pre` et `@note` quand utile
- ne pas laisser la documentation contredire le code

## Build

- chemin local privilegie : presets Visual Studio 2022 Win32 Debug
- commande de base :

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
ctest --preset test-debug-win32
```

## Git Hygiene

- ne jamais ecraser des changements non lies a la tache
- ne pas reformater massivement sans raison
- limiter les diffs au perimetre utile
- garder des messages et des changements coherents

## Definition Of Done

Le travail n'est pas termine tant que :

- le design est clair
- le code est lisible
- l'API est minimale
- les tests pertinents existent
- la documentation a ete synchronisee
- le depot reste propre et maintenable
