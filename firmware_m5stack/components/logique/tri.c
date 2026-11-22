#include "tri.h"
#include "config_tri.h"

void tri_init(tri_t *t) { t->etat = TRI_REPOS; t->bac = TRI_BAC_AUCUN; t->reference = false; t->t_debut_ms = 0; }
void tri_set_reference(tri_t *t, bool fait) { t->reference = fait; }

int32_t tri_position_bac_mm(tri_bac_t bac)
{
    switch (bac) {
    case TRI_BAC_PLASTIQUE:      return TRI_X_BAC_PLASTIQUE_MM;
    case TRI_BAC_PAPIER:         return TRI_X_BAC_PAPIER_MM;
    case TRI_BAC_NON_RECYCLABLE: return TRI_X_BAC_NON_RECYCLABLE_MM;
    default:                     return -1;
    }
}

tri_resultat_t tri_demarrer(tri_t *t, tri_bac_t bac, bool urgence, uint32_t now_ms)
{
    if (urgence) return TRI_REFUSE_URGENCE;
    if (tri_position_bac_mm(bac) < 0) return TRI_REFUSE_BAC;
    if (t->etat != TRI_REPOS) return TRI_REFUSE_OCCUPE;
    if (!t->reference) return TRI_REFUSE_NON_REFERENCE;
    t->bac = bac; t->etat = TRI_ALLER_BAC; t->t_debut_ms = now_ms;
    return TRI_OK;
}

static void etape(tri_t *t, tri_etat_t e, uint32_t now_ms) { t->etat = e; t->t_debut_ms = now_ms; }

tri_sortie_t tri_pas(tri_t *t, uint32_t now_ms, bool fini, bool urgence)
{
    tri_sortie_t s = {0};
    uint32_t ecoule = now_ms - t->t_debut_ms;

    if (urgence && t->etat != TRI_REPOS && t->etat != TRI_DEFAUT) { /* le fond reste dans sa position, on coupe le mouvement */
        s.arreter = true; etape(t, TRI_DEFAUT, now_ms); return s;
    }
    switch (t->etat) {
    case TRI_ALLER_BAC:
        s.deplacer = true; s.cible_mm = tri_position_bac_mm(t->bac);
        if (fini) etape(t, TRI_OUVERTURE_FOND, now_ms);
        else if (ecoule > TRI_DELAI_DEPLACEMENT_MS) { s.arreter = true; etape(t, TRI_DEFAUT, now_ms); }
        break;
    case TRI_OUVERTURE_FOND:
        s.servo = true; s.servo_deg = TRI_SERVO_ANGLE_OUVERT_DEG;
        if (ecoule >= TRI_SERVO_DUREE_MOUVEMENT_MS) etape(t, TRI_CHUTE, now_ms);
        break;
    case TRI_CHUTE:
        s.servo = true; s.servo_deg = TRI_SERVO_ANGLE_OUVERT_DEG;
        if (ecoule >= TRI_DUREE_CHUTE_MS) etape(t, TRI_FERMETURE_FOND, now_ms);
        break;
    case TRI_FERMETURE_FOND:
        s.servo = true; s.servo_deg = TRI_SERVO_ANGLE_FERME_DEG;
        if (ecoule >= TRI_SERVO_DUREE_MOUVEMENT_MS) etape(t, TRI_RETOUR, now_ms);
        break;
    case TRI_RETOUR:
        s.deplacer = true; s.cible_mm = TRI_X_OUVERTURE_MM;
        if (fini) { etape(t, TRI_REPOS, now_ms); t->bac = TRI_BAC_AUCUN; }
        else if (ecoule > TRI_DELAI_DEPLACEMENT_MS) { s.arreter = true; etape(t, TRI_DEFAUT, now_ms); }
        break;
    default: break;
    }
    return s;
}
