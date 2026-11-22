"""Point d'entrée de RecyOS (côté Raspberry Pi CM5) — en construction.

Le pont vers le StamPLC (RecyOS/m5stack_bridge.py) reste à écrire.
En attendant, pour tester le transport sans matériel :

    python ContratCM5RPI/faux_m5stack.py        # simulateur du StamPLC (terminal 1)
    python ContratCM5RPI/test_client_recyos.py  # client de test (terminal 2)
"""

if __name__ == "__main__":
    print(__doc__)
