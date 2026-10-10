# Relecture de l'intégration ExternalStream

Après corrections, aucun finding confirmé restant dans le périmètre ci-dessous.
Cette conclusion concerne le code relu et les validations décrites ; elle ne
constitue pas une garantie pour tous les pilotes graphiques.

## Périmètre

- Modèle, validation JSON/runtime et sérialisation éditeur : `Reticle.h`,
  `RuntimeBudgets.h`, `JsonLoader.cpp`, `DocumentSemanticValidator.cpp`,
  `EditorDocumentSerializer.cpp`.
- Pipeline des commandes : `CommandTypes`, Protobuf, `SceneRegistry`,
  `UdpRuntimeBridge`, deltas de `Animation.cpp`, feedback et générateur d'API.
- Rendu : branche ExternalStream de `Canvas2D`, comparaison avec Image,
  `MfdRenderer`, `OffscreenPageRenderer`, `ExternalStreamRegistry`.
- Éditeur : création/ajout, inspecteur, aperçu, sélection, limites géométriques,
  export et destruction des ressources avant fermeture du contexte.
- Transport natif : tous les fichiers de `mfd_external_stream`, contrat binaire,
  générations, trois surfaces, requêtes de fin GPU, verrous et déconnexion.
- Exemple Python/DLL, CMake, scripts de lancement et exclusions CI.

Les algorithmes historiques sans rapport avec cette intégration et le code des
dépendances tierces n'ont pas fait l'objet d'un nouvel audit exhaustif.

## Problèmes trouvés et corrigés pendant la validation

| Gravité | Zone / scénario minimal | Cause et impact | Correction et couverture |
|---|---|---|---|
| P1 | `UdpRuntimeBridge.cpp`, `MergePrimitivePatch` : Stop puis Play regroupés | Le dernier mode pouvait effacer la barrière Stop et conserver une ancienne image | Conservation du token d'invalidation ; test `CoalescingPreservesExternalStreamStopThenPlayBarrier` et test du handle généré |
| P1 | `LatestBatchPublisher.cpp`, fusion pendant un envoi occupé ou une reprise | Les nouveaux champs pouvaient disparaître lors du remplacement d'un batch en attente, notamment avec les deltas dynamiques | Fusion statique partagée avec le runtime dans `internal/ExternalStreamPatch.h` ; conservation ordonnée des contrôles dynamiques avant le nouveau delta. Tests Stop→Play, Pause, commandes groupées, reprises répétées, noms/identifiants et suppression/recréation |
| P1 | `StreamReceiver.cpp`, construction d'un second consommateur du même endpoint | La destruction du consommateur refusé pouvait marquer le premier comme arrêté | Fermeture du mapping refusé avant destruction ; test GPU du consommateur dupliqué |
| P1 | `OffscreenPageRenderer.cpp`, fenêtre désactivée | Un dessin de texture pouvait subsister alors que l'enregistrement de sa lecture était désactivé | Traitement de contrôle puis retour avant tout dessin ; suites de rendu hors écran et relecture du cycle de fence |
| P2 | Générateur : flux dans un réticule dynamique ou strobe | Handle créé sans liaison au cache de feedback | Liaison lors de la création ; test C++ compilé du réticule dynamique et tests Python du générateur |
| P2 | Création/ajout de primitives dans l'éditeur | Identifiant de flux par défaut identique pour plusieurs primitives | Identifiant dérivé du propriétaire et de la primitive ; test `NewStreamsHaveDistinctValidIdentifiersAcrossOwnersAndPrimitives` |
| P2 | Exemple Python et staging | DLL dépendante non trouvée avec Python 3.7 ; scripts supprimés par le staging global | Chemin DLL adapté et scripts enregistrés dans le mécanisme de staging existant ; lancement réel de l'exemple |

## Cohérence avec les autres primitives

Le nouveau type est ajouté en fin d'enum et de variant. Les types existants
gardent leur ordre. Le flux utilise les transformations communes et les mêmes
conventions d'orientation/mirroir que les images. Visibilité, ordre des couches,
superpositions et clipping du réticule restent dans le pipeline existant.
Comme une image, le flux n'est pas lui-même une primitive de masque ou de contour.

La sélection et les bornes utilisent son rectangle logique. La taille des pixels
du producteur ne modifie pas ce rectangle. La sauvegarde conserve la configuration
et le mode initial, sans enregistrer les états GPU, les compteurs ou Pause.
Les copies de templates conservent volontairement leur source configurée ; deux
consommateurs simultanés du même identifiant sont signalés, conformément à la
limite d'un consommateur par flux.

## Vérifications et limites

Validation finale Debug Win32 : compilation complète réussie ; 371 tests MFD,
113 tests clients, 258 tests éditeur et 14 tests runtime réussis. Trois tests
runtime du tutoriel sont ignorés car ses ressources ne sont pas présentes.
Les 19 tests CTest ciblant ExternalStream et le générateur passent, ainsi que
l'essai GPU avec producteur 64 bits et lecteur 32 bits et l'exemple Python réel.

Les tests des bibliothèques MFD, cliente et éditeur, les tests du générateur,
du protocole et le test GPU local ont été exécutés. Le test GPU vérifie chaque
pixel en RGBA/BGRA, l'alpha, l'ordre des lignes, Pause/Stop/masquage, une perte du
producteur et une reconnexion avec changement de dimensions. Une exécution
supplémentaire utilise un producteur 64 bits face au lecteur 32 bits.

L'exemple Python a aussi été vérifié avec deux flux simultanés, lecture des
feedbacks UDP et capture visuelle montrant les mires, compteurs et primitives
MFD superposées. Le guide décrit les commandes reproductibles et le matériel.

Risques résiduels : validation GPU limitée au matériel local ; perte physique
forcée du périphérique non injectée. Les erreurs de pilote sont traitées par
endpoint avec diagnostic, mais les appels du pilote n'offrent pas une borne
temps réel stricte. Le comportement à 50 Hz est une cible, pas une garantie de
latence de bout en bout.
