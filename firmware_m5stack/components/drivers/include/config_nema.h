#pragma once
/* Reglages du Nema 23 de la trappe (decision P5 du 08/11/2026).
   Un seul firmware pour le montage nominal (MKS SERVO57A) et le secours (DM542).
   Signaux : STEP sur G1, DIR sur G4, EN sur G5 (3,3 V). G2 libre (alarme du MKS
   a l'etude, selon la fiche du moteur).
   Valeurs provisoires : a confirmer dans les fiches des drivers et au banc. */

/* Niveau logique qui ACTIVE le driver sur la broche EN : 0 = actif bas, 1 = actif haut. */
#define NEMA_EN_NIVEAU_ACTIF 0

/* Frequence maximale du signal STEP, en Hz (environ 8 kHz necessaires a 100 mm/s et 80 pas/mm). */
#define NEMA_STEP_MAX_HZ 20000

/* Largeur minimale d'une impulsion STEP et delai DIR -> STEP, en microsecondes. */
#define NEMA_STEP_LARGEUR_US 5
#define NEMA_DIR_DELAI_US 10

/* Origine (homing, M18 sur DI6) : cote moteur, contre la paroi qui separe la zone
   technique de la zone des bacs, donc du cote de l'ouverture de depot (position 0).
   Decide par le CTO le 08/11/2026. Les positions des bacs sont des distances a partir de cette origine. */
#define NEMA_ORIGINE_COTE_MOTEUR 1
