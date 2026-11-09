# Spécification du logiciel CM5 (RecyOS) — interface avec le StamPLC

Version 0.4 du 09/11/2026 (seuil de compactage corrigé : 80 %, décision de la CEO, et alerte rouge au-dessus de 70 % après compactage) — auteur : Emmanuel Gilwandji (CTO BilengeTech). Destinataire : Aurore N'sasi (CEO), qui développe le logiciel du CM5 et l'IA.
Référence de contrat : `ContratCM5RPI/contrat.proto` **v3** (`protocol_version = 3`). En cas de divergence, le fichier `.proto` fait foi.
Le firmware du StamPLC K141 (ESP-IDF) est développé séparément ; ce document décrit ce que le CM5 doit faire et ce qu'il peut attendre du StamPLC.

## 1. Principe de responsabilité

**Le CM5 décide QUAND. Le StamPLC décide COMMENT, et refuse.**

| Le CM5 décide | Le StamPLC décide / refuse |
|---|---|
| Quel bac pour un déchet (IA) ; quand compacter (seuils) ; quand alerter le videur ; ce qui part au cloud | Pas, vitesses, rampes, fins de course ; durées maximales d'un cycle ; courant du bus ; 3 cycles de compactage maximum |
| Passage en mode vidage / maintenance | Refus si arrêt d'urgence actif, mauvais mode, chariot non référencé, cycle en cours |
| Reprise après arrêt d'urgence (`CLEAR_EMERGENCY`) | Réarmement physique de DI1 obligatoire en plus |

Le CM5 n'envoie jamais de pas moteur, d'angle de servo ni de durée de vérin : il envoie des intentions (`SORT_WASTE`, `START_COMPACTION`). Un ack négatif n'est pas une erreur du CM5 : il doit le traiter et le journaliser.

## 2. Liaison série

- UART 921 600 baud. Trame : `0x00 | COBS(Protobuf + CRC-16/CCITT-FALSE) | 0x00`. Protobuf ≤ 256 octets. Une trame au CRC faux est ignorée.
- Enveloppe : `protocol_version` (3), `message_id` (incrémental par émetteur), `session_id` (tiré au démarrage), `device_uptime_ms`, un seul `payload`.
- Une valeur d'énumération 0 (`UNSPECIFIED`) est toujours refusée. Ne jamais réutiliser un numéro de champ.
- Le code de référence côté Python : `RecyOS/transport.py` et le simulateur `ContratCM5RPI/faux_m5stack.py` (classe `EtatSimule`, qui applique les règles de refus). Développer et tester contre ce simulateur ; le test `ContratCM5RPI/test_regles_v3.py` décrit les règles.

## 3. Commandes (CM5 → StamPLC) et réponses

Chaque commande reçoit un `Ack` (`acked_message_id` + statut). Une commande sans ack sous 1 s est renvoyée au plus 2 fois avec le **même** contenu et un nouveau `message_id`.

| Commande | Rôle | Refus possibles |
|---|---|---|
| `SORT_WASTE{bac}` | Amener le dépôt au bac et le déposer (bacs 1, 2, 3) | `REFUSED_EMERGENCY`, `REFUSED_MODE`, `REFUSED_NOT_HOMED`, `BUSY`, `INVALID` |
| `START_COMPACTION{bac}` | Compacter (plastique ou non recyclable seulement) | idem ; papier → `INVALID` |
| `HOME` | Référencement du chariot (capteur M18) | `REFUSED_EMERGENCY`, `BUSY` |
| `SET_MODE{mode}` | NORMAL, VIDAGE, MAINTENANCE | `BUSY` si cycle en cours |
| `TEST_ACTUATOR`, `SET_OUTPUT` | Essais, **mode maintenance uniquement** | `REFUSED_MODE` |
| `ABORT_CYCLE` | Abandonner le cycle (résultat `ABORTED`) | — |
| `EMERGENCY_STOP` | Arrêt logiciel (coupe DO4) ; toujours accepté ; impose un nouveau `HOME` | — |
| `CLEAR_EMERGENCY` | Levée logicielle ; ne suffit pas sans réarmement physique | — |
| `READ_SENSORS`, `SET_TIME{epoch}`, `SET_LED`, `SET_CHARGE_PORT` | Lecture immédiate, mise à l'heure du RTC, LED, ports USB | — |

