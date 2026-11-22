#include "transport.h"
esp_err_t transport_init(void) { return ESP_OK; /* TODO : UART vers le CM5 (GPIO.EXT broches 11 = G9 et 14 = G11 ; G7/G8 = LCD, voir Spécification StamPLC v3.3), 921 600 bauds ; utiliser trame.h */ }
