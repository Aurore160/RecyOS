#include "securite.h"
static bool s_urgence = false;
void securite_init(void) { s_urgence = false; }
bool securite_urgence_active(void) { return s_urgence; }
