#include "capteur_niveau.h"
#include "config_niveau.h"

extern const capteur_niveau_ops_t capteur_jsn_ops;
extern const capteur_niveau_ops_t capteur_vl53l0x_ops;

const capteur_niveau_ops_t *capteur_niveau_ops(void)
{
#if NIVEAU_CAPTEUR_TYPE == NIVEAU_CAPTEUR_VL53L0X
    return &capteur_vl53l0x_ops;
#else
    return &capteur_jsn_ops;
#endif
}
