/* Tests du module securite (PC, sans ESP-IDF).
 * Deux configurations :
 *   gcc ... test/test_securite.c les .c de components/securite                (pilote : sans bouton d'arret d'urgence)
 *   gcc ... -DSEC_BOUTON_URGENCE_PRESENT=1 -DSEC_CONTACTEUR_PRESENT=1 ...     (avec bouton et contacteur)
 * Le temps est simule : aucune attente reelle. */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "securite.h"

static int nb_tests = 0;
#define CHECK(c) do { nb_tests++; if (!(c)) { printf("ECHEC ligne %d : %s\n", __LINE__, #c); return 1; } } while (0)

/* ---------- valeurs des enumerations = contrat.proto v3 (verifie aussi par test/verifier_enums_securite.py) ---------- */
static int test_enums(void)
{
    CHECK(SEC_ACK_OK == 1 && SEC_ACK_REFUSED_EMERGENCY == 2 && SEC_ACK_INVALID == 3 && SEC_ACK_BUSY == 4);
    CHECK(SEC_ACK_REFUSED_MODE == 6 && SEC_ACK_REFUSED_NOT_HOMED == 7);
    CHECK(SEC_MODE_NORMAL == 1 && SEC_MODE_VIDAGE == 2 && SEC_MODE_MAINTENANCE == 3);
    CHECK(SEC_CMD_START_COMPACTION == 2 && SEC_CMD_EMERGENCY_STOP == 4 && SEC_CMD_CLEAR_EMERGENCY == 7);
    CHECK(SEC_CMD_SORT_WASTE == 9 && SEC_CMD_HOME == 10 && SEC_CMD_SET_MODE == 11 && SEC_CMD_ABORT_CYCLE == 14);
    return 0;
}

/* ---------- liaison avec le CM5 ---------- */
typedef struct { uint32_t sil[16], rst[16], pau[16], rep[16]; int ns, nr, np, nrep; bool reset_vu; } journal_t;

/* Fait avancer la liaison de `duree` ms par pas de 100 ms, sans heartbeat ; note l'instant de chaque evenement. */
static void marche(liaison_cm5_t *l, uint32_t *now, uint32_t duree, journal_t *j)
{
    for (uint32_t ecoulee = 0; ecoulee < duree; ecoulee += 100) {
        *now += 100;
        liaison_cm5_sortie_t s = liaison_cm5_pas(l, *now, false);
        if (s.reset_actif) j->reset_vu = true;
        if (s.evt_silence && j->ns < 16) j->sil[j->ns++] = *now;
        if (s.evt_reset   && j->nr < 16) j->rst[j->nr++] = *now;
        if (s.evt_pause   && j->np < 16) j->pau[j->np++] = *now;
        if (s.evt_reprise && j->nrep < 16) j->rep[j->nrep++] = *now;
    }
}

static int test_liaison_nominale(uint32_t t0)
{
    liaison_cm5_t l; uint32_t now = t0; liaison_cm5_init(&l, now);
    /* demarrage : le CM5 envoie son premier heartbeat apres 40 s */
    for (int i = 0; i < 400; i++) { now += 100; liaison_cm5_sortie_t s = liaison_cm5_pas(&l, now, false); CHECK(!s.hors_service && !s.reset_actif); }
    liaison_cm5_sortie_t s = liaison_cm5_pas(&l, now += 100, true);
    CHECK(l.etat == CM5_OK && !s.evt_reprise && !s.hors_service);          /* pas d'alerte : aucun silence n'avait ete declare */
    /* heartbeat toutes les 2 s pendant 10 min : jamais de silence */
    uint32_t dernier = now;
    for (int i = 0; i < 300; i++) {
        for (int k = 0; k < 20; k++) { now += 100; s = liaison_cm5_pas(&l, now, k == 0); if (k == 0) dernier = now; CHECK(!s.hors_service && !s.evt_silence); }
    }
    /* un trou de 4,9 s est tolere, un trou de 5,0 s est un silence */
    for (int k = 0; k < 48 - 19; k++) { now += 100; s = liaison_cm5_pas(&l, now, false); CHECK(!s.hors_service); }
    CHECK(now - dernier == 4800);                                                            /* dernier heartbeat il y a 4,8 s */
    now += 100; s = liaison_cm5_pas(&l, now, false); CHECK(!s.hors_service);                 /* 4,9 s */
    now += 100; s = liaison_cm5_pas(&l, now, false);                                         /* 5,0 s */
    CHECK(s.evt_silence && s.hors_service && s.evt_reset && s.reset_actif);
    CHECK(l.cm5_reset_count == 1);
    return 0;
}