`SET_TIME` doit être envoyé à chaque démarrage de la liaison. Séquence de démarrage : attendre le heartbeat StamPLC → `SET_TIME` → `HOME` (si `homed = false`) → mode NORMAL.

Après `ACK_OK` d'un tri, suivre l'avancement par `ActivityStatus` (§4) ; le résultat final est `last_sort_result` (`OK`, `OBSTACLE`, `TIMEOUT`, `ABORTED`). Le CM5 n'envoie pas deux tris à la suite sans état `SORT_IDLE`.

## 4. Messages StamPLC → CM5

- **SensorReport** toutes les 500 ms : niveaux des 3 bacs (`fill_level_pct`), courant du bus, tension batterie, trappe fermée, arrêt d'urgence, présence, température, humidité, inclinaison, masque des DI, ports USB, drapeaux de défaut, **`valid_mask`**. Une valeur dont le bit de `valid_mask` est à 0 est inutilisable (capteur absent) : ne jamais l'interpréter comme 0 %.
- **ActivityStatus** : mode, `homed`, état du tri (`SORT_IDLE/MOVING/OPENING/DROPPING/CLOSING/RETURNING/FAULT`), bac en cours, état du compactage (`IDLE/RUNNING/FAULT`), numéro de cycle (1 à 3), derniers résultats. Envoyé à chaque changement d'état.
- **Alert** : sévérité + code (`MOTOR_OVERCURRENT`, `TRAPDOOR_JAMMED`, `BATTERY_LOW`, `EMERGENCY_TRIGGERED`, `EMERGENCY_CLEARED`, `COMPACTION_OBSTACLE`, `MOTOR_FAULT`, `CYCLE_TIMEOUT`, `NOT_HOMED`, `DEGRADED_MODE`) + texte ≤ 64 caractères. Toute alerte `CRITICAL` est publiée au cloud.
- **Heartbeat** toutes les 2 s dans **les deux sens** (champ `source` : STAMPLC ou CM5). Contenu StamPLC : arrêt d'urgence, autotest, version du firmware, mode. Contenu CM5 : mode et `cm5_reset_count`.

## 5. Surveillance mutuelle et mode dégradé

