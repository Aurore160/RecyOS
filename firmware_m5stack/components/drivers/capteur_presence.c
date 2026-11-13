#include "capteur_presence.h"
#include "config_presence.h"

/* PoC : RCWL-0516 lu sur P0 du PCF8574 (0x20). NON IMPLEMENTE : la lecture I2C sera ecrite avec le
 * bus I2C partage (BME680, MPU6050, PCA9685). En attendant elle echoue : la presence reste
 * invalide (valid_mask), jamais un faux "personne". A essayer au banc : faux positifs attendus. */
static bool rcwl_lire(bool *present) { (void)present; return false; }
static const capteur_presence_ops_t rcwl_ops = { .lire_brut = rcwl_lire };

/* V1 : cellule photoelectrique M18 sur une DI 24 V (TD-02). Non implemente. */
static bool m18_lire(bool *present) { (void)present; return false; }
static const capteur_presence_ops_t m18_ops __attribute__((unused)) = { .lire_brut = m18_lire };

const capteur_presence_ops_t *capteur_presence_ops(void)
{
#if PRESENCE_CAPTEUR_TYPE == PRESENCE_TYPE_M18_PHOTO
    return &m18_ops;
#else
    return &rcwl_ops;
#endif
}
