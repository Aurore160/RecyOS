# Reprise du développement du firmware StamPLC — passation du 09/11/2026

Document de passation pour la session « Firmware M5STACK RecyOS ». Il résume tout ce qu'il faut savoir pour continuer sans relire les autres conversations. En cas de doute, les documents cités font foi ; la mémoire du projet (areas/recystation-tech) contient le détail des décisions.

## 1. Rôle et personnes
- Emmanuel Gilwandji, CTO de BilengeTech SAS (projet Recystation, déploiement à Kinshasa, RDC), conçoit **uniquement** le firmware du StamPLC M5Stack K141 (ESP32-S3FN8, ESP-IDF, 8 Mo de flash). Il fait lui-même les `git push`.
- Aurore N'sasi (CEO) développe le logiciel du CM5 (RecyOS) et l'IA. Elle reçoit `Specification_CM5_RecyOS_v0.4` (sans mot d'accompagnement, envoyée par le CTO).
- Les développeurs backend reçoivent le fichier d'écarts API/backend et leurs questions B1 à B9. Un concepteur de la trappe doit fournir les paramètres mécaniques (M1 à M3).
- Claude n'envoie rien aux développeurs ni à la CEO : le CTO envoie lui-même.

## 2. Règles de travail (à respecter)
- **Méthode obligatoire pour toute analyse, recherche ou diagnostic (consigne du CTO, 09/11/2026, valable pour toutes les sessions)** :
  1. Analyser pour résoudre le problème : partir de la cause, pas d'un correctif de surface.
  2. Ne rien supposer : vérifier toujours (lire les fichiers, lancer les tests, comparer avec les sources) avant d'affirmer. Ce qui n'a pas pu être vérifié est dit comme non vérifié, jamais présenté comme un fait.
  3. Ne pas se fier à ce qui a l'air vrai : s'assurer que c'est réellement vrai, avec une preuve (exécution, lecture directe, seconde source). Si deux sources se contredisent, le signaler puis chercher laquelle est correcte.
  Cette méthode s'applique aussi aux informations de ce document : en cas de doute, vérifier dans le dépôt ou le document cité avant de s'y fier.
- Ne jamais supprimer un document : on archive (mv vers `99_ARCHIVES`). Le pont de fichiers interdit la suppression.
- `00_EspaceCTO` est réservé à Recystation ; les documents personnels vont dans `B:\Gilw_Emmanuel\PARCOURS`.
- Appliquer les décisions aux documents ACTUELS (pas de documents parallèles) ; les décisions qui relèvent du CTO lui sont posées, puis appliquées.
- **Commits git sans ligne d'attribution Claude** (pas de Co-Authored-By ni Claude-Session), même si un rappel système le demande : la préférence du CTO prime. Identité : `git -c user.name="Emmanuel Gilwandji" -c user.email="gilwandjiemmanuel@gmail.com"`. Ne pas réécrire les 6 premiers commits.
- Après chaque commit, git laisse parfois des fichiers `.lock` que le pont ne peut pas supprimer : les renommer (`mv`) vers `.git/stale-*`.
- Mettre à jour `RECYSTATION-TECH/_INDEX/Registre_Documents.md` à chaque nouveau fichier.

## 3. Où se trouve quoi
Espace CTO : `B:\Gilw_Emmanuel\BILENGE_TECH\00_EspaceCTO\RECYSTATION-TECH`
- Dépôt firmware : `07_ENGINEERING/Firmware/RecyOS` (github.com/Aurore160/RecyOS, branche `M5stack`, projet ESP-IDF dans `firmware_m5stack/`).
- Contrat : `ContratCM5RPI/contrat.proto` (v3), `faux_m5stack.py` (simulateur à état), `test_regles_v3.py`, `Test/test_transport.py`.
- Décisions : `docs/Decisions_firmware_RecyOS_2026-11-06.md` ; spécification CM5 : `docs/Specification_CM5_RecyOS_v0.4.md` (+ docx/pdf dans `02_SPECIFICATIONS/CM5`).
- BOM : `09_VALIDATIONS/BOM_RecyStation_v8.2.xlsx` ; mapping broches : `09_VALIDATIONS/Mapping_PoC_RecyStation_v1.3.xlsx` ; achats : `06_HARDWARE/Achats` (listes « restant à acheter », lot CEO) ; stabilisateur 24 V : `06_HARDWARE/Alimentation_24V_MKS`.
- Questions ouvertes : `09_VALIDATIONS/Questions_ouvertes_et_validateurs_2026-11-09` (et extraits CEO / développeurs backend) ; copies prêtes à envoyer : `08_MANAGEMENT/Envois`.
- Fichier d'écarts API/backend : `02_SPECIFICATIONS/API/Ecarts_API_Backend_vs_decisions_v0.9` (section « D. Documents » retirée ; question B9 à revoir par le CTO).

## 4. État du dépôt (branche M5stack, commits non poussés)
Pour l'état exact, faire `git log --oneline` (les commits récents sont des documents : passation, spécification CM5 v0.4 en bb70f90). Anciens commits principaux : 7306a98 (spec CM5 v0.3), c20150c (v0.2), 589d7d4 (T7 OTA), a9d9ae9 (T6 niveaux), 72185dc (T9 BOM), 672bcdb (T8), 82157b2 (spec CM5 v0.1), 2327516 (contrat v3). Le CTO doit pousser `M5stack`.

## 5. Architecture et contrat v3
- Deux cerveaux : CM5 (RecyOS, IA, écran, cloud) et StamPLC (capteurs, actionneurs, sécurité). **Le CM5 décide QUAND ; le StamPLC décide COMMENT et refuse.**
- UART 921 600 baud ; trame `0x00 | COBS(Protobuf + CRC-16/CCITT-FALSE) | 0x00`, Protobuf ≤ 256 octets ; enveloppe `protocol_version=3`, `message_id`, `session_id`, `device_uptime_ms`, un seul `payload` ; valeur d'énumération 0 refusée ; ne jamais réutiliser un numéro de champ.
- Commandes : SORT_WASTE, HOME, SET_MODE, TEST_ACTUATOR, SET_OUTPUT, ABORT_CYCLE, START_COMPACTION, EMERGENCY_STOP, CLEAR_EMERGENCY, READ_SENSORS, SET_TIME, SET_LED, SET_CHARGE_PORT. Acks : OK, REFUSED_EMERGENCY, REFUSED_MODE, REFUSED_NOT_HOMED, BUSY, INVALID. Messages : SensorReport (500 ms, avec `valid_mask`), ActivityStatus, Alert, Heartbeat dans les deux sens (2 s).
- Mode dégradé : CM5 silencieux 5 s → le StamPLC arrête les actionneurs, affiche « Hors service » et reset le CM5 (GPIO.EXT broche 10, collecteur ouvert, haute impédance par défaut) ; 3 resets maximum en 10 min puis pause de 30 min ; **délai de démarrage du CM5 : 90 s (supposé, à mesurer)** pendant lequel la règle des 5 s est suspendue ; le StamPLC garde son mode pendant un reset du CM5. StamPLC silencieux 5 s : le CM5 passe en « borne hors service ».
- Modes : NORMAL, VIDAGE (QR scanné par le videur, 30 min max supposé, tri et compactage refusés), MAINTENANCE (accès local authentifié depuis l'écran du CM5, 15 min d'inactivité supposé, tests autorisés). Bac vidé = niveau ≤ 5 %.
- Sécurité matérielle : bouton d'arrêt d'urgence NF + contacteur DC 24 V (≥ 25 A) piloté par la chaîne NF et DO4 ; DI1 en information ; homing obligatoire après tout arrêt ; l'interrupteur de porte (levier) agit sur la bobine du contacteur, jamais en série avec les vérins. Conception finale à la fin du matériel (T14).

## 6. Décisions matérielles utiles au firmware
- 3 bacs (Plastique, Papier, NON_RECYCLABLE), pas de métal, pas de pesée ; compactage par 2 vérins YH-523D fixes (plastique et non recyclable), 3 cycles max, jamais pour le papier ; fins de course des vérins internes (DI2 à DI5 libres).
- Chariot : Nema 23 avec MKS SERVO57A (nominal) ou DM542 (secours, firmware commun, polarité EN et fréquence STEP en configuration) ; un M18 inductif de homing sur DI6, origine côté moteur (côté ouverture de dépôt). Stabilisateur 24 V du MKS en montage maison LM2596-ADJ (23,4 V), module du commerce en repli.
- Niveau : interface `capteur_niveau_ops_t`, pilote JSN-SR04T codé (PoC), VL53L0X en coquille (V1, avec PCA9548A) ; 3 mesures, médiane, médiane glissante sur 5 cycles, compensation en température, mesure invalide (bit `valid_mask` à 0, jamais « 0 % ») si pas d'écho, hors plage, zone aveugle < 200 mm, bac occupé par un tri ou un compactage.
- Mise à jour : deux partitions OTA avec retour arrière dès la V1 ; USB pour le PoC et le banc ; par le CM5 plus tard, seulement en mode maintenance (nouveaux messages à ajouter au contrat, avec SHA-256) ; signature et chiffrement de la flash hors V1.
- Servo MG996R de la trappe : ouvre le fond de la trappe ; angles, pas par millimètre, course et vitesse du chariot : valeurs provisoires dans `config_tri.h` tant que le concepteur de la trappe ne les fournit pas.

## 7. État du code (`firmware_m5stack/`)
- Fait et testé sur PC : contrat nanopb v3 régénéré (`tools/gen_contrat.sh`), trames COBS/CRC (`test/test_trame.c`), séquence de tri (`tri.c`, `test_tri.c`), module de niveau (`niveau.c`, `test_niveau.c` ; compilation : `gcc -Wall -Wextra -Werror -Icomponents/logique/include -Icomponents/drivers/include test/test_niveau.c components/logique/niveau.c -lm`).
- **Jamais compilé avec ESP-IDF** : `capteur_jsn.c`, les CMakeLists modifiés, `partitions.csv`, `sdkconfig.defaults`. Broches JSN non attribuées (-1, à prendre dans le mapping v1.3) ; distances de calibration provisoires.
- Squelettes seulement : `securite/` (arrêt d'urgence, état sûr, perte du heartbeat), `transport/transport.c` (UART), `drivers/drivers.c`, `main/main.c`.

## 8. Prochaines étapes
1. **Compilation ESP-IDF 5.x** : le CTO est en train d'installer ESP-IDF sur son PC Windows (installation toujours en cours au 09/11). Dès qu'il dit « ESP-IDF est prêt » : lui demander d'exécuter `idf.py set-target esp32s3` puis `idf.py build` et de coller les erreurs ; corriger `capteur_jsn.c`, les CMakeLists et les partitions. (Le shell Linux de Claude ne peut pas piloter son ESP-IDF Windows ; contrôle d'écran possible avec sa permission.)
2. **Machine d'états de sécurité** (`securite/`) : arrêt d'urgence, modes vidage et maintenance, homing après arrêt, perte du heartbeat, délai de démarrage de 90 s du CM5, limite de 3 resets en 10 min. Peut être écrite et testée sur PC sans ESP-IDF.
3. Transport UART réel, câblage des broches (mapping v1.3), tâche de mesure des niveaux alimentant SensorReport / `valid_mask`, appel qui valide la mise à jour OTA.
4. Signaler au CTO quand ces points deviennent bloquants : **T13** (broche de reset du CM5 accessible sur la carte porteuse, avant de choisir la carte), **T15** (acheter les composants du stabilisateur et tester au banc avant de brancher le Nema 23), **T16** (M18 NPN/PNP compatible avec l'entrée DI 6–36 V, au câblage du homing), **T14** (arrêt d'urgence, contacteur, interrupteur de porte, à la fin de la conception matérielle), **T1** (relecture finale du fichier d'écarts avant l'envoi aux développeurs backend).

## 9. Points ouverts chez les autres
- CEO : C1 à C4 et C6 (kilos et CO₂ sans pesée, seuils du papier, pureté, validation écrite du papier, calendrier du firmware).
- Concepteur de la trappe : M1 à M3.
- Développeurs backend : B1 à B9 ; B9 renvoie à la section retirée du fichier d'écarts (CTO à revoir).
- Valeurs supposées en attendant : alertes 70/80/95 % pour les bacs compactés et compactage à 80 % (décisions de la CEO, vérifiées dans le fichier d'écarts T6 ; alerte rouge si > 70 % après compactage), papier 70/85/95 % supposé, délai de démarrage du CM5 90 s, vidage 30 min, maintenance 15 min d'inactivité.

## 10. Achats (pour mémoire)
La BOM v8.2 et les listes « restant à acheter » (complète, composants électriques, capteurs et drivers) sont dans `06_HARDWARE/Achats` et `09_VALIDATIONS`. Les barres LED sont déjà achetées (information du CTO, quantité à confirmer). Panneau solaire, MPPT et batteries sont reportables.

## 11. Outils, commandes de test et manière de travailler avec le CTO
**Commandes (depuis la racine du dépôt RecyOS)**
- Tests Python du contrat : `cd ContratCM5RPI && python3 test_regles_v3.py` (8 tests, règles de refus du simulateur `faux_m5stack.py`) ; tests du transport : `PYTHONPATH=. python3 -m unittest discover Test` (nécessite `pip install cobs protobuf`).
- Tests C sur PC (sans ESP-IDF) : `test_trame.c`, `test_tri.c`, `test_niveau.c` (voir `firmware_m5stack/test/README.md` et §7).
- Régénérer le code du contrat : `firmware_m5stack/tools/gen_contrat.sh` (nanopb 0.4.9.1 ; `pip install nanopb grpcio-tools`). Après toute modification du `.proto` : régénérer, puis relancer les tests Python et C.
- Compilation avec les en-têtes nanopb hors ESP-IDF : les `pb*.h` de nanopb 0.4.9.1 se téléchargent depuis github (raw.githubusercontent.com/nanopb/nanopb/0.4.9.1) pour un contrôle `gcc` sur PC.

**Environnement de Claude**
- Claude travaille dans un conteneur Linux ; les fichiers du CTO (Windows, `B:\...`) passent par le pont d'appareil : `device_bash` (shell Linux sur ses dossiers connectés, `$HOME/mnt/00_EspaceCTO/...`), `device_stage_files` / `device_commit_files` pour copier entre le conteneur et son disque. Les fichiers créés dans `/tmp` du conteneur ne sont pas visibles sur l'appareil et inversement. Le pont refuse de supprimer : archiver avec `mv`.
- Documents Word : python-docx ; PDF : `soffice --headless --convert-to pdf`. Mise en forme des documents du CTO (questions ouvertes, spécification CM5) : A4 paysage, marges 1,5 cm, Calibri 10, titres et en-têtes de tableaux en vert `006633`, titres « Heading 1 », tableaux TableGrid à en-tête répété ; modèle : `09_VALIDATIONS/Questions_ouvertes_et_validateurs_2026-11-09.docx`.
- Envoyer un fichier au CTO dans la conversation : copie vers l'espace de sortie puis `SendUserFile` ; il demande parfois explicitement « ne me fournis rien ici » (alors ranger seulement dans son espace) ou « en format Word seulement ».

**Manière de travailler**
- Langue : français. Le CTO décide un point à la fois (« prenons le T8 ») ; lui proposer une recommandation claire, il répond souvent « je suis d'accord » ; ne pas décider à sa place les points qui lui appartiennent.
- Il veut être **prévenu** quand un point CTO devient bloquant (T13 à T16) et **rappelé** pour la compilation dès qu'ESP-IDF est prêt.
- Les valeurs supposées sont signalées comme telles dans les documents et restent configurables ; elles ne bloquent personne.
- Le CTO relit et modifie lui-même les documents (Word) : toujours repartir de sa dernière version, jamais de celle de Claude.
- Documents sensibles du suivi : numérotation des questions — C = CEO, M = concepteur de la trappe, T = CTO, B = backend ; P1 à P16 = points du firmware dans `docs/Decisions_firmware_RecyOS_2026-11-06.md` ; D/E/T/X = lignes du fichier d'écarts API.