- Le CM5 envoie son heartbeat toutes les 2 s sans exception, y compris pendant un calcul d'IA (tâche dédiée, priorité haute).
- **CM5 silencieux 5 s** : le StamPLC arrête les actionneurs, affiche « Hors service » et commande le reset du CM5 (alerte `DEGRADED_MODE` à la reprise). Maximum 3 resets en 10 minutes ; au-delà il attend 30 minutes avant de réessayer. Après chaque reset, la règle des 5 s est suspendue pendant le délai de démarrage du CM5 (90 s, valeur supposée à mesurer sur le vrai CM5) : sans cela, le StamPLC relancerait le CM5 en boucle pendant son démarrage (voir §10).
- Au redémarrage le CM5 lit `cm5_reset_count` côté StamPLC, journalise le motif et remonte l'incident au cloud ; il ne reprend les dépôts qu'après `HOME` réussi.
- **StamPLC silencieux 5 s** (aucun heartbeat) : le CM5 passe en « borne hors service » (écran, plus de dépôt accepté), alerte `CRITICAL` au cloud, retente la liaison toutes les 5 s.
- Système de fichiers du CM5 en lecture seule en exploitation, watchdog Linux actif ; les données à conserver (file d'attente, journaux) sont dans une partition séparée.

## 6. Règles métier du CM5

1. **Type de déchet** : l'IA (vision, CPU) classe le déchet en PLASTIQUE, PAPIER ou NON_RECYCLABLE. Un déchet de type inconnu ou de faible pureté est classé NON_RECYCLABLE directement. La borne ne pèse pas.
2. **RecyCoins** = catégorie × pureté, calculés par l'IA. NON_RECYCLABLE ne donne **aucun** RecyCoin. Le firmware StamPLC ne connaît ni la pureté ni les points.
3. **Pureté / fraude** : déchet sale, mal trié ou fraude → pas de RecyCoins pour ce dépôt ; l'événement est enregistré. (Seuils de pureté : valeurs supposées à confirmer avec la CEO.)
4. **Bac plein** : la borne ne refuse jamais un dépôt. Elle affiche un message et déclenche les alertes.
5. **Seuils d'alerte au videur** (plastique et non recyclable) : 70 %, 80 %, 95 % (95 % = vidage urgent). Le papier suit des seuils distincts (valeurs supposées à confirmer). Une seule alerte par seuil et par remplissage : le compactage fait baisser puis remonter le niveau, mais les alertes ne sont réarmées qu'au vidage du bac (niveau ≤ 5 %).
6. **Compactage** : déclenché par le CM5 quand le niveau atteint 80 % (décision de la CEO, configurable), seulement en mode NORMAL et sans dépôt en cours ; 3 cycles maximum par séquence, jamais pour le papier. Le StamPLC refuse un quatrième cycle. À la fin de la séquence, si le niveau reste au-dessus de 70 %, le CM5 lance l'alerte rouge (vidage nécessaire) ; cette alerte est distincte de l'alerte de vidage urgent à 95 % (règle 5).
7. **Niveaux** : le StamPLC calcule le pourcentage de remplissage (3 mesures par bac, médiane, compensation en température). Le CM5 utilise `fill_level_pct` tel quel, sans refiltrer, et ignore un bac dont le bit de `valid_mask` est à 0 (tri ou compactage en cours dans ce bac, capteur absent, hors plage). Ce n'est pas une alerte.
   **Bac vidé** : un bac est considéré vidé quand son niveau repasse à **≤ 5 %** ; le CM5 remet alors ses compteurs d'alertes à zéro et le notifie.
8. **Modes** (le StamPLC applique, le CM5 demande).

   - NORMAL : dépôts acceptés.
   - VIDAGE : activé quand le videur scanne le QR de la borne avec l'application mobile et que le backend valide. Le CM5 envoie `SET_MODE(VIDAGE)` : tri et compactage refusés, dépôts suspendus avec un message à l'écran, délai maximal. Sortie à la validation du vidage par le videur (retour en NORMAL ; `HOME` si `homed = false`).
   - MAINTENANCE : accès local authentifié depuis l'écran du CM5 uniquement (pas d'entrée à distance). Dépôts suspendus, vitesses réduites, tests limités dans le temps, sortie automatique, arrêt d'urgence toujours prioritaire. Retour en NORMAL seulement après `HOME`.
   - Le badge NFC du technicien est prévu pour la version suivante.
9. **Arrêt d'urgence** : tout arrêt impose un nouvel `HOME` avant le prochain tri. Le CM5 affiche l'état et alerte le cloud.

## 7. Hors ligne et cloud

- Les dépôts se déroulent normalement sans réseau. Chaque événement (dépôt, alerte, télémétrie, vidage) est écrit dans une file locale persistante (SQLite) et rejoué dans l'ordre quand la connexion revient. Capacité visée : 7 jours.
- Télémétrie : un message par borne toutes les 15 minutes (niveaux, tension, température, état, version).
- Transport : MQTT (Mosquitto) pour les événements et la télémétrie, REST pour les requêtes et l'administration. Les formats et sujets définitifs suivent le fichier d'écarts `Ecarts_API_Backend_vs_decisions_v0.9` ; en cas de conflit, le contrat API récent fait foi.
- Identifiants uniques par événement (UUID) pour que le backend déduplique les rejeux.
- Pas de portes ni de serrures intelligentes : le CM5 n'envoie aucune commande de verrouillage et ne gère aucun capteur de porte.

