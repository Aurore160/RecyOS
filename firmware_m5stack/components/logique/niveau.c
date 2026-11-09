#include "niveau.h"

static const int32_t dist_vide[CAPTEUR_NIVEAU_NB_BACS] = NIVEAU_DIST_VIDE_MM;
static const int32_t dist_plein[CAPTEUR_NIVEAU_NB_BACS] = NIVEAU_DIST_PLEIN_MM;

void niveau_init(niveau_bac_t *b) { b->n = 0; b->suivant = 0; }

int32_t niveau_distance_mm(uint32_t echo_us, float temp_c)
{
    float v = 331.3f + 0.606f * temp_c;                 /* m/s */
    return (int32_t)((float)echo_us * v / 2000.0f + 0.5f);
}

float niveau_pct(int indice, int32_t d)
{
    if (indice < 0 || indice >= CAPTEUR_NIVEAU_NB_BACS) return 0.0f;
    float plage = (float)(dist_vide[indice] - dist_plein[indice]);
    if (plage <= 0.0f) return 0.0f;
    float p = (float)(dist_vide[indice] - d) * 100.0f / plage;
    return p < 0.0f ? 0.0f : (p > 100.0f ? 100.0f : p);
}

static int32_t mediane(const int32_t *v, int n)
{
    int32_t t[NIVEAU_HISTORIQUE > NIVEAU_ECHANTILLONS ? NIVEAU_HISTORIQUE : NIVEAU_ECHANTILLONS];
    for (int i = 0; i < n; i++) t[i] = v[i];
    for (int i = 1; i < n; i++) {                       /* tri par insertion */
        int32_t x = t[i]; int j = i - 1;
        while (j >= 0 && t[j] > x) { t[j + 1] = t[j]; j--; }
        t[j + 1] = x;
    }
    return (n % 2) ? t[n / 2] : (t[n / 2 - 1] + t[n / 2]) / 2;
}

niveau_resultat_t niveau_cycle(niveau_bac_t *b, int indice, const capteur_niveau_ops_t *ops,
                               float temp_c, bool temp_valide, bool bloque)
{
    niveau_resultat_t r = { false, 0, 0.0f, false };
    if (indice < 0 || indice >= CAPTEUR_NIVEAU_NB_BACS || ops == 0 || ops->mesurer_echo_us == 0) return r;
    if (bloque) { niveau_init(b); return r; }           /* le bac bouge : on repart d'un historique vide */
    if (!temp_valide) temp_c = NIVEAU_TEMP_DEFAUT_C;

    int32_t ok[NIVEAU_ECHANTILLONS]; int nb = 0;
    for (int i = 0; i < NIVEAU_ECHANTILLONS; i++) {
        uint32_t us;
        if (!ops->mesurer_echo_us(indice, &us)) continue;
        int32_t d = niveau_distance_mm(us, temp_c);
        if (d < NIVEAU_DIST_MIN_MM || d > NIVEAU_DIST_MAX_MM) continue;
        ok[nb++] = d;
    }
    if (nb < NIVEAU_ECHANTILLONS_MIN) return r;

    b->hist[b->suivant] = mediane(ok, nb);
    b->suivant = (b->suivant + 1) % NIVEAU_HISTORIQUE;
    if (b->n < NIVEAU_HISTORIQUE) b->n++;

    r.valide = true;
    r.distance_mm = mediane(b->hist, b->n);
    r.pct = niveau_pct(indice, r.distance_mm);
    r.vide = r.pct <= NIVEAU_SEUIL_VIDE_PCT;
    return r;
}
