#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "config_securite.h"

/* Surveillance du CM5 par le StamPLC (spec CM5 v0.6 section 5, decisions P1 et P4).
 * Module sans materiel, testable sur PC. Le temps est fourni par l'appelant (ms, 32 bits, debordement gere). */

typedef enum {
    CM5_DEMARRAGE = 0,   /* delai de demarrage : la regle des 5 s est suspendue */
    CM5_OK,              /* heartbeats recus */
    CM5_RESET,           /* impulsion de reset en cours */
    CM5_PAUSE            /* 3 resets utilises en 10 min : pause avant de reessayer */
} cm5_etat_t;

typedef struct {
    cm5_etat_t etat;
    uint32_t t_etat_ms;                         /* debut de l'etat courant */
    uint32_t t_dernier_hb_ms;
    bool silence_declare;                       /* vrai de la declaration du silence jusqu'au prochain heartbeat */
    uint32_t t_resets_ms[SEC_CM5_RESETS_MAX];   /* instants des derniers resets */
    uint8_t nb_resets_fenetre;                  /* nombre d'entrees valides de t_resets_ms */
    uint32_t cm5_reset_count;                   /* total depuis le demarrage (champ du heartbeat) */
} liaison_cm5_t;

typedef struct {
    bool hors_service;          /* niveau : afficher « Hors service » et interdire les actionneurs */
    bool reset_actif;           /* niveau a appliquer sur la broche de reset (G41 -> NPN). Repos = false (haute impedance). */
    bool evt_silence;           /* une fois : silence de 5 s declare */
    bool evt_reset;             /* une fois : impulsion de reset commencee */
    bool evt_pause;             /* une fois : resets epuises, pause commencee */
    bool evt_reprise;           /* une fois : heartbeat retrouve apres un silence (alerte DEGRADED_MODE) */
} liaison_cm5_sortie_t;

void liaison_cm5_init(liaison_cm5_t *l, uint32_t now_ms);
/* A appeler a chaque cycle. heartbeat_recu : au moins un heartbeat CM5 valide depuis l'appel precedent. */
liaison_cm5_sortie_t liaison_cm5_pas(liaison_cm5_t *l, uint32_t now_ms, bool heartbeat_recu);