## 8. Mise à jour du firmware du StamPLC
- PoC et banc : par câble USB, sans intervention du CM5. La version du firmware est dans chaque heartbeat du StamPLC (`firmware_version`, format `0xMMmmpppp`) ; le CM5 la journalise et la remonte au cloud.
- Mise à jour par le CM5 : **pas en V1**, après le banc. Elle se fera par messages de transfert ajoutés au contrat (début, blocs, fin avec SHA-256), seulement en mode maintenance. Le CM5 n'a rien à implémenter pour l'instant.

## 9. Livrables attendus et tests

- Module de pont série (`RecyOS/m5stack_bridge.py`) : envoi des commandes avec ack/renvoi, réception, heartbeat bidirectionnel, machine d'état de la liaison (§5).
- Tests automatisés contre `faux_m5stack.py` : démarrage nominal, refus (non référencé, urgence, mode, occupé), perte de heartbeat dans les deux sens, trames corrompues, redémarrage du StamPLC (nouveau `session_id`).
- Point ouvert côté CM5 : durées exactes des cycles et valeurs de seuils de pureté ; le StamPLC fournit en attendant des valeurs provisoires.

## 10. Délais et non-chevauchement

Ces valeurs sont cohérentes entre elles ; si le développeur en change une, il doit vérifier les règles de la dernière colonne.

| Minuterie | Valeur | Cohérence |
|---|---|---|
| Heartbeat du StamPLC et du CM5 | 2 s chacun | Le silence se déclare à 5 s : il faut avoir manqué deux heartbeats de suite |
| Silence déclaré (dans les deux sens) | 5 s | StamPLC : arrêt des actionneurs puis reset du CM5. CM5 : borne « hors service » |
| Ack d'une commande | 1 s, 2 renvois (3 s au plus) | Moins que les 5 s : un renvoi ne déclenche jamais le mode dégradé ; c'est le heartbeat qui détecte la perte de liaison |
| Délai de démarrage du CM5 après un reset | 90 s (supposé, à mesurer) | La règle des 5 s est suspendue pendant ce délai ; sinon 3 resets seraient consommés en 15 s |
| Resets du CM5 | 3 par 10 min, puis pause de 30 min | On compte les resets, pas les silences de 5 s ; avec le délai de 90 s, 3 resets prennent au moins 4 min 30 s |
| Rapport capteurs | 500 ms | Cycle de mesure des niveaux : moins de 300 ms pour 3 bacs ; médiane glissante sur 5 cycles (environ 2,5 s de lissage), donc pas de filtre supplémentaire côté CM5 |
| Télémétrie vers le cloud | 15 min | Indépendante des alertes, qui partent tout de suite |
| Durée maximale du mode VIDAGE | 30 min (supposé, configurable) | Sortie automatique vers NORMAL si le videur ne valide pas |
| Durée maximale du mode MAINTENANCE | 15 min d'inactivité (supposé, configurable) | Sortie automatique ; l'arrêt d'urgence reste prioritaire |

Règles complémentaires :

- **Mode après un reset du CM5** : le StamPLC conserve son mode (VIDAGE reste un état sûr). Au redémarrage, le CM5 lit le mode dans le heartbeat et dans `ActivityStatus` et se resynchronise ; il ne suppose jamais NORMAL.
- **Demande de mode pendant un cycle** : le StamPLC répond `ACK_BUSY`. Le CM5 cesse d'accepter de nouveaux dépôts, attend la fin du cycle (`SORT_IDLE` et `COMPACTION_IDLE`), puis renvoie `SET_MODE`.
- **Pendant un compactage ou un tri**, le niveau du bac concerné est invalide (`valid_mask`) : aucune alerte ni décision ne doit s'appuyer sur ce bac pendant ce temps.
- **Heartbeat en dégradé** : le heartbeat du CM5 continue d'être envoyé pendant toute la séquence de reset et de reprise.

