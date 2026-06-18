# T13 — Garde-fou CI (clang-tidy + grep AGENTS.md)

- **Gravite** : P2 (prevention de regression / reduction des cycles CI)
- **Module** : `.github/workflows/`, `Scripts/`, `.clang-tidy`
- **Vague CI** : 0 (a faire en premier ; independant)
- **Statut** : PROPOSITION

## Constat

- `.clang-tidy` definit un set de checks correct mais `WarningsAsErrors: ''` : rien
  n'echoue. La CI (`ci.yml`) ne lance **ni** clang-tidy **ni** le script de grep
  d'anti-patterns que AGENTS.md decrit pourtant (« C++ readability validation before
  commit »).
- `MFD_ENABLE_WARNINGS_AS_ERRORS` existe (CMake) mais vaut `OFF`.
- Resultat : les anti-patterns AGENTS.md (lambdas-helpers, casts non expliques, `(void)`,
  `using namespace`, structs locales...) ne sont detectes par personne avant la revue.

## Objectif

Faire echouer **tot et localement** (avant de consommer un des 4 builds Windows de la
CI) toute reintroduction des classes de defauts listees par AGENTS.md, et stabiliser la
qualite pendant que les vagues 1-5 reduisent la dette.

## Correction recommandee (minimale, additive)

1. **Script de garde** `Scripts/check_agents_patterns.*` reprenant la liste de grep de
   AGENTS.md (section « C++ readability validation before commit ») et renvoyant un code
   d'echec sur les motifs interdits **non justifies** (allowlist explicite pour les
   frontieres C deja auditees : sockets, WGL/GL, Win32, protobuf wire — cf. revue casts).
2. **Job CI dedie** (Linux, leger) executant ce script + `clang-tidy` sur
   `compile_commands.json` (deja exporte via `CMAKE_EXPORT_COMPILE_COMMANDS`), **separe**
   de la matrice de build Windows pour ne pas rallonger ni multiplier les builds lourds.
3. Activer progressivement `MFD_ENABLE_WARNINGS_AS_ERRORS=ON` **une fois** les vagues
   nettoyees, d'abord sur un seul preset.

## Reduction des « build go »

Un job de lint rapide qui echoue en quelques secondes evite de declencher (et d'attendre)
4 builds Windows pour un defaut trivial. Le script est aussi executable en local avant
push, ce qui supprime les allers-retours CI.

## Test / validation

- Le script doit echouer sur un cas de test introduit volontairement (ex. une lambda
  `}();` non justifiee) et reussir sur l'arbre courant (apres allowlist des frontieres
  C deja auditees).
- Documenter l'usage dans `docs/DEVELOPMENT.md`.
