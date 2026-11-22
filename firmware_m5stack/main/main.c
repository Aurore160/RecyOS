#include "esp_log.h"
#include "transport.h"
#include "drivers.h"
#include "securite.h"
#include "logique.h"

static const char *TAG = "recystation";

void app_main(void)
{
    ESP_LOGI(TAG, "RecyStation StamPLC : demarrage");

    securite_init();   /* en premier : etat sur par defaut */
    drivers_init();
    transport_init();
    logique_init();

    /* TODO : creer les taches (reception UART, rapport capteurs 500 ms, heartbeat 2 s). */
}
