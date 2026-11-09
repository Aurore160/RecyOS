#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "niveau.h"

/* Pilote simule : suite de durees d'echo ; 0 = pas d'echo. */
static uint32_t suite[64]; static int ns, pos;
static bool fake(int indice, uint32_t *us)
{
    (void)indice;
    if (pos >= ns) return false;
    uint32_t v = suite[pos++];
    if (v == 0) return false;
    *us = v; return true;
}
static const capteur_niveau_ops_t ops = { .mesurer_echo_us = fake };
static void charge(const uint32_t *v, int n) { for (int i = 0; i < n; i++) suite[i] = v[i]; ns = n; pos = 0; }
/* echo pour une distance donnee a 20 C */
static uint32_t echo(int32_t mm) { return (uint32_t)((float)mm * 2000.0f / (331.3f + 0.606f * 20.0f) + 0.5f); }

int main(void)
{
    /* conversion : 1 m a 20 C ~ 5830 us */
    assert(abs((int)niveau_distance_mm(5830, 20.0f) - 1000) <= 1);
    /* compensation : meme echo, plus chaud = plus loin */
    assert(niveau_distance_mm(5830, 45.0f) > niveau_distance_mm(5830, 10.0f));

    /* pourcentage : vide 900, plein 250 (provisoires) */
    assert(fabsf(niveau_pct(0, 900) - 0.0f) < 0.01f);
    assert(fabsf(niveau_pct(0, 250) - 100.0f) < 0.01f);
    assert(niveau_pct(0, 1200) == 0.0f && niveau_pct(0, 100) == 100.0f);   /* bornes */
    assert(fabsf(niveau_pct(0, 575) - 50.0f) < 0.01f);

    niveau_bac_t b; niveau_init(&b);
    /* cycle normal : 3 mesures proches de 575 mm -> 50 % */
    uint32_t a[] = { echo(575), echo(576), echo(574) }; charge(a, 3);
    niveau_resultat_t r = niveau_cycle(&b, 0, &ops, 20.0f, true, false);
    assert(r.valide && abs(r.distance_mm - 575) <= 2 && fabsf(r.pct - 50.0f) < 1.0f && !r.vide);

    /* mesure aberrante : la mediane l'ignore */
    niveau_init(&b); uint32_t c[] = { echo(575), echo(2000), echo(576) }; charge(c, 3);
    r = niveau_cycle(&b, 0, &ops, 20.0f, true, false);
    assert(r.valide && abs(r.distance_mm - 576) <= 2);

    /* un seul echo valide sur 3 : cycle invalide, jamais 0 % */
    niveau_init(&b); uint32_t d[] = { echo(575), 0, 0 }; charge(d, 3);
    r = niveau_cycle(&b, 0, &ops, 20.0f, true, false);
    assert(!r.valide && r.pct == 0.0f);

    /* zone aveugle (150 mm < 200 mm) : invalide */
    niveau_init(&b); uint32_t e[] = { echo(150), echo(150), echo(150) }; charge(e, 3);
    r = niveau_cycle(&b, 1, &ops, 20.0f, true, false);
    assert(!r.valide);

    /* bac occupe (tri / compactage) : invalide et historique efface */
    niveau_init(&b); charge(a, 3); niveau_cycle(&b, 0, &ops, 20.0f, true, false);
    assert(b.n == 1);
    charge(a, 3); r = niveau_cycle(&b, 0, &ops, 20.0f, true, true);
    assert(!r.valide && b.n == 0 && pos == 0);   /* aucune mesure faite pendant l'occupation */

    /* bac vide : <= 5 % */
    niveau_init(&b); uint32_t f[] = { echo(890), echo(892), echo(891) }; charge(f, 3);
    r = niveau_cycle(&b, 2, &ops, 20.0f, true, false);
    assert(r.valide && r.vide);

    /* mediane glissante : 5 cycles, le cinquieme aberrant ne bouge presque pas le resultat */
    niveau_init(&b);
    for (int k = 0; k < 4; k++) { uint32_t g[] = { echo(500), echo(500), echo(500) }; charge(g, 3); niveau_cycle(&b, 0, &ops, 20.0f, true, false); }
    uint32_t h[] = { echo(300), echo(300), echo(300) }; charge(h, 3);
    r = niveau_cycle(&b, 0, &ops, 20.0f, true, false);
    assert(r.valide && abs(r.distance_mm - 500) <= 2);

    /* indice invalide ou pilote absent */
    assert(!niveau_cycle(&b, 7, &ops, 20.0f, true, false).valide);
    assert(!niveau_cycle(&b, 0, 0, 20.0f, true, false).valide);

    /* temperature indisponible : valeur par defaut, cycle valide */
    niveau_init(&b); charge(a, 3);
    assert(niveau_cycle(&b, 0, &ops, 0.0f, false, false).valide);

    puts("test_niveau OK");
    return 0;
}
