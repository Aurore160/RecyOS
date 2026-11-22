#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "config_securite.h"

/* Garde-fous des 2 verins (Option B v0.14, chapitre 4 ; decision D6). Module sans materiel.
 * Les verins n'ont aucun signal de position : la duree de marche est la seule protection.
 * Regles : un seul verin en mouvement ; jamais d'inversion en charge (arret, attente 300 ms, inversion) ;
 * duree continue limitee ; 120 s de marche par fenetre glissante de 20 min et par verin.
 * L'appelant traduit sens[] en etats des relais (voir le chapitre 4 : l'etat « arret » = les deux ponts au 0 V). */

#define GARDE_NB_VERINS 2
#define GARDE_HISTORIQUE 16     /* nombre de segments de marche memorises par verin */

typedef enum { VERIN_ARRET = 0, VERIN_SENS_1 = 1, VERIN_SENS_2 = 2 } verin_sens_t;

typedef enum {
    GARDE_OK = 0,
    GARDE_REFUSE_PARAMETRE,         /* numero de verin ou sens invalide */
    GARDE_REFUSE_INTERDIT,          /* actionneurs interdits (urgence, CM5 hors service) */
    GARDE_REFUSE_AUTRE_VERIN,       /* l'autre verin est en mouvement */
    GARDE_REFUSE_INVERSION,         /* inversion demandee en marche ou avant la pause de 300 ms */
    GARDE_REFUSE_BUDGET             /* 120 s de marche deja utilises dans la fenetre de 20 min */
} garde_resultat_t;

typedef enum { GARDE_CAUSE_AUCUNE = 0, GARDE_CAUSE_MARCHE_MAX, GARDE_CAUSE_BUDGET, GARDE_CAUSE_INTERDIT } garde_cause_t;

typedef struct { uint32_t fin_ms; uint32_t duree_ms; } garde_segment_t;

typedef struct {
    verin_sens_t sens;                          /* sens en cours (VERIN_ARRET si arrete) */
    uint32_t t_debut_ms;                        /* debut de la marche en cours */
    uint32_t t_arret_ms;                        /* instant du dernier arret (pour la pause d'inversion) */
    verin_sens_t dernier_sens;                  /* sens avant le dernier arret */
    bool a_deja_marche;
    garde_segment_t seg[GARDE_HISTORIQUE];
    uint8_t nb_seg;
} garde_verin_unite_t;

typedef struct {
    garde_verin_unite_t v[GARDE_NB_VERINS];
} garde_verin_t;

typedef struct {
    verin_sens_t sens[GARDE_NB_VERINS];         /* ordre a appliquer aux relais */
    garde_cause_t arret_force[GARDE_NB_VERINS]; /* une fois : le garde-fou vient d'arreter ce verin */
} garde_verin_sortie_t;

void garde_verin_init(garde_verin_t *g);
/* Demande de marche (sens 1 ou 2) ou d'arret (VERIN_ARRET, toujours accepte). */
garde_resultat_t garde_verin_demander(garde_verin_t *g, int verin, verin_sens_t sens, bool interdit, uint32_t now_ms);
/* A appeler a chaque cycle : applique les limites, retourne l'etat des verins. interdit = urgence ou CM5 hors service. */
garde_verin_sortie_t garde_verin_pas(garde_verin_t *g, uint32_t now_ms, bool interdit);
/* Marche utilisee dans la fenetre glissante (ms), pour les essais et le diagnostic. */
/* Delai avant qu'une marche de duree_ms (a pleine course, sans coupure par le budget) soit admissible pour ce verin arrete.
 * 0 = tout de suite ; GARDE_ATTENTE_JAMAIS = impossible (verin en marche, indice invalide ou duree superieure au budget). */
#define GARDE_ATTENTE_JAMAIS 0xFFFFFFFFUL
uint32_t garde_verin_attente_ms(const garde_verin_t *g, int verin, uint32_t duree_ms, uint32_t now_ms);
uint32_t garde_verin_marche_fenetre_ms(const garde_verin_t *g, int verin, uint32_t now_ms);
