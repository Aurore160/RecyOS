#include <stdio.h>
#include <assert.h>
#include "tri.h"
#include "config_tri.h"

int main(void)
{
    tri_t t; tri_init(&t); uint32_t now = 0;
    assert(tri_demarrer(&t, TRI_BAC_PLASTIQUE, false, now) == TRI_REFUSE_NON_REFERENCE);
    tri_set_reference(&t, true);
    assert(tri_demarrer(&t, TRI_BAC_AUCUN, false, now) == TRI_REFUSE_BAC);
    assert(tri_demarrer(&t, (tri_bac_t)4, false, now) == TRI_REFUSE_BAC);
    assert(tri_demarrer(&t, TRI_BAC_PAPIER, true, now) == TRI_REFUSE_URGENCE);
    assert(tri_demarrer(&t, TRI_BAC_PAPIER, false, now) == TRI_OK);
    assert(tri_demarrer(&t, TRI_BAC_PLASTIQUE, false, now) == TRI_REFUSE_OCCUPE);

    tri_sortie_t s = tri_pas(&t, now, false, false);
    assert(s.deplacer && s.cible_mm == TRI_X_BAC_PAPIER_MM);
    now += 3000; s = tri_pas(&t, now, true, false);          /* arrive */
    assert(t.etat == TRI_OUVERTURE_FOND);
    s = tri_pas(&t, now, false, false);
    assert(s.servo && s.servo_deg == TRI_SERVO_ANGLE_OUVERT_DEG);
    now += TRI_SERVO_DUREE_MOUVEMENT_MS; tri_pas(&t, now, false, false);
    assert(t.etat == TRI_CHUTE);
    now += TRI_DUREE_CHUTE_MS; tri_pas(&t, now, false, false);
    assert(t.etat == TRI_FERMETURE_FOND);
    s = tri_pas(&t, now, false, false);
    assert(s.servo && s.servo_deg == TRI_SERVO_ANGLE_FERME_DEG);
    now += TRI_SERVO_DUREE_MOUVEMENT_MS; tri_pas(&t, now, false, false);
    assert(t.etat == TRI_RETOUR);
    s = tri_pas(&t, now, false, false);
    assert(s.deplacer && s.cible_mm == TRI_X_OUVERTURE_MM);
    now += 3000; tri_pas(&t, now, true, false);
    assert(t.etat == TRI_REPOS);

    /* urgence en cours de route */
    assert(tri_demarrer(&t, TRI_BAC_PLASTIQUE, false, now) == TRI_OK);
    s = tri_pas(&t, now + 100, false, true);
    assert(s.arreter && t.etat == TRI_DEFAUT);
    assert(tri_demarrer(&t, TRI_BAC_PLASTIQUE, false, now) == TRI_REFUSE_OCCUPE);

    /* delai depasse */
    tri_init(&t); tri_set_reference(&t, true); now = 0;
    tri_demarrer(&t, TRI_BAC_PLASTIQUE, false, now);
    s = tri_pas(&t, now + TRI_DELAI_DEPLACEMENT_MS + 1, false, false);
    assert(s.arreter && t.etat == TRI_DEFAUT);
    puts("test_tri OK");
    return 0;
}
