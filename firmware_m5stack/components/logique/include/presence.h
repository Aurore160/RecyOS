#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "capteur_presence.h"

/* Presence d'un citoyen devant la borne, filtree. Module sans materiel : testable sur PC, le pilote
 * est injecte. Le resultat alimente SensorReport.presence_detected et le bit VALID_PRESENCE.
 * Le StamPLC ne pilote pas l'ecran : le CM5 se reveille a partir de presence_detected.
 * Regles (VALEURS PROVISOIRES, a regler au banc car le RCWL-0516 donne des faux positifs) :
 *  - activation apres PRESENCE_CONFIRMATIONS lectures brutes consecutives a vrai ;
 *  - maintien PRESENCE_MAINTIEN_MS apres la derniere lecture a vrai (evite de couper l'ecran) ;
 *  - lecture impossible : presence invalide ET fausse, historique remis a zero. */
#define PRESENCE_PERIODE_MS     100
#define PRESENCE_CONFIRMATIONS  3
#define PRESENCE_MAINTIEN_MS    5000

typedef struct {
    int consecutifs;
    bool actif;
    uint32_t dernier_vu_ms;
} presence_t;

typedef struct {
    bool valide;
    bool presente;
} presence_resultat_t;

void presence_init(presence_t *p);
/* A appeler toutes les PRESENCE_PERIODE_MS. now_ms : horloge monotone du StamPLC. */
presence_resultat_t presence_pas(presence_t *p, const capteur_presence_ops_t *ops, uint32_t now_ms);
