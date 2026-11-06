# RecyOS — décisions firmware (6 novembre 2026)

Source : « RecyOS – points à trancher et ajustements » (16 points) et « Prérequis firmware ».
Légende : **Proposé** = recommandation appliquée ou prête, à valider par le CTO. **À trancher** = décision du CTO, avec ce qu'il faut pour décider.

## A. Matériel et sécurité

| Réf. | Sujet | Statut | Décision / ce qu'il faut |
|---|---|---|---|
| P1 | Broche de reset du CM5 | Proposé | GPIO.EXT broche 10 en sortie (la 9 lit les JSN-SR04T). Vérifier au banc le niveau 3,3 V côté entrée de reset du CM5. Si le déclencheur I2C des JSN échoue au banc, la broche 10 est reprise et ce point est à rouvrir. |
| P2 | Ligne d'arrêt d'urgence CM5 ↔ StamPLC | Proposé | Pas de broche dédiée en V1 : arrêt d'urgence = DI1 (contact NF) + commande UART `CMD_EMERGENCY_STOP`. À écrire dans la spec. |
| P3 | Comportement de l'arrêt d'urgence | Proposé (appliqué au contrat) | Arrêt verrouillé ; DO4 coupe la puissance ; redémarrage seulement après réarmement physique de DI1 ET `CMD_CLEAR_EMERGENCY` du CM5. |
| P4 | Mode dégradé si le CM5 ne répond plus | **À trancher** | Le StamPLC arrête les actionneurs, affiche « Hors service » sur son LCD 1,14″ (dans le coffret), reset le CM5 après 5 s puis attend. Conséquence : le citoyen ne voit rien sur le grand écran (piloté par le CM5). Décision : accepter, ou prévoir autre chose. Corriger aussi le texte du rapport BOM. |
| P5 | Montage du Nema 23 | Proposé | Un seul firmware, réglages de configuration (polarité EN, fréquence STEP max). Adaptation 3,3 V → 5 V du DM542 à valider au banc. |
| P6 | Séquence mécanique de tri | **À trancher** | Coder seulement le homing (DI6) et le déplacement en pas. Il faut du concepteur de la trappe : rôle exact du servo MG996R (angles, butées), ratio pas par mm (pas de la transmission), course utile validée (≈ 92 cm sur rail de 1 050 mm), vitesse max. |
| P7 | Fin de course et blocage des vérins | **À trancher** | Hypothèse : fins de course internes (code « Signal output » 0). Il faut du fournisseur : le YH-523D a-t-il des fins de course internes ? Quel code « Signal output » ? Courant à rotor bloqué ? À défaut, test au banc à la réception. En attendant : durée maximale par cycle (paramètre) + courant du bus. |
| P8 | Courant par moteur | **À trancher** | L'INA226 mesure le bus global. Le courant par moteur est-il exigé en V1 ? Sinon : champ renommé `bus_current_a` (déjà fait dans le contrat v2, à confirmer). |

## B. Contrat de communication (appliqué dans `contrat.proto` v2)

| Réf. | Sujet | Statut | Décision |
|---|---|---|---|
| P9 | Cadrage UART | Proposé | Garder COBS + 0x00 ; ajouter un CRC-16 par trame des deux côtés et une taille maximale (le message le plus gros fait 116 octets ; plafond à fixer, par ex. 256). **Pas encore implémenté** dans `transport.py`. |
| P10 | Contenu du rapport capteurs | Proposé (appliqué) | Rapport complet à 500 ms : 3 bacs, courant du bus, tension batterie, trappe, arrêt d'urgence, présence, température, humidité, inclinaison, DI, ports USB, drapeaux de défaut, `valid_mask` (un capteur absent n'est pas « 0 % »). À valider champ par champ. |
| P11 | Capteurs de niveau en PoC | Proposé | Couche capteur de niveau à deux pilotes (JSN-SR04T pour le PoC, VL53L0X pour la V1) ; ne coder que le JSN-SR04T. Cycle ultrason bac par bac, pas « 10 ms ». |
| P12 | Identifiants des bacs | Proposé (appliqué) | Énumération `BAC_PLASTIQUE`, `BAC_PAPIER`, `BAC_NON_RECYCLABLE` ; la compaction du papier est refusée (`ACK_INVALID`). |
| P13 | Horodatage | Proposé (appliqué) | Temps de fonctionnement dans l'enveloppe ; le CM5 envoie l'heure au démarrage (`CMD_SET_TIME`) pour régler le RTC. |

## C. Organisation

| Réf. | Sujet | Statut | Décision / ce qu'il faut |
|---|---|---|---|
| P14 | Où vit le firmware | **À trancher** | Même dépôt, un seul `contrat.proto`. Actuellement le dossier s'appelle `firmware_m5stack/` ; le rapport suggère `firmware/`. |
| P15 | Mise à jour du firmware | Proposé | V1 : table de partitions avec double partition OTA (flash 8 Mo) ; mise à jour par le CM5 via l'UART repoussée après le banc. |
| P16 | Calendrier | **À trancher** | La date du 6 novembre est atteinte. À fixer après les décisions P4, P6, P7, P10. |

## Autres décisions à prendre (documents « Prérequis »)

| Sujet | Qui décide | Ce qu'il faut |
|---|---|---|
| PCA9548A (BOM ×2, TD-01 ×2, spec ×1) | CTO | Choisir : 0 en PoC et 1 en V1, 1 + 1 de réserve, ou 2. Un seul module suffit pour 3 ToF. |
| Tension max de la MKS SERVO57A (12-24 V, bus à 25,6 V nominal, ≈ 29 V en charge) | Fournisseur / CTO | Question au fournisseur avant toute mise sous tension. Si pas de réponse : alimenter la MKS par un convertisseur 24 V régulé ou utiliser le montage de secours DM542. |
| Seuils et machine à états (60/70/75/80/95 %, cycles max) | CTO + CEO | Une page : seuil de compactage, alerte orange, alerte rouge, refus de dépôt, cycles maximum. |
| Catégories et règle de crédit sans pesée | CEO | Écrit de la CEO sur les 3 bacs et le papier. En attendant : dépôt = catégorie + horodatage, sans poids. |
| MQTT et backend | CTO | Une page : Mosquitto, Node.js / Express, topics, QoS, télémétrie, authentification. Sans effet sur le firmware du StamPLC. |

## Modifications faites le 6 novembre sur la branche `M5stack`

- `ContratCM5RPI/contrat.proto` : version 2 (valeurs 0 = `UNSPECIFIED` refusées, énumération `Bac`, rapport capteurs refondu, `CMD_CLEAR_EMERGENCY`, `CMD_SET_TIME`, version du firmware dans le heartbeat). Anciens numéros réservés.
- `contrat_pb2.py` et le code nanopb régénérés.
- `faux_mstack.py` : `session_id` tiré au sort, compteur d'Ack propre, refus des commandes invalides ; `test_client_recyos.py` adapté.
- Pas encore fait : CRC-16 (P9), `transport.py` par blocs et série, `.gitignore`, README, paquets Python, nettoyage de `Test/`, commandes du chariot et des vérins (P6, P7).
