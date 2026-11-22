#pragma once
#include <stdbool.h>

/* Interface du capteur de presence (detection d'un citoyen qui s'approche). Un pilote par techno.
 * PoC : RCWL-0516 (radar, sortie 3,3 V sur P0 du PCF8574, adresse I2C 0x20, ecrire 1 avant lecture).
 * V1 : cellule photoelectrique M18 (dette technique TD-02, sur une DI 24 V) : meme interface.
 * Le choix se fait dans config_presence.h (PRESENCE_CAPTEUR_TYPE), sans toucher a la logique. */

typedef struct {
    /* Etat brut instantane. Rend false si la lecture a echoue (bus I2C en erreur, capteur absent,
     * pilote non implemente) : la presence est alors marquee invalide, jamais "personne". */
    bool (*lire_brut)(bool *present);
} capteur_presence_ops_t;

/* Pilote choisi par la configuration. Jamais NULL. */
const capteur_presence_ops_t *capteur_presence_ops(void);
