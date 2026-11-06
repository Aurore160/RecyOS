#pragma once
/*
 * Parametres du tri (chariot, servo MG996R). TOUTES LES VALEURS SONT PROVISOIRES :
 * a valider ou changer par le CTO, puis a mesurer au banc.
 * Hypothese de la transmission : Nema 23 1,8 deg (200 pas/tour), micro-pas 1/16,
 * poulie GT2 20 dents (40 mm par tour) => 3200 / 40 = 80 pas/mm.
 */
#define TRI_PAS_PAR_MM              80      /* pas/mm, depend de la transmission reelle (P6) */
#define TRI_COURSE_UTILE_MM         920     /* course utile du chariot, rail de 1 050 mm */
#define TRI_VITESSE_MAX_MM_S        100     /* vitesse de croisiere, 8 000 pas/s a 80 pas/mm */
#define TRI_ACCEL_MM_S2             200
#define TRI_DELAI_DEPLACEMENT_MS    15000   /* au-dela : defaut, abandon de la sequence */

/* Positions du chariot, mesurees depuis le point de homing (DI6), en mm. */
#define TRI_X_OUVERTURE_MM          0       /* sous l'ouverture de depot = point de homing */
#define TRI_X_BAC_PLASTIQUE_MM      150
#define TRI_X_BAC_PAPIER_MM         460
#define TRI_X_BAC_NON_RECYCLABLE_MM 770

/* Servo MG996R : ouvre le fond de la trappe. */
#define TRI_SERVO_ANGLE_FERME_DEG   0
#define TRI_SERVO_ANGLE_OUVERT_DEG  90
#define TRI_SERVO_DUREE_MOUVEMENT_MS 600    /* temps pour atteindre l'angle demande */
#define TRI_DUREE_CHUTE_MS          1500    /* fond ouvert, le temps que le dechet tombe */
