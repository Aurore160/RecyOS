idf_component_register(SRCS "drivers.c" "capteur_niveau.c" "capteur_jsn.c" "capteur_vl53l0x.c" "capteur_presence.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_timer esp_rom)
