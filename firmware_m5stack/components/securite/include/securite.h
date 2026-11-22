#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "config_securite.h"
#include "liaison_cm5.h"
#include "garde_verin.h"

/* Module de securite du StamPLC : arret d'urgence, modes, regles de refus des commandes, surveillance du CM5,
 * garde-fous des verins. Aucun acces materiel : le temps et les entrees sont fournis par l'appelant, les ordres
 * sortent dans des structures. Toutes les fonctions doivent etre appelees depuis la MEME tache (pas de verrou).
 * Les valeurs numeriques des enumerations reprennent celles de contrat.proto (v3) ; test_securite.c les verifie. */

typedef enum { SEC_MODE_NORMAL = 1, SEC_MODE_VIDAGE = 2, SEC_MODE_MAINTENANCE = 3 } sec_mode_t;     /* = OperatingMode */

typedef enum {                                                  /* = AckStatus */
    SEC_ACK_OK = 1,
    SEC_ACK_REFUSED_EMERGENCY = 2,
    SEC_ACK_INVALID = 3,
    SEC_ACK_BUSY = 4,
    SEC_ACK_REFUSED_MODE = 6,
    SEC_ACK_REFUSED_NOT_HOMED = 7
} sec_ack_t;

typedef enum {                                                  /* = CommandType */
    SEC_CMD_NONE = 0,
    SEC_CMD_START_COMPACTION = 2, SEC_CMD_SET_LED = 3, SEC_CMD_EMERGENCY_STOP = 4, SEC_CMD_READ_SENSORS = 5,
    SEC_CMD_SET_CHARGE_PORT = 6, SEC_CMD_CLEAR_EMERGENCY = 7, SEC_CMD_SET_TIME = 8, SEC_CMD_SORT_WASTE = 9,
    SEC_CMD_HOME = 10, SEC_CMD_SET_MODE = 11, SEC_CMD_TEST_ACTUATOR = 12, SEC_CMD_SET_OUTPUT = 13,
    SEC_CMD_ABORT_CYCLE = 14
} sec_cmd_t;

typedef struct {
    sec_cmd_t type;
    int param;      /* SORT_WASTE / START_COMPACTION : Bac (1 a 3) ; SET_MODE : mode (1 a 3) ;
                       TEST_ACTUATOR : Actuator (1 a 4) ; SET_OUTPUT : OutputId (2 seul accepte : 1 = ventilateur, refuse). 0 = non specifie. */
} sec_commande_t;

typedef struct {
    bool di1_circuit_ferme;     /* contact NF du bouton d'arret d'urgence ferme = sain (le pilote traduit la polarite des DI). Ignore si SEC_BOUTON_URGENCE_PRESENT = 0. */
    bool cycle_actif;           /* un tri ou un compactage est en cours */
    bool heartbeat_cm5_recu;    /* au moins un heartbeat CM5 valide depuis l'appel precedent */
} sec_entrees_t;

enum {                          /* evenements a publier (une fois) */
    SEC_EVT_URGENCE_DECLENCHEE = 1u << 0,   /* alerte EMERGENCY_TRIGGERED */
    SEC_EVT_URGENCE_EFFACEE    = 1u << 1,   /* alerte EMERGENCY_CLEARED */
    SEC_EVT_CM5_SILENCE        = 1u << 2,
    SEC_EVT_CM5_RESET          = 1u << 3,
    SEC_EVT_CM5_PAUSE          = 1u << 4,   /* 3 resets utilises : evenement vers le cloud */
    SEC_EVT_CM5_REPRISE        = 1u << 5,   /* alerte DEGRADED_MODE a la reprise */
    SEC_EVT_MODE_FIN_AUTO      = 1u << 6,   /* sortie automatique de VIDAGE ou MAINTENANCE */
    SEC_EVT_ARRET_CYCLE        = 1u << 7,   /* le tri / compactage en cours doit s'arreter */
    SEC_EVT_HOMING_DEMANDE     = 1u << 8,
    SEC_EVT_MODE_CHANGE        = 1u << 9    /* publier ActivityStatus */
};

typedef struct {
    bool actionneurs_interdits; /* niveau : urgence ou CM5 hors service. Rien ne doit bouger ; arreter ce qui bouge. */
    bool hors_service;          /* afficher « Hors service » sur le LCD du StamPLC (CM5 muet) */
    bool reset_cm5_actif;       /* niveau de la broche de reset du CM5 (G41 -> NPN) ; repos = false */
    bool coupure_puissance;     /* DO4 : seulement si SEC_CONTACTEUR_PRESENT ; sinon toujours false */
    bool urgence_active;        /* verrou d'arret d'urgence */
    uint32_t evenements;        /* masque SEC_EVT_* */
    garde_verin_sortie_t verins;/* sens a appliquer aux relais (0 = arret) et verins coupes par la garde */
} sec_sorties_t;

typedef struct {
    bool urgence;               /* verrou */
    bool di1_sain;
    bool homed;                 /* chariot reference */
    bool cycle_actif;
    sec_mode_t mode;
    uint32_t t_mode_ms;         /* entree en VIDAGE, ou derniere activite en MAINTENANCE */
    uint32_t evt;               /* evenements en attente, rendus par securite_pas */
    liaison_cm5_t cm5;
    bool hors_service;
    garde_verin_t garde;        /* marche des verins ; le pilote de relais passe par securite_garde() */
    uint8_t cycles_seq[2];      /* cycles de compactage acceptes dans la sequence en cours, par verin */
} securite_t;

void securite_instance_init(securite_t *s, uint32_t now_ms);
/* Reste du contrat sans etat : verifie la commande et applique ses effets sur l'etat de securite.
 * Ne demarre rien : logique.c lance ensuite tri_demarrer() si la reponse est SEC_ACK_OK. */
sec_ack_t securite_evaluer(securite_t *s, const sec_commande_t *c, uint32_t now_ms);
sec_sorties_t securite_pas(securite_t *s, uint32_t now_ms, const sec_entrees_t *in);
/* Le homing (M18, DI6) est termine : le chariot est reference. */
void securite_set_reference(securite_t *s, bool fait);
bool securite_est_reference(const securite_t *s);
sec_mode_t securite_mode(const securite_t *s);
uint32_t securite_cm5_reset_count(const securite_t *s);
garde_verin_t *securite_garde(securite_t *s);
/* Fin de la sequence de compactage d'un bac (1 Plastique, 3 Non recyclable) : remet son compteur de cycles a zero. */
void securite_fin_sequence_compactage(securite_t *s, int bac);

/* Instance unique de l'application (main.c). */
void securite_init(void);
bool securite_urgence_active(void);
securite_t *securite_instance(void);