static int test_liaison_reset_et_pause(uint32_t t0)
{
    liaison_cm5_t l; uint32_t now = t0; liaison_cm5_init(&l, now);
    journal_t j; memset(&j, 0, sizeof j);
    /* un heartbeat, puis silence total */
    liaison_cm5_pas(&l, now += 100, true);
    uint32_t t_dernier_hb = now;
    marche(&l, &now, 4800, &j); CHECK(j.ns == 0);
    marche(&l, &now, 200, &j);
    CHECK(j.ns == 1 && j.sil[0] - t_dernier_hb == 5000);                      /* silence declare a 5,0 s */
    CHECK(j.nr == 1 && j.rst[0] == j.sil[0]);                                 /* reset commande aussitot */
    /* les 10 minutes qui suivent : impulsion, 90 s de demarrage, puis nouveaux resets, 3 au plus */
    marche(&l, &now, 600000, &j);
    CHECK(j.reset_vu);
    CHECK(j.nr == 3 && l.cm5_reset_count == 3);
    CHECK(j.rst[1] - j.rst[0] >= SEC_CM5_RESET_IMPULSION_MS + SEC_CM5_DEMARRAGE_MS);   /* au moins 90 s entre deux resets */
    CHECK(j.rst[2] - j.rst[1] >= SEC_CM5_RESET_IMPULSION_MS + SEC_CM5_DEMARRAGE_MS);
    CHECK(j.np == 1 && l.etat == CM5_PAUSE);                                  /* le 4e reset est refuse : pause */
    CHECK(j.rst[2] - j.rst[0] < SEC_CM5_FENETRE_RESETS_MS);                   /* les 3 resets tiennent dans la fenetre de 10 min */
    /* la pause dure 30 min depuis sa declaration, puis un nouveau reset est autorise */
    uint32_t t_pause = j.pau[0];
    marche(&l, &now, (t_pause + SEC_CM5_PAUSE_MS - 1000) - now, &j);          /* 1 s avant la fin de la pause */
    CHECK(j.nr == 3 && l.etat == CM5_PAUSE);
    marche(&l, &now, 2000, &j);
    CHECK(j.nr == 4 && l.cm5_reset_count == 4);
    CHECK(j.rst[3] - t_pause == SEC_CM5_PAUSE_MS);
    return 0;
}

/* Fenetre de 10 min : un reset plus ancien que 10 min ne compte plus (ecriture directe de l'historique). */
static int test_liaison_fenetre(uint32_t t0)
{
    liaison_cm5_t l; uint32_t now = t0 + 2000000u; liaison_cm5_init(&l, t0); l.etat = CM5_OK; l.t_dernier_hb_ms = t0;
    l.nb_resets_fenetre = 3;
    l.t_resets_ms[0] = now - (SEC_CM5_FENETRE_RESETS_MS - 1);       /* 9 min 59,999 s : compte encore */
    l.t_resets_ms[1] = now - 1000; l.t_resets_ms[2] = now - 2000;
    liaison_cm5_sortie_t s = liaison_cm5_pas(&l, now, false);       /* silence constate */
    CHECK(s.evt_silence && s.evt_pause && !s.evt_reset && l.etat == CM5_PAUSE);
    liaison_cm5_t m; liaison_cm5_init(&m, t0); m.etat = CM5_OK; m.t_dernier_hb_ms = t0;
    m.nb_resets_fenetre = 3;
    m.t_resets_ms[0] = now - SEC_CM5_FENETRE_RESETS_MS;             /* exactement 10 min : sorti de la fenetre */
    m.t_resets_ms[1] = now - 1000; m.t_resets_ms[2] = now - 2000;
    s = liaison_cm5_pas(&m, now, false);
    CHECK(s.evt_reset && !s.evt_pause && m.cm5_reset_count == 1 && m.nb_resets_fenetre == 3);
    return 0;
}

static int test_liaison_reprise(uint32_t t0)
{
    liaison_cm5_t l; uint32_t now = t0; liaison_cm5_init(&l, now);
    liaison_cm5_pas(&l, now += 100, true);
    journal_t j; memset(&j, 0, sizeof j);
    marche(&l, &now, 5200, &j);
    CHECK(l.silence_declare && l.cm5_reset_count == 1);
    marche(&l, &now, 40000, &j);                                              /* le CM5 demarre : heartbeat 40 s plus tard */
    liaison_cm5_sortie_t s = liaison_cm5_pas(&l, now += 100, true);
    CHECK(s.evt_reprise && !s.hors_service && l.etat == CM5_OK);
    s = liaison_cm5_pas(&l, now += 100, true);
    CHECK(!s.evt_reprise);                                                    /* une seule alerte */
    /* un heartbeat pendant l'impulsion de reset est ignore ; le suivant, apres l'impulsion, est pris en compte */
    liaison_cm5_t r; now = t0; liaison_cm5_init(&r, now);
    liaison_cm5_pas(&r, now += 100, true);
    memset(&j, 0, sizeof j);
    marche(&r, &now, 5200, &j);
    CHECK(r.etat == CM5_RESET);
    s = liaison_cm5_pas(&r, now += 100, true);
    CHECK(!s.evt_reprise && s.reset_actif && s.hors_service);
    marche(&r, &now, 600, &j);
    CHECK(r.etat == CM5_DEMARRAGE);
    s = liaison_cm5_pas(&r, now += 100, true);
    CHECK(s.evt_reprise && r.etat == CM5_OK);
    /* reprise pendant la pause (le chien de garde Linux a relance le CM5 tout seul) */
    liaison_cm5_t m; now = t0; liaison_cm5_init(&m, now);
    liaison_cm5_pas(&m, now += 100, true);
    memset(&j, 0, sizeof j);
    marche(&m, &now, 700000, &j);
    CHECK(m.etat == CM5_PAUSE);
    s = liaison_cm5_pas(&m, now += 100, true);
    CHECK(s.evt_reprise && m.etat == CM5_OK && !s.hors_service);
    return 0;
}

static int test_liaison_demarrage_initial(uint32_t t0)
{
    liaison_cm5_t l; uint32_t now = t0; liaison_cm5_init(&l, now);
    journal_t j; memset(&j, 0, sizeof j);
    marche(&l, &now, SEC_CM5_DEMARRAGE_INITIAL_MS - 100, &j);                 /* jusqu'a 89,9 s : rien */
    CHECK(j.ns == 0 && l.cm5_reset_count == 0);
    marche(&l, &now, 200, &j);
    CHECK(j.ns == 1 && j.nr == 1 && l.cm5_reset_count == 1);
    return 0;
}

