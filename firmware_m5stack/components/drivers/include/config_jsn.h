#pragma once
/* Broches du JSN-SR04T (un TRIG et un ECHO par bac). A AFFECTER d'apres le mapping GPIO valide.
 * Valeur -1 = non cable : le pilote rend alors "pas de mesure" au lieu de planter.
 * Rappel : ECHO sort en 5 V, il doit passer par le diviseur prevu avant le StamPLC (3,3 V). */
#define JSN_PIN_TRIG_PLASTIQUE        (-1)
#define JSN_PIN_TRIG_PAPIER           (-1)
#define JSN_PIN_TRIG_NON_RECYCLABLE   (-1)
#define JSN_PIN_ECHO_PLASTIQUE        (-1)
#define JSN_PIN_ECHO_PAPIER           (-1)
#define JSN_PIN_ECHO_NON_RECYCLABLE   (-1)

#define JSN_TRIG_LARGEUR_US           12      /* impulsion de declenchement (au moins 10 us) */
#define JSN_ECHO_DEBUT_TIMEOUT_US     20000   /* attente du debut de l'echo */
#define JSN_ECHO_FIN_TIMEOUT_US       30000   /* duree maximale de l'echo (environ 5 m) */
