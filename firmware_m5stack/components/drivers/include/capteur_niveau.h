#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Interface commune des capteurs de niveau (decision T6 du 09/11) : un pilote par techno.
 * PoC : JSN-SR04T (ultrason). V1 : VL53L0X (ToF, via PCA9548A) - coquille vide pour l'instant.
 * Le choix se fait dans config_niveau.h (NIVEAU_CAPTEUR_TYPE), sans toucher a la logique. */

#define CAPTEUR_NIVEAU_NB_BACS 3        /* indice 0 = plastique, 1 = papier, 2 = non recyclable */

typedef struct {
    /* Une mesure brute du bac `indice` : duree de l'echo en microsecondes (aller-retour du son).
     * Rend false si pas d'echo, timeout, capteur non cable ou non implemente. Mesure bloquante,
     * a appeler depuis une tache dediee (30 ms au plus par mesure). */
    bool (*mesurer_echo_us)(int indice, uint32_t *echo_us);
} capteur_niveau_ops_t;

/* Pilote choisi par la configuration. Jamais NULL. */
const capteur_niveau_ops_t *capteur_niveau_ops(void);