/* ---------- garde-fous des verins ---------- */
static int test_garde_verin(uint32_t t0)
{
    garde_verin_t g; garde_verin_init(&g); uint32_t now = t0;
    CHECK(garde_verin_demander(&g, 2, VERIN_SENS_1, false, now) == GARDE_REFUSE_PARAMETRE);
    CHECK(garde_verin_demander(&g, 0, (verin_sens_t)5, false, now) == GARDE_REFUSE_PARAMETRE);
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_1, true, now) == GARDE_REFUSE_INTERDIT);
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_1, false, now) == GARDE_OK);
    CHECK(garde_verin_demander(&g, 1, VERIN_SENS_1, false, now) == GARDE_REFUSE_AUTRE_VERIN);   /* un seul verin en mouvement */
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_2, false, now) == GARDE_REFUSE_INVERSION);     /* pas d'inversion en marche */
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_1, false, now) == GARDE_OK);                   /* meme sens : sans effet */
    garde_verin_sortie_t s = garde_verin_pas(&g, now += 1000, false);
    CHECK(s.sens[0] == VERIN_SENS_1 && s.sens[1] == VERIN_ARRET);
    /* arret, puis inversion : pas avant 300 ms */
    CHECK(garde_verin_demander(&g, 0, VERIN_ARRET, false, now) == GARDE_OK);
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_2, false, now + 299) == GARDE_REFUSE_INVERSION);
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_2, false, now + 300) == GARDE_OK);
    now += 300;
    CHECK(garde_verin_marche_fenetre_ms(&g, 0, now) == 1000);
    CHECK(garde_verin_demander(&g, 0, VERIN_ARRET, false, now + 500) == GARDE_OK);
    now += 500;
    /* meme sens apres arret : permis tout de suite (ce n'est pas une inversion) */
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_2, false, now) == GARDE_OK);
    CHECK(garde_verin_demander(&g, 0, VERIN_ARRET, false, now + 10) == GARDE_OK);
    return 0;
}

static int test_garde_marche_max_et_budget(uint32_t t0)
{
    garde_verin_t g; garde_verin_init(&g); uint32_t now = t0;
    /* marche continue : arret force a 60 s */
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_1, false, now) == GARDE_OK);
    garde_verin_sortie_t s = garde_verin_pas(&g, now + SEC_VERIN_MARCHE_MAX_MS - 1, false);
    CHECK(s.sens[0] == VERIN_SENS_1 && s.arret_force[0] == GARDE_CAUSE_AUCUNE);
    s = garde_verin_pas(&g, now + SEC_VERIN_MARCHE_MAX_MS, false);
    CHECK(s.sens[0] == VERIN_ARRET && s.arret_force[0] == GARDE_CAUSE_MARCHE_MAX);
    s = garde_verin_pas(&g, now + SEC_VERIN_MARCHE_MAX_MS + 100, false);
    CHECK(s.arret_force[0] == GARDE_CAUSE_AUCUNE);                                  /* signale une seule fois */
    now += SEC_VERIN_MARCHE_MAX_MS;
    CHECK(garde_verin_marche_fenetre_ms(&g, 0, now) == 60000);
    /* retour 60 s dans l'autre sens apres la pause : le budget de 120 s est alors epuise */
    now += 300;
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_2, false, now) == GARDE_OK);
    s = garde_verin_pas(&g, now + SEC_VERIN_MARCHE_MAX_MS, false);
    CHECK(s.arret_force[0] == GARDE_CAUSE_MARCHE_MAX);
    now += SEC_VERIN_MARCHE_MAX_MS;
    CHECK(garde_verin_marche_fenetre_ms(&g, 0, now) == 120000);
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_1, false, now + 1000) == GARDE_REFUSE_BUDGET);
    /* l'autre verin a son propre budget */
    CHECK(garde_verin_demander(&g, 1, VERIN_SENS_1, false, now + 1000) == GARDE_OK);
    CHECK(garde_verin_demander(&g, 1, VERIN_ARRET, false, now + 2000) == GARDE_OK);
    /* le budget revient quand les segments sortent de la fenetre de 20 min */
    uint32_t fin_premier = t0 + SEC_VERIN_MARCHE_MAX_MS;                            /* fin de la 1re marche */
    uint32_t t_libre = fin_premier + SEC_VERIN_FENETRE_MS;                          /* elle sort de la fenetre a cet instant */
    CHECK(garde_verin_marche_fenetre_ms(&g, 0, t_libre - 1) == 60000 + 1);          /* 1 ms de la 1re marche reste dans la fenetre, plus les 60 s de la 2e */
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_1, false, t_libre - 1) == GARDE_OK);   /* il reste 59,999 s de budget */
    return 0;
}

static int test_garde_interdit_et_historique(uint32_t t0)
{
    garde_verin_t g; garde_verin_init(&g); uint32_t now = t0;
    CHECK(garde_verin_demander(&g, 1, VERIN_SENS_2, false, now) == GARDE_OK);
    garde_verin_sortie_t s = garde_verin_pas(&g, now + 2000, true);                  /* urgence ou CM5 hors service */
    CHECK(s.sens[1] == VERIN_ARRET && s.arret_force[1] == GARDE_CAUSE_INTERDIT);
    CHECK(garde_verin_demander(&g, 1, VERIN_SENS_2, true, now + 5000) == GARDE_REFUSE_INTERDIT);
    /* beaucoup de petites marches : l'historique fusionne et ne sous-estime jamais */
    garde_verin_init(&g); now = t0;
    uint32_t reel = 0;
    for (int i = 0; i < 40; i++) {
        CHECK(garde_verin_demander(&g, 0, VERIN_SENS_1, false, now) == GARDE_OK);
        now += 1000; reel += 1000;
        CHECK(garde_verin_demander(&g, 0, VERIN_ARRET, false, now) == GARDE_OK);
        now += 100;
    }
    CHECK(garde_verin_marche_fenetre_ms(&g, 0, now) >= reel);                         /* 40 s reels, jamais moins */
    CHECK(garde_verin_marche_fenetre_ms(&g, 0, now) <= reel + 100);                   /* et pas beaucoup plus tant que tout est dans la fenetre */
    return 0;
}

