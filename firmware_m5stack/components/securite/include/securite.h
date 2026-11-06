#pragma once
#include <stdbool.h>
void securite_init(void);
bool securite_urgence_active(void);
/* TODO : arret d'urgence (DO4), etat sur, perte du heartbeat (5 s). */
