#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "capteur_niveau.h"
#include "config_niveau.h"

/* Calcul du niveau de remplissage (decision T6 du 09/11). Module sans materiel : testable sur PC,
 * le pilote est injecte (capteur_niveau_ops_t). Un bac = un cycle de mesure.
 * Regles : 3 mesures, mediane, mediane glissante sur 5 cycles, compensation en temperature,
 * mesure invalide (jamais "0 %") si pas d'echo, hors plage ou bac occupe par un tri / compactage. */

typedef struct {
    bool valide;
    int32_t distance_mm;        /* mediane glissante, 0 si invalide */
    float pct;                  /* 0 a 100, 0 si invalide */
    bool vide;                  /* pct <= NIVEAU_SEUIL_VIDE_PCT (seulement si valide) */
} niveau_resultat_t;

typedef struct {
    int32_t hist[NIVEAU_HISTORIQUE];
    int n;                      /* valeurs presentes (<= NIVEAU_HISTORIQUE) */
    int suivant;                /* prochaine case a ecrire */
} niveau_bac_t;

void niveau_init(niveau_bac_t *b);
/* Duree d'echo (aller-retour) -> distance en mm, avec v = 331,3 + 0,606 * T (m/s). */
int32_t niveau_distance_mm(uint32_t echo_us, float temp_c);
float niveau_pct(int indice, int32_t distance_mm);
/* indice : 0 plastique, 1 papier, 2 non recyclable. bloque : tri ou compactage en cours dans ce bac. */
niveau_resultat_t niveau_cycle(niveau_bac_t *b, int indice, const capteur_niveau_ops_t *ops,
                               float temp_c, bool temp_valide, bool bloque);