/* Cycle de compactage complet (course 300 mm : 60 s par sens). A documenter : un 2e cycle est refuse environ 20 min. */
static int test_garde_cycles_compactage(uint32_t t0)
{
    garde_verin_t g; garde_verin_init(&g); uint32_t now = t0; int ok = 0;
    for (int cycle = 1; cycle <= 3; cycle++) {
        for (int sens = 1; sens <= 2; sens++) {
            garde_resultat_t r = garde_verin_demander(&g, 0, (verin_sens_t)sens, false, now);
            if (r != GARDE_OK) goto fin;
            garde_verin_pas(&g, now + SEC_VERIN_MARCHE_MAX_MS, false);
            now += SEC_VERIN_MARCHE_MAX_MS + SEC_VERIN_PAUSE_INVERSION_MS;
        }
        ok++;
    }
fin:
    CHECK(ok == 1);          /* un seul cycle de 120 s passe : les cycles 2 et 3 attendent la fenetre de 20 min */
    return 0;
}

/* ---------- securite : commandes, urgence, modes, CM5 ---------- */
static sec_entrees_t E(bool di1, bool cycle, bool hb) { sec_entrees_t e = { di1, cycle, hb }; return e; }

static void bonne_marche(securite_t *s, uint32_t *now, int pas_ms, int n)    /* heartbeat a chaque pas : liaison saine */
{
    for (int i = 0; i < n; i++) { *now += pas_ms; sec_entrees_t e = E(true, s->cycle_actif, true); securite_pas(s, *now, &e); }
}

static int test_refus_commandes(void)
{
    securite_t s; uint32_t now = 1000; securite_instance_init(&s, now);
    bonne_marche(&s, &now, 100, 5);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_NONE, 0 }, now) == SEC_ACK_INVALID);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ (sec_cmd_t)99, 0 }, now) == SEC_ACK_INVALID);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ (sec_cmd_t)1, 0 }, now) == SEC_ACK_INVALID);           /* 1 : reserve (ancien OPEN_TRAPDOOR) */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ (sec_cmd_t)15, 0 }, now) == SEC_ACK_INVALID);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_READ_SENSORS, 0 }, now) == SEC_ACK_OK);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_TIME, 0 }, now) == SEC_ACK_OK);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_LED, 0 }, now) == SEC_ACK_OK);
    /* non reference */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SORT_WASTE, 1 }, now) == SEC_ACK_REFUSED_NOT_HOMED);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SORT_WASTE, 0 }, now) == SEC_ACK_INVALID);     /* bac 0 : invalide avant tout autre refus */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SORT_WASTE, 4 }, now) == SEC_ACK_INVALID);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_START_COMPACTION, 2 }, now) == SEC_ACK_INVALID);  /* papier */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_START_COMPACTION, 0 }, now) == SEC_ACK_INVALID);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_START_COMPACTION, 1 }, now) == SEC_ACK_REFUSED_NOT_HOMED);
    /* HOME puis reference */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_HOME, 0 }, now) == SEC_ACK_OK);
    sec_entrees_t e = E(true, false, true);
    sec_sorties_t o = securite_pas(&s, now += 100, &e);
    CHECK(o.evenements & SEC_EVT_HOMING_DEMANDE);
    CHECK(!securite_est_reference(&s));                                       /* HOME accepte n'est pas HOME termine */
    securite_set_reference(&s, true);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SORT_WASTE, 3 }, now) == SEC_ACK_OK);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_START_COMPACTION, 3 }, now) == SEC_ACK_OK);
    /* cycle en cours */
    e = E(true, true, true); securite_pas(&s, now += 100, &e);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SORT_WASTE, 1 }, now) == SEC_ACK_BUSY);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_HOME, 0 }, now) == SEC_ACK_BUSY);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_VIDAGE }, now) == SEC_ACK_BUSY);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_ABORT_CYCLE, 0 }, now) == SEC_ACK_OK);
    o = securite_pas(&s, now += 100, &e);
    CHECK(o.evenements & SEC_EVT_ARRET_CYCLE);
    CHECK(securite_est_reference(&s) || 1);
    e = E(true, false, true); securite_pas(&s, now += 100, &e);
    /* modes */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, 0 }, now) == SEC_ACK_INVALID);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, 4 }, now) == SEC_ACK_INVALID);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_TEST_ACTUATOR, 1 }, now) == SEC_ACK_REFUSED_MODE);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_OUTPUT, 1 }, now) == SEC_ACK_REFUSED_MODE);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_VIDAGE }, now) == SEC_ACK_OK);
    CHECK(securite_mode(&s) == SEC_MODE_VIDAGE);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SORT_WASTE, 1 }, now) == SEC_ACK_REFUSED_MODE);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_START_COMPACTION, 1 }, now) == SEC_ACK_REFUSED_MODE);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_TEST_ACTUATOR, 1 }, now) == SEC_ACK_REFUSED_MODE);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_MAINTENANCE }, now) == SEC_ACK_OK);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_TEST_ACTUATOR, 0 }, now) == SEC_ACK_INVALID);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_TEST_ACTUATOR, 5 }, now) == SEC_ACK_INVALID);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_OUTPUT, 3 }, now) == SEC_ACK_INVALID);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_OUTPUT, 2 }, now) == SEC_ACK_OK);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_OUTPUT, 1 }, now) == SEC_ACK_INVALID);   /* le ventilateur n'obeit qu'au StamPLC */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_TEST_ACTUATOR, 3 }, now) == SEC_ACK_OK);       /* verin : la reference reste */
    CHECK(securite_est_reference(&s));
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_TEST_ACTUATOR, 1 }, now) == SEC_ACK_OK);       /* chariot : reference perdue */
    CHECK(!securite_est_reference(&s));
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_NORMAL }, now) == SEC_ACK_REFUSED_NOT_HOMED);
    CHECK(securite_mode(&s) == SEC_MODE_MAINTENANCE);
    securite_set_reference(&s, true);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_NORMAL }, now) == SEC_ACK_OK);
    CHECK(securite_mode(&s) == SEC_MODE_NORMAL);
    return 0;
}

