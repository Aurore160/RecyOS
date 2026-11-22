#include "securite.h"

static uint32_t ecoule(uint32_t now, uint32_t t) { return now - t; }

void securite_instance_init(securite_t *s, uint32_t now_ms)
{
    *s = (securite_t){0};
    s->mode = SEC_MODE_NORMAL;
    s->t_mode_ms = now_ms;
    s->homed = false;                       /* jamais reference au demarrage */
    s->di1_sain = !SEC_BOUTON_URGENCE_PRESENT;  /* bouton present : suppose non sain jusqu'a la premiere lecture */
    liaison_cm5_init(&s->cm5, now_ms);
    garde_verin_init(&s->garde);
}

/* PROPOSITION : le vidage vide les bacs, donc les sequences de compactage repartent de zero. */
static void raz_sequences(securite_t *s) { s->cycles_seq[0] = 0; s->cycles_seq[1] = 0; }
void securite_fin_sequence_compactage(securite_t *s, int bac)
{
    if (bac == 1) s->cycles_seq[SEC_VERIN_PLASTIQUE] = 0;
    else if (bac == 3) s->cycles_seq[SEC_VERIN_NON_RECYCLABLE] = 0;
}

/* Urgence effective : verrou, ou bouton present et ouvert (meme avant le premier cycle de securite_pas). */
static bool urgence_effective(const securite_t *s) { return s->urgence || (SEC_BOUTON_URGENCE_PRESENT && !s->di1_sain); }

static void verrouiller_urgence(securite_t *s)
{
    if (!s->urgence) { s->urgence = true; s->evt |= SEC_EVT_URGENCE_DECLENCHEE; }
    s->homed = false;                       /* tout arret impose un nouveau HOME (P3) */
    s->evt |= SEC_EVT_ARRET_CYCLE;
}

static bool valeur_dans(int v, int mini, int maxi) { return v >= mini && v <= maxi; }

sec_ack_t securite_evaluer(securite_t *s, const sec_commande_t *c, uint32_t now)
{
    switch (c->type) {
    case SEC_CMD_EMERGENCY_STOP:                    /* toujours accepte */
        verrouiller_urgence(s);
        return SEC_ACK_OK;

    case SEC_CMD_CLEAR_EMERGENCY:                   /* P3 : DI1 rearme ET cette commande */
        if (!s->urgence) return SEC_ACK_OK;         /* rien a effacer */
        if (SEC_BOUTON_URGENCE_PRESENT && !s->di1_sain) return SEC_ACK_REFUSED_EMERGENCY;
        s->urgence = false;
        s->homed = false;
        s->evt |= SEC_EVT_URGENCE_EFFACEE | SEC_EVT_MODE_CHANGE;
        return SEC_ACK_OK;

    case SEC_CMD_READ_SENSORS: case SEC_CMD_SET_TIME: case SEC_CMD_SET_LED:
        return SEC_ACK_OK;

    case SEC_CMD_START_COMPACTION: case SEC_CMD_SET_CHARGE_PORT: case SEC_CMD_SORT_WASTE:
    case SEC_CMD_HOME: case SEC_CMD_SET_MODE: case SEC_CMD_TEST_ACTUATOR: case SEC_CMD_SET_OUTPUT:
    case SEC_CMD_ABORT_CYCLE:
        break;

    default:
        return SEC_ACK_INVALID;                     /* type inconnu ou non specifie */
    }

    if (urgence_effective(s)) return SEC_ACK_REFUSED_EMERGENCY;
    /* CM5 hors service : aucune commande ne devrait arriver ; si l'une arrive, on ne bouge rien (PROPOSITION : ACK_REFUSED_MODE). */
    if (s->hors_service) return SEC_ACK_REFUSED_MODE;

    switch (c->type) {
    case SEC_CMD_SET_MODE:
        if (!valeur_dans(c->param, SEC_MODE_NORMAL, SEC_MODE_MAINTENANCE)) return SEC_ACK_INVALID;
        if (s->cycle_actif) return SEC_ACK_BUSY;
        /* Spec v0.6 §8 : retour en NORMAL depuis la maintenance seulement apres HOME. */
        if (s->mode == SEC_MODE_MAINTENANCE && c->param == SEC_MODE_NORMAL && !s->homed) return SEC_ACK_REFUSED_NOT_HOMED;
        if (s->mode == SEC_MODE_VIDAGE && c->param != SEC_MODE_VIDAGE) raz_sequences(s);
        if ((sec_mode_t)c->param != s->mode) s->evt |= SEC_EVT_MODE_CHANGE;
        s->mode = (sec_mode_t)c->param;
        s->t_mode_ms = now;
        return SEC_ACK_OK;

    case SEC_CMD_ABORT_CYCLE:
        s->evt |= SEC_EVT_ARRET_CYCLE;
        return SEC_ACK_OK;

    case SEC_CMD_SET_CHARGE_PORT:
        return SEC_ACK_OK;

    case SEC_CMD_HOME:
        if (s->cycle_actif) return SEC_ACK_BUSY;
        s->evt |= SEC_EVT_HOMING_DEMANDE;           /* homed ne passe a vrai que par securite_set_reference() */
        return SEC_ACK_OK;

    case SEC_CMD_SET_OUTPUT: case SEC_CMD_TEST_ACTUATOR: {
        if (s->mode != SEC_MODE_MAINTENANCE) return SEC_ACK_REFUSED_MODE;
        int maxi = (c->type == SEC_CMD_TEST_ACTUATOR) ? 4 : 2;      /* Actuator 1..4, OutputId 1..2 */
        if (!valeur_dans(c->param, 1, maxi)) return SEC_ACK_INVALID;
        if (c->type == SEC_CMD_SET_OUTPUT && c->param == 1) return SEC_ACK_INVALID;   /* OUTPUT_FAN : le ventilateur (DO1) n'obeit qu'au StamPLC (decision 17/11) */
        if (c->type == SEC_CMD_TEST_ACTUATOR && c->param == 1) s->homed = false;   /* ACTUATOR_CARRIAGE : le chariot bouge, la reference est perdue */
        s->t_mode_ms = now;                          /* activite : relance les 15 min d'inactivite */
        return SEC_ACK_OK;
    }

    default: break;                                  /* SORT_WASTE, START_COMPACTION */
    }

    /* SORT_WASTE et START_COMPACTION : parametre, mode normal, reference, libre (meme ordre que faux_m5stack.py) */
    if (c->type == SEC_CMD_SORT_WASTE && !valeur_dans(c->param, 1, 3)) return SEC_ACK_INVALID;
    if (c->type == SEC_CMD_START_COMPACTION && c->param != 1 && c->param != 3) return SEC_ACK_INVALID;   /* papier (2) non compacte */
    if (s->mode != SEC_MODE_NORMAL) return SEC_ACK_REFUSED_MODE;
    if (!s->homed) return SEC_ACK_REFUSED_NOT_HOMED;
    if (s->cycle_actif) return SEC_ACK_BUSY;
    if (c->type == SEC_CMD_START_COMPACTION) {
        int v = (c->param == 1) ? SEC_VERIN_PLASTIQUE : SEC_VERIN_NON_RECYCLABLE;
        /* PROPOSITION : aucun code d'ack n'est prevu par le contrat. INVALID = definitif (plafond atteint), BUSY = temporaire (verin a refroidir). */
        if (s->cycles_seq[v] >= SEC_COMPACTION_CYCLES_MAX) return SEC_ACK_INVALID;
        if (garde_verin_attente_ms(&s->garde, v, SEC_COMPACTION_MARCHE_CYCLE_MS, now) != 0) return SEC_ACK_BUSY;
        s->cycles_seq[v]++;
    }
    return SEC_ACK_OK;
}

