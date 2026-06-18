# T07 — Supprimer l'imbrication `stateMutex` sous les mutex de file

- **Gravite** : P2 (danger latent d'ordre de verrouillage / serialisation)
- **Module** : `mfd_api/src/control/UdpRuntimeBridge.cpp`
- **Fonctions** : `Impl::PushQueuedBatch` (l. ~609-624), `Impl::PushQueuedFeedback` (l. ~634-639)
- **Vague CI** : 2
- **Statut** : PLAUSIBLE

## Scenario minimal

Fort debit de commandes + feedback : `PushQueuedBatch` detient deja `inboundMutex`
(l. ~609) et `PushQueuedFeedback` detient `outboundMutex` (l. ~634), puis appellent
`SetCommandStatus` / `SetFeedbackStatus` qui verrouillent `stateMutex`.

## Cause exacte

`stateMutex` est donc imbrique **a l'interieur** de `inboundMutex` / `outboundMutex`.
Aucun chemin actuel ne prend inbound/outbound en tenant `stateMutex`, donc pas
d'interblocage aujourd'hui — mais c'est un ordre de verrouillage non documente et
fragile, et la file reste serialisee sur le mutex d'etat.

## Impact

Pas de deadlock prouve. Risque latent en cas d'evolution + contention inutile de la
file sur `stateMutex`. Viole l'esprit AGENTS.md « encapsuler les invariants » et
predictibilite du runtime.

## Correction recommandee (minimale)

Construire la chaine de statut **sous** le verrou de file, **liberer** le verrou de
file, puis appeler `Set*Status`. A defaut, documenter explicitement l'ordre de
verrouillage `inbound/outbound > state` dans le code et garantir qu'aucun chemin ne
l'inverse.

## Test de non-regression

Test multithread (ideanlement sous ThreadSanitizer) martelant `PushQueuedBatch` /
`PushQueuedFeedback` en concurrence avec `MetricsSnapshot` / `IsRunning` ; absence de
data race signalee et progression garantie.
