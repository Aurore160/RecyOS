#include "liaison_cm5.h"

static uint32_t ecoule(uint32_t now, uint32_t t) { return now - t; }   /* correct au debordement de 32 bits */

void liaison_cm5_init(liaison_cm5_t *l, uint32_t now_ms)
{
    *l = (liaison_cm5_t){0};
    l->etat = CM5_DEMARRAGE;
    l->t_etat_ms = now_ms;
    l->t_dernier_hb_ms = now_ms;
}

/* Nombre de resets encore dans la fenetre de 10 min ; retire les plus anciens. */
static uint8_t resets_dans_fenetre(liaison_cm5_t *l, uint32_t now)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < l->nb_resets_fenetre; i++)
        if (ecoule(now, l->t_resets_ms[i]) < SEC_CM5_FENETRE_RESETS_MS) l->t_resets_ms[n++] = l->t_resets_ms[i];
    l->nb_resets_fenetre = n;
    return n;
}

static void aller(liaison_cm5_t *l, cm5_etat_t e, uint32_t now) { l->etat = e; l->t_etat_ms = now; }

/* Silence constate (ou delai de demarrage ecoule sans heartbeat) : reset si le budget le permet, sinon pause. */
static void traiter_silence(liaison_cm5_t *l, uint32_t now, liaison_cm5_sortie_t *s)
{
    if (!l->silence_declare) { l->silence_declare = true; s->evt_silence = true; }
    if (resets_dans_fenetre(l, now) < SEC_CM5_RESETS_MAX) {
        l->t_resets_ms[l->nb_resets_fenetre++] = now;
        l->cm5_reset_count++;
        s->evt_reset = true;
        aller(l, CM5_RESET, now);
    } else {
        s->evt_pause = true;
        aller(l, CM5_PAUSE, now);
    }
}

liaison_cm5_sortie_t liaison_cm5_pas(liaison_cm5_t *l, uint32_t now, bool hb)
{
    liaison_cm5_sortie_t s = {0};

    if (hb) l->t_dernier_hb_ms = now;

    switch (l->etat) {
    case CM5_DEMARRAGE: {
        uint32_t delai = (l->cm5_reset_count == 0 && !l->silence_declare) ? SEC_CM5_DEMARRAGE_INITIAL_MS : SEC_CM5_DEMARRAGE_MS;
        if (hb) { aller(l, CM5_OK, now); if (l->silence_declare) { l->silence_declare = false; s.evt_reprise = true; } }
        else if (ecoule(now, l->t_etat_ms) >= delai) traiter_silence(l, now, &s);
        break;
    }
    case CM5_OK:
        if (!hb && ecoule(now, l->t_dernier_hb_ms) >= SEC_CM5_SILENCE_MS) traiter_silence(l, now, &s);
        break;
    case CM5_RESET:
        if (ecoule(now, l->t_etat_ms) >= SEC_CM5_RESET_IMPULSION_MS) aller(l, CM5_DEMARRAGE, now);
        break;
    case CM5_PAUSE:
        if (hb) { aller(l, CM5_OK, now); l->silence_declare = false; s.evt_reprise = true; }
        else if (ecoule(now, l->t_etat_ms) >= SEC_CM5_PAUSE_MS) traiter_silence(l, now, &s);
        break;
    }

    s.hors_service = l->silence_declare;
    s.reset_actif = (l->etat == CM5_RESET);
    return s;
}
