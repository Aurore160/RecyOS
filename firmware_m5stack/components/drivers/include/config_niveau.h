#pragma once
/*
 * Capteurs de niveau (decision T6 du 09/11). TOUTES LES VALEURS SONT PROVISOIRES :
 * les distances a vide et a plein sont a mesurer sur les vrais bacs une fois la barre de capteurs posee.
 */
#define NIVEAU_CAPTEUR_JSN_SR04T    0       /* PoC */
#define NIVEAU_CAPTEUR_VL53L0X      1       /* V1 (pas encore code) */
#define NIVEAU_CAPTEUR_TYPE         NIVEAU_CAPTEUR_JSN_SR04T

/* Mesure */
#define NIVEAU_ECHANTILLONS         3       /* mesures par bac et par cycle (on garde la mediane) */
#define NIVEAU_ECHANTILLONS_MIN     2       /* mesures valides minimales pour accepter le cycle */
#define NIVEAU_HISTORIQUE           5       /* mediane glissante sur les derniers cycles valides */
#define NIVEAU_TEMP_DEFAUT_C        25.0f   /* si la temperature du BME680 est indisponible */

/* Plage de distance acceptee (JSN-SR04T : zone aveugle d'environ 20 cm, portee utile ~ 5 m) */
#define NIVEAU_DIST_MIN_MM          200
#define NIVEAU_DIST_MAX_MM          5000

/* Calibration par bac (indice 0 = plastique, 1 = papier, 2 = non recyclable), en mm depuis le capteur.
 * A plein, la surface doit rester au-dela de la zone aveugle (NIVEAU_DIST_MIN_MM). */
#define NIVEAU_DIST_VIDE_MM         { 900, 900, 900 }
#define NIVEAU_DIST_PLEIN_MM        { 250, 250, 250 }

/* Bac considere comme vide (decision du 08/11) */
#define NIVEAU_SEUIL_VIDE_PCT       5.0f
