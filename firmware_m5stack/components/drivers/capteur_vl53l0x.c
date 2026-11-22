#include "capteur_niveau.h"

/* V1 : VL53L0X via PCA9548A (dette technique TD-01). Pas implemente : rend toujours "pas de mesure",
 * ce qui met le niveau en invalide plutot que de donner un faux pourcentage. */
static bool vl53l0x_mesurer(int indice, uint32_t *echo_us) { (void)indice; (void)echo_us; return false; }
const capteur_niveau_ops_t capteur_vl53l0x_ops = { .mesurer_echo_us = vl53l0x_mesurer };
