Tests unitaires (framework Unity d'ESP-IDF) : a ajouter par composant.

Tests sur PC (sans ESP-IDF), depuis firmware_m5stack/ :
- securite : voir docs/Securite_StamPLC_2026-11-17.md (deux configurations de compilation) ; `python3 test/verifier_enums_securite.py` compare les valeurs des enumerations a contrat.proto.
- simulateur du CM5 : depuis la racine du depot, `PYTHONPATH=.:ContratCM5RPI python3 -m unittest test_regles_v3`.
