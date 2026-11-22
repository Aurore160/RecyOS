# Module de sécurité du StamPLC — conception et état (17 novembre 2026)

Code : `firmware_m5stack/components/securite/` (C pur, sans ESP-IDF). Tests PC : `test/test_securite.c` (deux configurations) et `test/verifier_enums_securite.py` (valeurs identiques à `contrat.proto` v3).

```
cd firmware_m5stack
SRC="components/securite/securite.c components/securite/liaison_cm5.c components/securite/garde_verin.c"
gcc -Wall -Wextra -Werror -Wpedantic -std=c11 -Icomponents/securite/include test/test_securite.c $SRC -o ts && ./ts              # pilote : sans bouton ni contacteur
gcc -Wall -Wextra -Werror -Wpedantic -std=c11 -DSEC_BOUTON_URGENCE_PRESENT=1 -DSEC_CONTACTEUR_PRESENT=1 -Icomponents/securite/include test/test_securite.c $SRC -o ts && ./ts
python3 test/verifier_enums_securite.py
# simulateur du CM5 (depuis la racine du dépôt RecyOS)
PYTHONPATH=.:ContratCM5RPI python3 -m unittest test_regles_v3
```

## 1. Ce que fait chaque fichier

| Fichier | Rôle | Source de la règle |
|---|---|---|
| `liaison_cm5.c` | Silence du CM5 déclaré à 5 s ; reset par impulsion (niveau de la broche G41) ; 3 resets au plus en 10 min, puis pause de 30 min ; règle des 5 s suspendue 90 s après chaque reset ; reprise (alerte DEGRADED_MODE) au retour du heartbeat | Spéc CM5 v0.6 §5 et §10 ; décisions P1, P4 |
| `garde_verin.c` | Un seul vérin en mouvement ; délai avant une marche pleine course (`garde_verin_attente_ms`) ; jamais d'inversion en charge (arrêt, 300 ms, inversion) ; marche continue limitée ; 120 s de marche par vérin et par fenêtre glissante de 20 min | Option B v0.14 chap. 4 ; décisions D6, D2 |
| `securite.c` | Verrou d'arrêt d'urgence (DI1 + commande, effacement = DI1 réarmé ET `CLEAR_EMERGENCY`), HOME obligatoire après tout arrêt, modes NORMAL / VIDAGE / MAINTENANCE avec fin automatique (30 min, 15 min d'inactivité), refus des commandes dans le même ordre que `faux_m5stack.py` | Décisions P2, P3 ; spéc CM5 v0.6 §8 et §9 ; simulateur de référence |

Interface : le temps (ms, 32 bits, débordement géré) et les entrées sont fournis par l'appelant, les ordres sortent dans des structures (`sec_sorties_t` : actionneurs interdits, « Hors service », niveau de reset, DO4, événements à publier). Toutes les fonctions sont appelées depuis la même tâche (pas de verrou).

## 2. Choix de conception à valider (PROPOSITION : non décidés par le CTO)

1. **Délai de 90 s aussi à la mise sous tension du StamPLC** (`SEC_CM5_DEMARRAGE_INITIAL_MS`). La spécification ne parle que du délai après un reset. Sans lui, un CM5 qui démarre en même temps que le StamPLC serait réinitialisé à 5 s.
2. **Silence du CM5 : la référence du chariot est considérée perdue** (HOME exigé ensuite), puisque les actionneurs sont coupés en route. La spécification impose HOME après un arrêt d'urgence ; ici elle est étendue au mode dégradé.
3. **Commande reçue pendant le silence : réponse `ACK_REFUSED_MODE`.** Aucun code d'acquittement n'est prévu pour ce cas ; aucune commande ne devrait arriver sans heartbeat.
4. **Sortie de la maintenance vers NORMAL refusée si le chariot n'est pas référencé** (`ACK_REFUSED_NOT_HOMED`), selon la spécification §8. Le simulateur `faux_m5stack.py` a été aligné le 17/11.
5. **`TEST_ACTUATOR` du chariot en maintenance fait perdre la référence** (le chariot bouge).
6. **Pilote sans arrêt d'urgence (D1)** : `SEC_BOUTON_URGENCE_PRESENT = 0` ignore DI1 (sinon une DI1 non câblée, lue comme « ouverte », verrouillerait l'urgence en permanence) et `SEC_CONTACTEUR_PRESENT = 0` ne commande pas DO4. À passer à 1 avec le montage du bouton, avant tout test avec des personnes extérieures. Avec le bouton présent, l'urgence est supposée active tant que DI1 n'a pas été lue.
7. **Historique des marches des vérins plein (16 segments)** : les deux plus anciens sont fusionnés, ce qui surestime la marche (jamais l'inverse).

## 3. Valeurs supposées (à mesurer)

Délai de démarrage du CM5 90 s ; durée de l'impulsion de reset 500 ms (essai 1 de l'Option B : niveau et durée qui font redémarrer le CM5) ; vidage 30 min ; maintenance 15 min ; course des vérins 300 mm = 60 s par sens (hypothèse D6). Toutes dans `config_securite.h`, chaque valeur étiquetée `[SPEC]`, `[SUPPOSE]` ou `[PROPOSITION]`.