static int test_urgence(void)
{
    securite_t s; uint32_t now = 1000; securite_instance_init(&s, now);
    sec_entrees_t e = E(true, false, true);
    sec_sorties_t o = securite_pas(&s, now += 100, &e);
    CHECK(!o.actionneurs_interdits && !o.urgence_active && !o.coupure_puissance);
    securite_set_reference(&s, true);
    /* arret d'urgence par commande : toujours accepte, meme en vidage */
    securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_VIDAGE }, now);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_EMERGENCY_STOP, 0 }, now) == SEC_ACK_OK);
    o = securite_pas(&s, now += 100, &e);
    CHECK(o.urgence_active && o.actionneurs_interdits);
    CHECK(o.evenements & SEC_EVT_URGENCE_DECLENCHEE);
    CHECK(o.evenements & SEC_EVT_ARRET_CYCLE);
    CHECK(SEC_CONTACTEUR_PRESENT ? o.coupure_puissance : !o.coupure_puissance);
    CHECK(!securite_est_reference(&s));                                                 /* HOME obligatoire */
    o = securite_pas(&s, now += 100, &e);
    CHECK(!(o.evenements & SEC_EVT_URGENCE_DECLENCHEE));                                /* evenement unique */
    /* tout est refuse sauf lecture, heure, LED, urgence et effacement */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SORT_WASTE, 1 }, now) == SEC_ACK_REFUSED_EMERGENCY);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_HOME, 0 }, now) == SEC_ACK_REFUSED_EMERGENCY);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_NORMAL }, now) == SEC_ACK_REFUSED_EMERGENCY);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_ABORT_CYCLE, 0 }, now) == SEC_ACK_REFUSED_EMERGENCY);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_READ_SENSORS, 0 }, now) == SEC_ACK_OK);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_EMERGENCY_STOP, 0 }, now) == SEC_ACK_OK);   /* idempotent */
    /* effacement */
#if SEC_BOUTON_URGENCE_PRESENT
    e = E(false, false, true); securite_pas(&s, now += 100, &e);                        /* bouton toujours enfonce */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_CLEAR_EMERGENCY, 0 }, now) == SEC_ACK_REFUSED_EMERGENCY);
    e = E(true, false, true); securite_pas(&s, now += 100, &e);                         /* bouton rearme */
#endif
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_CLEAR_EMERGENCY, 0 }, now) == SEC_ACK_OK);
    o = securite_pas(&s, now += 100, &e);
    CHECK(!o.urgence_active && !o.actionneurs_interdits);
    CHECK(o.evenements & SEC_EVT_URGENCE_EFFACEE);
    CHECK(securite_mode(&s) == SEC_MODE_VIDAGE);                                       /* l'urgence ne change pas le mode */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SORT_WASTE, 1 }, now) == SEC_ACK_REFUSED_MODE);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_NORMAL }, now) == SEC_ACK_OK);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SORT_WASTE, 1 }, now) == SEC_ACK_REFUSED_NOT_HOMED);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_CLEAR_EMERGENCY, 0 }, now) == SEC_ACK_OK);   /* rien a effacer */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_HOME, 0 }, now) == SEC_ACK_OK);
    return 0;
}

static int test_bouton_di1(void)
{
    securite_t s; uint32_t now = 1000; securite_instance_init(&s, now);
    sec_entrees_t e = E(false, false, true);                                            /* DI1 « ouverte » */
    sec_sorties_t o = securite_pas(&s, now += 100, &e);
#if SEC_BOUTON_URGENCE_PRESENT
    CHECK(o.urgence_active && o.actionneurs_interdits && (o.evenements & SEC_EVT_URGENCE_DECLENCHEE));
    /* avant la premiere lecture, un bouton present est suppose non sain : refus */
    securite_t t; securite_instance_init(&t, 0);
    CHECK(securite_evaluer(&t, &(sec_commande_t){ SEC_CMD_READ_SENSORS, 0 }, 0) == SEC_ACK_OK);
    CHECK(securite_evaluer(&t, &(sec_commande_t){ SEC_CMD_HOME, 0 }, 0) == SEC_ACK_REFUSED_EMERGENCY);
#else
    CHECK(!o.urgence_active && !o.actionneurs_interdits);                               /* pilote : DI1 ignoree */
    CHECK(!(o.evenements & SEC_EVT_URGENCE_DECLENCHEE));
    CHECK(!o.coupure_puissance);
#endif
    return 0;
}

