#include "presence.h"

void presence_init(presence_t *p) { p->consecutifs = 0; p->actif = false; p->dernier_vu_ms = 0; }

presence_resultat_t presence_pas(presence_t *p, const capteur_presence_ops_t *ops, uint32_t now_ms)
{
    presence_resultat_t r = { false, false };
    bool brut = false;
    if (ops == 0 || ops->lire_brut == 0 || !ops->lire_brut(&brut)) {
        presence_init(p);                       /* capteur muet : invalide, jamais "personne" */
        return r;
    }
    r.valide = true;
    if (brut) {
        p->dernier_vu_ms = now_ms;
        if (p->consecutifs < PRESENCE_CONFIRMATIONS) p->consecutifs++;
        if (p->consecutifs >= PRESENCE_CONFIRMATIONS) p->actif = true;
    } else {
        p->consecutifs = 0;
        if (p->actif && (uint32_t)(now_ms - p->dernier_vu_ms) > PRESENCE_MAINTIEN_MS) p->actif = false;
    }
    r.presente = p->actif;
    return r;
}