## 4. Décisions du CTO et points tranchés (17/11, après la première version de ce document)

| Point | Décision | Application |
|---|---|---|
| Compactage | **Décidé par le CTO : 1 à 2 cycles par séquence pour commencer.** La spécification CM5 garde « 3 au maximum » : 2 est un plafond plus bas, donc compatible. | `SEC_COMPACTION_CYCLES_MAX = 2` ; 3e cycle refusé `ACK_INVALID` |
| Durée d'une course | **Décidé par le CTO : 60 s par sens.** Les 150 s de « temporisation de sécurité » (Option B v0.14, chap. 4) ne sont pas implémentés. | `SEC_VERIN_MARCHE_MAX_MS = 60000` |
| Entrée en vidage ou en maintenance sans chariot référencé | **Choix délégué, tranché : permise.** Le vidage n'utilise pas le chariot. La maintenance sert justement à diagnostiquer un chariot non référencé (M18 en panne, par exemple) : l'interdire bloquerait tout. Le §8 de la spec (« HOME si homed = false » à la sortie du vidage) suppose d'ailleurs que l'on peut entrer sans référence. Le retour en NORMAL depuis la maintenance exige HOME (§8). | `securite.c` (déjà ainsi), `faux_m5stack.py` aligné, tests |
| `CLEAR_EMERGENCY` du simulateur | **Choix délégué, tranché : le simulateur suit le contrat.** DI1 réarmé ET commande (P3). Le simulateur a maintenant `bouton_present` (faux sur le pilote) et `bouton_ferme`. | `faux_m5stack.py`, `test_regles_v3.py` (12 tests, OK) |

Conséquence physique du 10 % de la fiche (2 min / 18 min), **vérifiée par calcul sur le code** : un cycle complet à 60 s par sens consomme les 120 s du budget. Le 2e cycle du même vérin n'est accepté que **19 min 59 s** après la fin du 1er (`garde_verin_attente_ms`). Une séquence de 2 cycles dure donc environ 22 min. Deux cycles consécutifs ne tiennent dans le budget que si un cycle dure 60 s au plus, c'est-à-dire une course de 150 mm au plus à 5 mm/s. La course de 300 mm est une hypothèse (D6) : à mesurer sur le bac. Si elle est plus courte, `SEC_VERIN_MARCHE_MAX_MS` suffit à tout recalculer (le délai se déduit du budget).

Réponses du StamPLC à `START_COMPACTION` (PROPOSITION : le contrat ne prévoit pas de code pour ces deux cas) : `ACK_BUSY` quand le vérin doit refroidir (temporaire, le CM5 peut réessayer plus tard), `ACK_INVALID` quand le plafond de la séquence est atteint (définitif). Ordre des refus : urgence, hors service, mode, non référencé, cycle en cours, plafond, budget.

