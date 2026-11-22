#include "garde_verin.h"

static uint32_t ecoule(uint32_t now, uint32_t t) { return now - t; }

void garde_verin_init(garde_verin_t *g) { *g = (garde_verin_t){0}; }

/* Ajoute la marche qui vient de finir a l'historique. */
static void enregistrer(garde_verin_unite_t *u, uint32_t now)
{
    uint32_t duree = ecoule(now, u->t_debut_ms);
    if (duree == 0) return;
    if (u->nb_seg == GARDE_HISTORIQUE) {
        /* Historique plein : on fusionne les deux plus anciens segments (fin = la plus recente, duree = somme).
         * Le segment fusionne reste compte plus longtemps dans la fenetre : on ne sous-estime jamais la marche. */
        u->seg[1].duree_ms += u->seg[0].duree_ms;
        for (uint8_t i = 1; i < GARDE_HISTORIQUE; i++) u->seg[i - 1] = u->seg[i];
        u->nb_seg--;
    }
    u->seg[u->nb_seg].fin_ms = now;
    u->seg[u->nb_seg].duree_ms = duree;
    u->nb_seg++;
}

static void arreter(garde_verin_unite_t *u, uint32_t now)
{
    if (u->sens == VERIN_ARRET) return;
    enregistrer(u, now);
    u->dernier_sens = u->sens;
    u->sens = VERIN_ARRET;
    u->t_arret_ms = now;
    u->a_deja_marche = true;
}

/* Marche des segments finis, comptee dans la fenetre (un segment a cheval n'est compte que pour sa part recente) + marche en cours. */
uint32_t garde_verin_marche_fenetre_ms(const garde_verin_t *g, int verin, uint32_t now)
{
    if (verin < 0 || verin >= GARDE_NB_VERINS) return 0;
    const garde_verin_unite_t *u = &g->v[verin];
    uint32_t total = 0;
    for (uint8_t i = 0; i < u->nb_seg; i++) {
        uint32_t age_fin = ecoule(now, u->seg[i].fin_ms);
        if (age_fin >= SEC_VERIN_FENETRE_MS) continue;
        uint32_t reste = SEC_VERIN_FENETRE_MS - age_fin;           /* partie du segment encore dans la fenetre */
        total += (u->seg[i].duree_ms < reste) ? u->seg[i].duree_ms : reste;
    }
    if (u->sens != VERIN_ARRET) {
        uint32_t d = ecoule(now, u->t_debut_ms);
        total += (d < SEC_VERIN_FENETRE_MS) ? d : SEC_VERIN_FENETRE_MS;
    }
    return total;
}

garde_resultat_t garde_verin_demander(garde_verin_t *g, int verin, verin_sens_t sens, bool interdit, uint32_t now)
{
    if (verin < 0 || verin >= GARDE_NB_VERINS) return GARDE_REFUSE_PARAMETRE;
    if (sens != VERIN_ARRET && sens != VERIN_SENS_1 && sens != VERIN_SENS_2) return GARDE_REFUSE_PARAMETRE;
    garde_verin_unite_t *u = &g->v[verin];

    if (sens == VERIN_ARRET) { arreter(u, now); return GARDE_OK; }
    if (interdit) return GARDE_REFUSE_INTERDIT;
    if (u->sens == sens) return GARDE_OK;                           /* deja dans ce sens : rien a faire */
    if (u->sens != VERIN_ARRET) return GARDE_REFUSE_INVERSION;      /* inversion en marche : arreter d'abord */
    if (g->v[1 - verin].sens != VERIN_ARRET) return GARDE_REFUSE_AUTRE_VERIN;
    if (u->a_deja_marche && u->dernier_sens != sens && ecoule(now, u->t_arret_ms) < SEC_VERIN_PAUSE_INVERSION_MS)
        return GARDE_REFUSE_INVERSION;                              /* pas encore 300 ms depuis l'arret */
    if (garde_verin_marche_fenetre_ms(g, verin, now) >= SEC_VERIN_MARCHE_FENETRE_MS) return GARDE_REFUSE_BUDGET;

    u->sens = sens;
    u->t_debut_ms = now;
    return GARDE_OK;
}

uint32_t garde_verin_attente_ms(const garde_verin_t *g, int verin, uint32_t duree_ms, uint32_t now)
{
    if (verin < 0 || verin >= GARDE_NB_VERINS) return GARDE_ATTENTE_JAMAIS;
    if (g->v[verin].sens != VERIN_ARRET) return GARDE_ATTENTE_JAMAIS;
    if (duree_ms > SEC_VERIN_MARCHE_FENETRE_MS) return GARDE_ATTENTE_JAMAIS;
    /* Verin arrete : la marche comptee dans la fenetre ne fait que baisser avec le temps. Recherche du plus petit delai. */
    uint32_t bas = 0, haut = SEC_VERIN_FENETRE_MS;                  /* a haut, toute marche passee est sortie de la fenetre */
    if (garde_verin_marche_fenetre_ms(g, verin, now) + duree_ms <= SEC_VERIN_MARCHE_FENETRE_MS) return 0;
    while (haut - bas > 1) {
        uint32_t m = bas + (haut - bas) / 2;
        if (garde_verin_marche_fenetre_ms(g, verin, now + m) + duree_ms <= SEC_VERIN_MARCHE_FENETRE_MS) haut = m; else bas = m;
    }
    return haut;
}

garde_verin_sortie_t garde_verin_pas(garde_verin_t *g, uint32_t now, bool interdit)
{
    garde_verin_sortie_t s = {0};
    for (int i = 0; i < GARDE_NB_VERINS; i++) {
        garde_verin_unite_t *u = &g->v[i];
        if (u->sens != VERIN_ARRET) {
            if (interdit) { arreter(u, now); s.arret_force[i] = GARDE_CAUSE_INTERDIT; }
            else if (ecoule(now, u->t_debut_ms) >= SEC_VERIN_MARCHE_MAX_MS) { arreter(u, now); s.arret_force[i] = GARDE_CAUSE_MARCHE_MAX; }
            else if (garde_verin_marche_fenetre_ms(g, i, now) >= SEC_VERIN_MARCHE_FENETRE_MS) { arreter(u, now); s.arret_force[i] = GARDE_CAUSE_BUDGET; }
        }
        s.sens[i] = u->sens;
    }
    return s;
}
