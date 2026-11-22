#pragma once
#include "esp_err.h"
esp_err_t transport_init(void);
/* TODO : envoyer/recevoir une Envelope (COBS + nanopb), ACK pour Command et Alert uniquement. */