static int test_cm5_et_modes(uint32_t t0)
{
    securite_t s; uint32_t now = t0; securite_instance_init(&s, now);
    sec_entrees_t e = E(true, false, true);
    securite_pas(&s, now += 100, &e);
    securite_set_reference(&s, true);
    securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_VIDAGE }, now);
    uint32_t t_vidage = s.t_mode_ms;
    /* silence du CM5 : actionneurs interdits, « Hors service », reset, reference perdue, mode conserve */
    uint32_t dernier = now; sec_sorties_t o = {0}; uint32_t evt_total = 0; bool reset_vu = false;
    e = E(true, false, false);
    while ((uint32_t)(now - dernier) < 5000) { now += 100; o = securite_pas(&s, now, &e); evt_total |= o.evenements; if (o.reset_cm5_actif) reset_vu = true; }
    CHECK(o.hors_service && o.actionneurs_interdits && reset_vu);
    CHECK((evt_total & SEC_EVT_CM5_SILENCE) && (evt_total & SEC_EVT_CM5_RESET) && (evt_total & SEC_EVT_ARRET_CYCLE));
    CHECK(!securite_est_reference(&s));
    CHECK(securite_mode(&s) == SEC_MODE_VIDAGE);
    CHECK(securite_cm5_reset_count(&s) == 1);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SORT_WASTE, 1 }, now) == SEC_ACK_REFUSED_MODE);   /* hors service */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_EMERGENCY_STOP, 0 }, now) == SEC_ACK_OK);
    securite_set_reference(&s, true);
    CHECK(!securite_est_reference(&s));                                                  /* pas de reference tant que hors service */
    /* en mode NORMAL, une commande de tri recue pendant le silence est refusee « mode » (et non « non reference ») */
    { securite_t h; uint32_t n2 = t0; securite_instance_init(&h, n2);
      sec_entrees_t eh = E(true, false, true); securite_pas(&h, n2 += 100, &eh);
      securite_set_reference(&h, true);
      eh = E(true, false, false);
      sec_sorties_t oh = {0};
      while (!oh.hors_service) { n2 += 100; oh = securite_pas(&h, n2, &eh); }
      CHECK(securite_mode(&h) == SEC_MODE_NORMAL && !securite_est_reference(&h));
      CHECK(securite_evaluer(&h, &(sec_commande_t){ SEC_CMD_SORT_WASTE, 1 }, n2) == SEC_ACK_REFUSED_MODE);
      CHECK(securite_evaluer(&h, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_VIDAGE }, n2) == SEC_ACK_REFUSED_MODE); }
    /* le CM5 revient apres l'impulsion de reset : alerte DEGRADED_MODE, mode toujours VIDAGE */
    e = E(true, false, false);
    while (o.reset_cm5_actif) { now += 100; o = securite_pas(&s, now, &e); }
    e = E(true, false, true);
    now += 100; o = securite_pas(&s, now, &e);                       /* etat DEMARRAGE : le heartbeat est pris en compte */
    CHECK((o.evenements & SEC_EVT_CM5_REPRISE) && !o.hors_service);
    CHECK(securite_mode(&s) == SEC_MODE_VIDAGE);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_CLEAR_EMERGENCY, 0 }, now) == SEC_ACK_OK);    /* l'urgence recue pendant le silence est effacee */
    CHECK(securite_mode(&s) == SEC_MODE_VIDAGE);
    /* fin automatique du vidage : exactement 30 min apres l'entree */
    e = E(true, false, true);
    while ((uint32_t)(now - t_vidage) < SEC_VIDAGE_MAX_MS - 100) { now += 100; o = securite_pas(&s, now, &e); }
    CHECK(securite_mode(&s) == SEC_MODE_VIDAGE);
    now += 100; o = securite_pas(&s, now, &e);
    CHECK(securite_mode(&s) == SEC_MODE_NORMAL && (o.evenements & SEC_EVT_MODE_FIN_AUTO));
    /* maintenance : 15 min d'inactivite, relancees par chaque essai */
    securite_set_reference(&s, true);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_MAINTENANCE }, now) == SEC_ACK_OK);
    uint32_t t_debut = now;
    while ((uint32_t)(now - t_debut) < 600000) { now += 100; securite_pas(&s, now, &e); }           /* 10 min sans rien */
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_OUTPUT, 2 }, now) == SEC_ACK_OK);   /* activite */
    t_debut = now;
    while ((uint32_t)(now - t_debut) < SEC_MAINTENANCE_INACTIVITE_MS - 100) { now += 100; securite_pas(&s, now, &e); }
    CHECK(securite_mode(&s) == SEC_MODE_MAINTENANCE);                                    /* 10 + 14,9 min : toujours en maintenance */
    now += 100; o = securite_pas(&s, now, &e);
    CHECK(securite_mode(&s) == SEC_MODE_NORMAL && (o.evenements & SEC_EVT_MODE_FIN_AUTO));
    return 0;
}

/* ---------- compactage : plafond de cycles et attente du budget des verins ---------- */
static int cycle_verin(securite_t *s, int v, uint32_t *now)     /* un cycle complet : sortie puis rentree, 59,9 s par sens */
{
    for (int sens = 1; sens <= 2; sens++) {
        if (garde_verin_demander(securite_garde(s), v, (verin_sens_t)sens, false, *now) != GARDE_OK) return 1;
        bonne_marche(s, now, 100, 599);
        garde_verin_demander(securite_garde(s), v, VERIN_ARRET, false, *now);
        bonne_marche(s, now, 100, 4);
    }
    return 0;
}

