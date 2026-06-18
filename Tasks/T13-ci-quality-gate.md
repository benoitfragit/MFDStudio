# T13 — Garde-fou CI (clang-tidy + grep AGENTS.md)

- **Criticite** : P2 (prevention de regression / reduction des cycles CI)
- **Vague CI** : 0 (a faire en premier ; independant)
- **Statut** : PROPOSITION

## Description breve

`.clang-tidy` n'echoue jamais (`WarningsAsErrors: ''`) et la CI ne lance ni clang-tidy ni
le script de grep d'anti-patterns que AGENTS.md decrit. Les anti-patterns ne sont donc
detectes par personne avant la revue. Ajouter un filet rapide qui echoue tot, hors de la
matrice Windows lourde.

## Fichiers impactes

- `.github/workflows/` — nouveau job de lint leger (Linux)
- `Scripts/check_agents_patterns.*` — nouveau script de garde
- `.clang-tidy` / `CMakeLists.txt` (`MFD_ENABLE_WARNINGS_AS_ERRORS`) — activation progressive
- `docs/DEVELOPMENT.md` — usage local

## Contrainte de dev (AGENTS.md)

- section « C++ readability validation before commit » (la liste de grep a reprendre)
- « Every hit must be reviewed [...] intentional, localized, justified »
- Build : « presets Visual Studio 2022 Win32 » restent la reference de build ; le lint est
  un job **separe** pour ne pas multiplier les builds lourds

## Strategie de resolution detaillee

1. Script `Scripts/check_agents_patterns` reprenant les grep de AGENTS.md, renvoyant un
   code d'echec sur motifs interdits **non justifies**, avec allowlist explicite pour les
   frontieres C deja auditees (sockets, WGL/GL, Win32, protobuf wire — cf. revue casts).
2. Job CI Linux leger : execute le script + `clang-tidy` sur `compile_commands.json`
   (deja exporte), **separe** de la matrice Windows.
3. Activer `MFD_ENABLE_WARNINGS_AS_ERRORS=ON` **apres** nettoyage des vagues, d'abord sur
   un seul preset.

## Strategie de test

- Le script echoue sur un cas introduit volontairement (ex. `}();` non justifie) et
  reussit sur l'arbre courant (apres allowlist).
- Le job CI s'execute en quelques secondes et bloque la PR avant les builds Windows.

## Documentation impactee

`docs/DEVELOPMENT.md` : documenter l'execution locale du script avant push.