sec_sorties_t securite_pas(securite_t *s, uint32_t now, const sec_entrees_t *in)
{
    sec_sorties_t o = {0};
    s->cycle_actif = in->cycle_actif;
    s->di1_sain = SEC_BOUTON_URGENCE_PRESENT ? in->di1_circuit_ferme : true;
    if (SEC_BOUTON_URGENCE_PRESENT && !s->di1_sain) verrouiller_urgence(s);

    liaison_cm5_sortie_t l = liaison_cm5_pas(&s->cm5, now, in->heartbeat_cm5_recu);
    if (l.evt_silence) {
        s->evt |= SEC_EVT_CM5_SILENCE | SEC_EVT_ARRET_CYCLE;
        s->homed = false;      /* PROPOSITION : actionneurs coupes en cours de route, la reference n'est plus sure */
    }
    if (l.evt_reset)   s->evt |= SEC_EVT_CM5_RESET;
    if (l.evt_pause)   s->evt |= SEC_EVT_CM5_PAUSE;
    if (l.evt_reprise) s->evt |= SEC_EVT_CM5_REPRISE;
    s->hors_service = l.hors_service;

    /* Fin automatique des modes (le StamPLC garde son mode pendant un reset du CM5 ; seule la duree le ramene a NORMAL). */
    if ((s->mode == SEC_MODE_VIDAGE && ecoule(now, s->t_mode_ms) >= SEC_VIDAGE_MAX_MS) ||
        (s->mode == SEC_MODE_MAINTENANCE && ecoule(now, s->t_mode_ms) >= SEC_MAINTENANCE_INACTIVITE_MS)) {
        if (s->mode == SEC_MODE_VIDAGE) raz_sequences(s);
        s->mode = SEC_MODE_NORMAL;
        s->t_mode_ms = now;
        s->evt |= SEC_EVT_MODE_FIN_AUTO | SEC_EVT_MODE_CHANGE;
    }

    o.urgence_active = s->urgence;
    o.hors_service = s->hors_service;
    o.actionneurs_interdits = urgence_effective(s) || s->hors_service;
    o.reset_cm5_actif = l.reset_actif;
    o.coupure_puissance = SEC_CONTACTEUR_PRESENT ? urgence_effective(s) : false;
    o.verins = garde_verin_pas(&s->garde, now, o.actionneurs_interdits);
    o.evenements = s->evt;
    s->evt = 0;
    return o;
}

void securite_set_reference(securite_t *s, bool fait) { s->homed = fait && !urgence_effective(s) && !s->hors_service; }
bool securite_est_reference(const securite_t *s) { return s->homed; }
sec_mode_t securite_mode(const securite_t *s) { return s->mode; }
garde_verin_t *securite_garde(securite_t *s) { return &s->garde; }
uint32_t securite_cm5_reset_count(const securite_t *s) { return s->cm5.cm5_reset_count; }

/* ---- Instance unique de l'application ---- */
static securite_t s_instance;
void securite_init(void) { securite_instance_init(&s_instance, 0); }
bool securite_urgence_active(void) { return urgence_effective(&s_instance); }
securite_t *securite_instance(void) { return &s_instance; }
