# T07 — Supprimer l'imbrication `stateMutex` sous les mutex de file

- **Criticite** : P2 (danger latent d'ordre de verrouillage / serialisation inutile)
- **Vague CI** : 2
- **Statut** : PLAUSIBLE

## Description breve

`PushQueuedBatch` (tient `inboundMutex`) et `PushQueuedFeedback` (tient `outboundMutex`)
appellent `Set*Status` qui verrouille `stateMutex` : `stateMutex` est donc imbrique sous
les mutex de file. Pas de deadlock aujourd'hui, mais ordre de verrouillage non documente
et contention inutile de la file sur le mutex d'etat.

## Fichiers impactes

- `mfd_api/src/control/UdpRuntimeBridge.cpp` — `Impl::PushQueuedBatch` (l. ~609-624),
  `Impl::PushQueuedFeedback` (l. ~634-639), `SetCommandStatus`/`SetFeedbackStatus`
- `tests/mfd_api/` — test multithread (idealement sous TSan)

## Contrainte de dev (AGENTS.md)

- « garder un code [...] predictible » ; « encapsuler les invariants »
- « risques memoire / desynchronisations » = cible prioritaire de la revue
- « toute revue touchant au runtime doit proposer ou ajouter des tests anti-freeze »

## Strategie de resolution detaillee

1. Construire la chaine de statut **sous** le verrou de file, liberer ce verrou, puis
   appeler `Set*Status` (qui prend `stateMutex`). Plus aucune imbrication.
2. A defaut (si la construction depend de l'etat protege par `stateMutex`), documenter
   explicitement l'ordre `inbound/outbound > state` et garantir qu'aucun chemin ne l'inverse.

## Strategie de test

- Test concurrent martelant `PushQueuedBatch`/`PushQueuedFeedback` contre
  `MetricsSnapshot`/`IsRunning` ; absence de data race (TSan) et progression garantie
  (anti-freeze).

## Documentation impactee

Commentaire d'ordre de verrouillage dans `UdpRuntimeBridge.cpp` ; `docs/architecture/`
si le modele de threading du bridge y est decrit.