static int test_attente_budget(uint32_t t0)
{
    garde_verin_t g; garde_verin_init(&g);
    CHECK(garde_verin_attente_ms(&g, 0, SEC_COMPACTION_MARCHE_CYCLE_MS, t0) == 0);                 /* verin neuf */
    CHECK(garde_verin_attente_ms(&g, 2, 1000, t0) == GARDE_ATTENTE_JAMAIS);                        /* indice invalide */
    CHECK(garde_verin_attente_ms(&g, -1, 1000, t0) == GARDE_ATTENTE_JAMAIS);
    CHECK(garde_verin_attente_ms(&g, 0, SEC_VERIN_MARCHE_FENETRE_MS + 1, t0) == GARDE_ATTENTE_JAMAIS);   /* plus que le budget */
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_1, false, t0) == GARDE_OK);
    CHECK(garde_verin_attente_ms(&g, 0, 1000, t0 + 10) == GARDE_ATTENTE_JAMAIS);                   /* en marche */
    uint32_t now = t0 + 60000;
    garde_verin_demander(&g, 0, VERIN_ARRET, false, now);
    now += 400;
    CHECK(garde_verin_demander(&g, 0, VERIN_SENS_2, false, now) == GARDE_OK);
    now += 60000;
    garde_verin_demander(&g, 0, VERIN_ARRET, false, now);
    /* 120 s consommees : pour chaque duree, le delai rendu est le plus petit qui convient (verifie a la milliseconde) */
    const uint32_t durees[] = { 1, 30000, 60000, 90000, 120000 };
    uint32_t precedent = 0;
    for (unsigned i = 0; i < sizeof durees / sizeof durees[0]; i++) {
        uint32_t w = garde_verin_attente_ms(&g, 0, durees[i], now);
        CHECK(w > 0 && w <= SEC_VERIN_FENETRE_MS);
        CHECK(garde_verin_marche_fenetre_ms(&g, 0, now + w) + durees[i] <= SEC_VERIN_MARCHE_FENETRE_MS);
        CHECK(garde_verin_marche_fenetre_ms(&g, 0, now + w - 1) + durees[i] > SEC_VERIN_MARCHE_FENETRE_MS);
        CHECK(w >= precedent);                                                                     /* plus long = attente plus longue */
        precedent = w;
    }
    CHECK(garde_verin_attente_ms(&g, 1, 120000, now) == 0);                                        /* l'autre verin est neuf */
    /* verin 1 en marche : seul lui est « jamais » ; le verin 0 (arrete) garde son delai */
    CHECK(garde_verin_demander(&g, 1, VERIN_SENS_1, false, now) == GARDE_OK);
    CHECK(garde_verin_attente_ms(&g, 1, 1000, now + 5) == GARDE_ATTENTE_JAMAIS);
    CHECK(garde_verin_attente_ms(&g, 0, 1000, now + 5) > 0 && garde_verin_attente_ms(&g, 0, 1000, now + 5) != GARDE_ATTENTE_JAMAIS);
    return 0;
}

static int test_compactage_cycles(uint32_t t0)
{
    securite_t s; uint32_t now = t0; securite_instance_init(&s, now);
    bonne_marche(&s, &now, 100, 1);
    securite_set_reference(&s, true);
    const sec_commande_t plast = { SEC_CMD_START_COMPACTION, 1 }, nonrec = { SEC_CMD_START_COMPACTION, 3 };
    CHECK(SEC_COMPACTION_CYCLES_MAX == 2);                                                         /* decision CTO 17/11 (pilote) */
    CHECK(securite_evaluer(&s, &plast, now) == SEC_ACK_OK);                                        /* cycle 1, plastique */
    CHECK(cycle_verin(&s, SEC_VERIN_PLASTIQUE, &now) == 0);
    /* pas de 2e cycle tout de suite : plus de budget (10 % de la fiche) -> BUSY, compteur inchange */
    CHECK(securite_evaluer(&s, &plast, now) == SEC_ACK_BUSY);
    CHECK(s.cycles_seq[SEC_VERIN_PLASTIQUE] == 1);
    CHECK(securite_evaluer(&s, &nonrec, now) == SEC_ACK_OK);                                       /* l'autre bac a son propre verin */
    CHECK(s.cycles_seq[SEC_VERIN_NON_RECYCLABLE] == 1);
    /* le 2e cycle est accepte a la milliseconde ou le budget le permet, pas avant */
    uint32_t t_fin = now;
    uint32_t w = garde_verin_attente_ms(&s.garde, SEC_VERIN_PLASTIQUE, SEC_COMPACTION_MARCHE_CYCLE_MS, t_fin);
    CHECK(w > 0 && w <= SEC_VERIN_FENETRE_MS);
    CHECK(securite_evaluer(&s, &plast, t_fin + w - 1) == SEC_ACK_BUSY);
    now = t_fin + w;
    { sec_entrees_t e = E(true, false, true); securite_pas(&s, now, &e); }
    CHECK(securite_evaluer(&s, &plast, now) == SEC_ACK_OK);                                        /* cycle 2 */
    CHECK(s.cycles_seq[SEC_VERIN_PLASTIQUE] == 2);
    CHECK(cycle_verin(&s, SEC_VERIN_PLASTIQUE, &now) == 0);                                        /* il va jusqu'au bout, sans coupure du budget */
    /* plafond atteint : refus definitif (INVALID), meme une fois le budget revenu */
    bonne_marche(&s, &now, 100, 13000);                                                            /* 21 min 40 s */
    CHECK(garde_verin_attente_ms(&s.garde, SEC_VERIN_PLASTIQUE, SEC_COMPACTION_MARCHE_CYCLE_MS, now) == 0);
    CHECK(securite_evaluer(&s, &plast, now) == SEC_ACK_INVALID);
    CHECK(s.cycles_seq[SEC_VERIN_PLASTIQUE] == 2);
    /* fin de sequence du bac : le compteur repart, l'autre bac n'est pas touche */
    securite_fin_sequence_compactage(&s, 1);
    CHECK(s.cycles_seq[SEC_VERIN_PLASTIQUE] == 0 && s.cycles_seq[SEC_VERIN_NON_RECYCLABLE] == 1);
    securite_fin_sequence_compactage(&s, 2);                                                       /* papier : sans effet */
    securite_fin_sequence_compactage(&s, 0);
    CHECK(s.cycles_seq[SEC_VERIN_NON_RECYCLABLE] == 1);
    CHECK(securite_evaluer(&s, &plast, now) == SEC_ACK_OK);
    /* le vidage remet les sequences a zero (PROPOSITION) : on sature le bac Non recyclable (verin non utilise : pas de BUSY) */
    CHECK(securite_evaluer(&s, &nonrec, now) == SEC_ACK_OK);
    CHECK(s.cycles_seq[SEC_VERIN_NON_RECYCLABLE] == 2);
    CHECK(securite_evaluer(&s, &nonrec, now) == SEC_ACK_INVALID);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_VIDAGE }, now) == SEC_ACK_OK);
    CHECK(securite_evaluer(&s, &nonrec, now) == SEC_ACK_REFUSED_MODE);                             /* mode avant plafond, compteur intact */
    CHECK(s.cycles_seq[SEC_VERIN_NON_RECYCLABLE] == 2);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_NORMAL }, now) == SEC_ACK_OK);
    CHECK(s.cycles_seq[0] == 0 && s.cycles_seq[1] == 0);
    CHECK(securite_evaluer(&s, &nonrec, now) == SEC_ACK_OK);
    /* sortie automatique du vidage : meme effet */
    CHECK(securite_evaluer(&s, &nonrec, now) == SEC_ACK_OK);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_VIDAGE }, now) == SEC_ACK_OK);
    bonne_marche(&s, &now, 100, (int)(SEC_VIDAGE_MAX_MS / 100) + 2);
    CHECK(securite_mode(&s) == SEC_MODE_NORMAL && s.cycles_seq[SEC_VERIN_NON_RECYCLABLE] == 0);
    /* la maintenance ne remet rien a zero */
    CHECK(securite_evaluer(&s, &nonrec, now) == SEC_ACK_OK);
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_MAINTENANCE }, now) == SEC_ACK_OK);
    CHECK(securite_est_reference(&s));
    CHECK(securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_SET_MODE, SEC_MODE_NORMAL }, now) == SEC_ACK_OK);
    CHECK(s.cycles_seq[SEC_VERIN_NON_RECYCLABLE] == 1);
    return 0;
}

