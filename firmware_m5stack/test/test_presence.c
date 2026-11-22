#include <stdio.h>
#include <assert.h>
#include "presence.h"

static bool etat, ok;
static bool fake(bool *p) { if (!ok) return false; *p = etat; return true; }
static const capteur_presence_ops_t ops = { .lire_brut = fake };

int main(void)
{
    presence_t p; presence_init(&p); ok = true; etat = false;
    uint32_t t = 0;
    presence_resultat_t r;

    /* repos */
    r = presence_pas(&p, &ops, t); assert(r.valide && !r.presente);

    /* un pic isole (faux positif) : pas de presence */
    etat = true; t += 100; r = presence_pas(&p, &ops, t); assert(r.valide && !r.presente);
    etat = false; t += 100; r = presence_pas(&p, &ops, t); assert(!r.presente);

    /* 3 lectures consecutives : presence */
    etat = true;
    for (int i = 0; i < PRESENCE_CONFIRMATIONS; i++) { t += 100; r = presence_pas(&p, &ops, t); }
    assert(r.valide && r.presente);

    /* le signal tombe : la presence est maintenue PRESENCE_MAINTIEN_MS puis tombe */
    etat = false; uint32_t fin = t;
    t = fin + PRESENCE_MAINTIEN_MS; r = presence_pas(&p, &ops, t); assert(r.presente);
    t = fin + PRESENCE_MAINTIEN_MS + 100; r = presence_pas(&p, &ops, t); assert(r.valide && !r.presente);

    /* capteur muet : invalide et faux, jamais "personne" valide */
    etat = true;
    for (int i = 0; i < PRESENCE_CONFIRMATIONS; i++) { t += 100; r = presence_pas(&p, &ops, t); }
    assert(r.presente);
    ok = false; t += 100; r = presence_pas(&p, &ops, t); assert(!r.valide && !r.presente);
    ok = true; t += 100; r = presence_pas(&p, &ops, t); assert(r.valide && !r.presente);   /* repart de zero */

    /* pilote absent */
    r = presence_pas(&p, 0, t); assert(!r.valide && !r.presente);

    puts("test_presence OK");
    return 0;
}