Fin de séquence de compactage (PROPOSITION, **à valider avec la CEO**) : ni la spécification v0.6 ni `contrat.proto` ne disent quand une séquence se termine (vérifié). Le module remet les compteurs à zéro à la sortie du mode VIDAGE (les bacs sont vidés), à la demande par `securite_fin_sequence_compactage(bac)` et au redémarrage du StamPLC. Sans autre règle, un bac compacté 2 fois ne le sera plus avant le prochain vidage. Si la CEO veut qu'une séquence se termine aussi quand le niveau redescend (par exemple sous 70 %), il faut appeler `securite_fin_sequence_compactage` depuis le module de niveau.

À corriger dans la prochaine version de la spécification CM5 (v0.7, Word et PDF compris, car la v0.6 est déjà chez la CEO) : ligne 14 du tableau §1 (« chariot non référencé » à retirer pour le passage en vidage ou maintenance) ; règle 6 (plafond du pilote, refus `ACK_BUSY` / `ACK_INVALID`, délai d'environ 20 min entre deux cycles du même bac).

### Ventilateur (DO1) et température (décisions du CTO, 17/11)

- Le ventilateur 24 V (DO1, reçu le 17/11) n'obéit qu'au StamPLC, d'après le BME680 de la zone technique. Le CM5 ne peut pas le commander : `SET_OUTPUT` avec `OUTPUT_FAN` est refusé `ACK_INVALID`, même en maintenance (vérifié : `contrat.proto`, `securite.c`, `faux_m5stack.py`, tests `test_securite.c` et `test_regles_v3.py`). `SET_OUTPUT` ne sert plus qu'à DO2 (éclairage).
- Le StamPLC continue de remonter température et humidité au CM5 (`SensorReport`, `valid_mask`) : supervision et télémétrie seulement. **Le CM5 ne met jamais la borne hors service, et n'arrête aucun dépôt, à cause de la température** : une surchauffe que le ventilateur ne corrige pas ne déclenche aucune action du CM5. Vérifié : la spécification v0.6 ne lie la température à aucune décision (seul le silence du StamPLC met le CM5 en « hors service »), et aucun code d'alerte de température n'existe dans `contrat.proto`.
- Décidé par le CTO : si le BME680 ne répond plus, le ventilateur reste à l'arrêt (la température n'est alors plus surveillée : le défaut du capteur doit rester visible, `valid_mask` ou alerte, à définir).
- Non défini : seuils de marche et d'arrêt (hystérésis) et pilotage de DO1 par le firmware (non codé).
- À reporter dans la spec CM5 v0.7 : ligne `SET_OUTPUT` du tableau des commandes (DO2 seul, DO1 refusé) ; nouvelle règle : le CM5 relaie la température et l'humidité au cloud, ne commande pas le ventilateur et ne met pas la borne hors service pour cause de température.

## 5. Pas encore fait (intégration)

- `logique.c` ne l'appelle pas encore : il faut brancher `securite_evaluer()` au récepteur de commandes, `securite_pas()` à une tâche de 100 ms, et appliquer `actionneurs_interdits`, `reset_cm5_actif` (G41), `hors_service` (LCD du StamPLC), `coupure_puissance` (DO4).
- Pilotes matériels : sortie G41 (NPN, repos = haute impédance), lecture de DI1 par l'AW9523B (traduire la polarité des DI en `di1_circuit_ferme`), relais des vérins via le PCF8574 (P4 à P7, octet complet à chaque écriture, P4 à P7 = 0 juste après le démarrage), LCD.
- Module de compactage (séquence des cycles, pilotage des relais) absent : seuls le plafond de cycles et l'attente du budget sont codés. Perte de l'historique des vérins au redémarrage du StamPLC : après une coupure, le budget thermique repart de zéro (à traiter à l'intégration, par exemple 20 min d'attente après chaque démarrage si la cause est un reset en marche).
- `securite_evaluer` accepte un `START_COMPACTION` avant que le cycle ne soit réellement lancé : `logique.c` doit lancer le cycle dans la foulée (même tâche) pour que le BUSY joue.
- Génération du heartbeat du StamPLC avec `securite_cm5_reset_count()` et le mode.
- `tri.c` reçoit toujours `urgence` en paramètre : à alimenter avec `urgence_active` / `actionneurs_interdits`.
- Jamais compilé avec ESP-IDF (comme les autres composants) ; seulement `gcc` sur PC.