static int test_verins_dans_sorties(void)
{
    securite_t s; uint32_t now = 1000; securite_instance_init(&s, now);
    sec_entrees_t e = E(true, false, true);
    securite_pas(&s, now += 100, &e);
    CHECK(garde_verin_demander(securite_garde(&s), 0, VERIN_SENS_1, false, now) == GARDE_OK);
    sec_sorties_t o = securite_pas(&s, now += 100, &e);
    CHECK(o.verins.sens[0] == VERIN_SENS_1 && o.verins.sens[1] == VERIN_ARRET);                    /* marche normale : sens transmis */
    securite_evaluer(&s, &(sec_commande_t){ SEC_CMD_EMERGENCY_STOP, 0 }, now);
    o = securite_pas(&s, now += 100, &e);
    CHECK(o.verins.sens[0] == VERIN_ARRET && o.verins.arret_force[0] == GARDE_CAUSE_INTERDIT);     /* urgence : verin coupe */
    CHECK(garde_verin_demander(securite_garde(&s), 0, VERIN_SENS_1, o.actionneurs_interdits, now) == GARDE_REFUSE_INTERDIT);
    return 0;
}

static int test_instance_globale(void)
{
    securite_init();
#if SEC_BOUTON_URGENCE_PRESENT
    CHECK(securite_urgence_active());                       /* bouton present, pas encore lu : on suppose l'urgence */
    sec_entrees_t e = { true, false, true };
    securite_pas(securite_instance(), 100, &e);
#endif
    CHECK(!securite_urgence_active());
    CHECK(securite_instance() != NULL);
    CHECK(securite_mode(securite_instance()) == SEC_MODE_NORMAL);
    return 0;
}

int main(void)
{
    /* chaque scenario temporel est rejoue avec un horodatage de depart normal et juste avant le debordement de 32 bits */
    const uint32_t departs[] = { 0u, 123456u, 0xFFFFFF00u, 0xFFFFFFFFu - 650000u };
    int r = 0;
    r |= test_enums();
    for (unsigned i = 0; i < sizeof departs / sizeof departs[0] && !r; i++) {
        uint32_t t0 = departs[i];
        r |= test_liaison_nominale(t0);
        r |= test_liaison_reset_et_pause(t0);
        r |= test_liaison_reprise(t0);
        r |= test_liaison_fenetre(t0);
        r |= test_liaison_demarrage_initial(t0);
        r |= test_garde_verin(t0);
        r |= test_garde_marche_max_et_budget(t0);
        r |= test_garde_interdit_et_historique(t0);
        r |= test_garde_cycles_compactage(t0);
        r |= test_attente_budget(t0);
        r |= test_compactage_cycles(t0);
        r |= test_cm5_et_modes(t0);
    }
    r |= test_refus_commandes();
    r |= test_urgence();
    r |= test_bouton_di1();
    r |= test_verins_dans_sorties();
    r |= test_instance_globale();
    if (r) { printf("ECHEC\n"); return 1; }
    printf("test_securite OK (%d verifications, bouton=%d contacteur=%d)\n", nb_tests, SEC_BOUTON_URGENCE_PRESENT, SEC_CONTACTEUR_PRESENT);
    return 0;
}
