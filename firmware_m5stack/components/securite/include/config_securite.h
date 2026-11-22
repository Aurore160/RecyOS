#pragma once
/*
 * Parametres de securite. Chaque valeur porte son statut :
 *   [SPEC]  = decision ecrite (specification CM5 v0.6, decisions P1 a P8, Option B v0.14) ;
 *   [SUPPOSE] = valeur supposee, a mesurer au banc ;
 *   [PROPOSITION] = choix de conception de ce module, a valider par le CTO.
 * Toutes les durees sont en millisecondes. Chaque macro peut etre surchargee avant l'inclusion (tests).
 */

/* ---- Liaison avec le CM5 (spec v0.6 section 5, decision P1/P4) ---- */
#ifndef SEC_CM5_SILENCE_MS
#define SEC_CM5_SILENCE_MS          5000UL      /* [SPEC] silence declare apres 5 s sans heartbeat */
#endif
#ifndef SEC_CM5_DEMARRAGE_MS
#define SEC_CM5_DEMARRAGE_MS        90000UL     /* [SUPPOSE] delai de demarrage du CM5, a mesurer */
#endif
#ifndef SEC_CM5_DEMARRAGE_INITIAL_MS
#define SEC_CM5_DEMARRAGE_INITIAL_MS 90000UL    /* [PROPOSITION] meme delai a la mise sous tension du StamPLC (le CM5 demarre aussi) */
#endif
#ifndef SEC_CM5_RESETS_MAX
#define SEC_CM5_RESETS_MAX          3           /* [SPEC] 3 resets au plus ... */
#endif
#ifndef SEC_CM5_FENETRE_RESETS_MS
#define SEC_CM5_FENETRE_RESETS_MS   600000UL    /* [SPEC] ... en 10 minutes */
#endif
#ifndef SEC_CM5_PAUSE_MS
#define SEC_CM5_PAUSE_MS            1800000UL   /* [SPEC] puis pause de 30 minutes */
#endif
#ifndef SEC_CM5_RESET_IMPULSION_MS
#define SEC_CM5_RESET_IMPULSION_MS  500UL       /* [SUPPOSE] duree de l'impulsion sur G41, a mesurer (essai 1 Option B) */
#endif

/* ---- Arret d'urgence (decisions P2, P3 ; Option B decision D1) ---- */
#ifndef SEC_BOUTON_URGENCE_PRESENT
#define SEC_BOUTON_URGENCE_PRESENT  0           /* [SPEC D1] pilote sans arret d'urgence : DI1 ignoree. Mettre 1 des que le bouton est cable. */
#endif
#ifndef SEC_CONTACTEUR_PRESENT
#define SEC_CONTACTEUR_PRESENT      0           /* [SPEC D1] pas de contacteur : DO4 n'est pas commandee */
#endif

/* ---- Modes (spec v0.6 section 5 et 10) ---- */
#ifndef SEC_VIDAGE_MAX_MS
#define SEC_VIDAGE_MAX_MS           1800000UL   /* [SUPPOSE] 30 min */
#endif
#ifndef SEC_MAINTENANCE_INACTIVITE_MS
#define SEC_MAINTENANCE_INACTIVITE_MS 900000UL  /* [SUPPOSE] 15 min d'inactivite */
#endif

/* ---- Verins YH8-523D (Option B v0.14, chapitre 4 ; decision D6) ---- */
#ifndef SEC_VERIN_PAUSE_INVERSION_MS
#define SEC_VERIN_PAUSE_INVERSION_MS 300UL      /* [SPEC] arret, attente environ 300 ms, inversion */
#endif
#ifndef SEC_VERIN_MARCHE_MAX_MS
#define SEC_VERIN_MARCHE_MAX_MS     60000UL     /* [SUPPOSE] 60 s par sens pour 300 mm (course = hypothese D6) */
#endif
#ifndef SEC_VERIN_FENETRE_MS
#define SEC_VERIN_FENETRE_MS        1200000UL   /* [SPEC D6] fenetre glissante de 20 min ... */
#endif
/* ---- Compactage (spec CM5 v0.6 regle 6 : 3 cycles au plus par sequence ; pilote : 2, decision CTO du 17/11) ---- */
#ifndef SEC_COMPACTION_CYCLES_MAX
#define SEC_COMPACTION_CYCLES_MAX   2U          /* [DECISION CTO 17/11, pilote] plafond StamPLC ; la spec CM5 fixe 3 au maximum */
#endif
#ifndef SEC_COMPACTION_MARCHE_CYCLE_MS
#define SEC_COMPACTION_MARCHE_CYCLE_MS (2UL * SEC_VERIN_MARCHE_MAX_MS)  /* un cycle = sortie + rentree, a pleine course */
#endif
#ifndef SEC_VERIN_PLASTIQUE
#define SEC_VERIN_PLASTIQUE         0           /* [CHOIX INTERNE] indice du verin du bac Plastique (le pilote de relais utilise le meme) */
#endif
#ifndef SEC_VERIN_NON_RECYCLABLE
#define SEC_VERIN_NON_RECYCLABLE    1           /* [CHOIX INTERNE] indice du verin du bac Non recyclable */
#endif
#ifndef SEC_VERIN_MARCHE_FENETRE_MS
#define SEC_VERIN_MARCHE_FENETRE_MS 120000UL    /* [SPEC D6] ... 120 s de marche par verin (rapport cyclique 10 % de l'etiquette) */
#endif
