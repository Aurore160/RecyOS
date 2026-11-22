#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Sequence de tri (decision CTO du 06/11) : chariot sous l'ouverture -> chariot au-dessus
 * du bon bac -> le servo ouvre le fond de la trappe -> le dechet tombe -> le servo referme
 * -> retour sous l'ouverture. Module sans materiel : testable sur PC. */

typedef enum {
    TRI_BAC_AUCUN = 0,          /* valeur 0 refusee */
    TRI_BAC_PLASTIQUE = 1,      /* memes valeurs que l'enum Bac du contrat */
    TRI_BAC_PAPIER = 2,
    TRI_BAC_NON_RECYCLABLE = 3
} tri_bac_t;

typedef enum {
    TRI_REPOS = 0,
    TRI_ALLER_BAC,
    TRI_OUVERTURE_FOND,
    TRI_CHUTE,
    TRI_FERMETURE_FOND,
    TRI_RETOUR,
    TRI_DEFAUT                  /* urgence ou delai depasse : le CM5 doit relancer */
} tri_etat_t;

typedef enum { TRI_OK = 0, TRI_REFUSE_BAC, TRI_REFUSE_OCCUPE, TRI_REFUSE_NON_REFERENCE, TRI_REFUSE_URGENCE } tri_resultat_t;

typedef struct {
    tri_etat_t etat;
    tri_bac_t bac;
    bool reference;             /* homing fait */
    uint32_t t_debut_ms;        /* debut de l'etape en cours */
} tri_t;

typedef struct {
    bool deplacer;              /* ordre au driver : aller a cible_mm */
    int32_t cible_mm;
    bool arreter;               /* arret immediat du chariot */
    bool servo;                 /* ordre au servo : aller a servo_deg */
    int servo_deg;
} tri_sortie_t;

void tri_init(tri_t *t);
void tri_set_reference(tri_t *t, bool fait);
int32_t tri_position_bac_mm(tri_bac_t bac);   /* -1 si invalide */
tri_resultat_t tri_demarrer(tri_t *t, tri_bac_t bac, bool urgence, uint32_t now_ms);
/* A appeler a chaque cycle. deplacement_termine : le chariot est arrive a la derniere cible. */
tri_sortie_t tri_pas(tri_t *t, uint32_t now_ms, bool deplacement_termine, bool urgence);
