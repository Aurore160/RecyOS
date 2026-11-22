#include "capteur_niveau.h"
#include "config_jsn.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

/* Non compile dans l'environnement de redaction (pas d'ESP-IDF) : a compiler et essayer au banc. */
static const int trig_pins[CAPTEUR_NIVEAU_NB_BACS] = { JSN_PIN_TRIG_PLASTIQUE, JSN_PIN_TRIG_PAPIER, JSN_PIN_TRIG_NON_RECYCLABLE };
static const int echo_pins[CAPTEUR_NIVEAU_NB_BACS] = { JSN_PIN_ECHO_PLASTIQUE, JSN_PIN_ECHO_PAPIER, JSN_PIN_ECHO_NON_RECYCLABLE };
static bool configure[CAPTEUR_NIVEAU_NB_BACS];

static bool prepare(int i)
{
    if (configure[i]) return true;
    if (trig_pins[i] < 0 || echo_pins[i] < 0) return false;
    gpio_config_t t = { .pin_bit_mask = 1ULL << trig_pins[i], .mode = GPIO_MODE_OUTPUT };
    gpio_config_t e = { .pin_bit_mask = 1ULL << echo_pins[i], .mode = GPIO_MODE_INPUT };
    if (gpio_config(&t) != ESP_OK || gpio_config(&e) != ESP_OK) return false;
    gpio_set_level(trig_pins[i], 0);
    configure[i] = true;
    return true;
}

static bool jsn_mesurer(int i, uint32_t *echo_us)
{
    if (i < 0 || i >= CAPTEUR_NIVEAU_NB_BACS || echo_us == NULL || !prepare(i)) return false;
    gpio_set_level(trig_pins[i], 1);
    esp_rom_delay_us(JSN_TRIG_LARGEUR_US);
    gpio_set_level(trig_pins[i], 0);
    int64_t t0 = esp_timer_get_time();
    while (gpio_get_level(echo_pins[i]) == 0) {
        if (esp_timer_get_time() - t0 > JSN_ECHO_DEBUT_TIMEOUT_US) return false;
    }
    int64_t debut = esp_timer_get_time();
    while (gpio_get_level(echo_pins[i]) == 1) {
        if (esp_timer_get_time() - debut > JSN_ECHO_FIN_TIMEOUT_US) return false;
    }
    *echo_us = (uint32_t)(esp_timer_get_time() - debut);
    return true;
}

const capteur_niveau_ops_t capteur_jsn_ops = { .mesurer_echo_us = jsn_mesurer };
