"""
test_client_recyos.py — Petit client de test qui se fait passer pour RecyOS.

But : se connecter au faux M5Stack, afficher tout ce qu'il reçoit
(Heartbeat, SensorReport), et envoyer une Command de test pour vérifier
qu'un Ack revient bien.

Ce n'est PAS encore le vrai M5Stack Bridge (module 2) — juste un test
manuel pour valider que transport.py fonctionne dans les deux sens.
"""

import socket
import threading
import time

import contrat_pb2
from RecyOS.transport import envoyer_message, recevoir_message

HOTE = "127.0.0.1"
PORT = 9000


def tache_ecoute(sock: socket.socket) -> None:
    """Affiche en continu tout ce que le faux M5Stack envoie."""
    while True:
        try:
            enveloppe = recevoir_message(sock)
        except (ConnectionError, ValueError) as erreur:
            print(f"[RecyOS test] Réception interrompue : {erreur}")
            return

        # "WhichOneof" dit quel champ du oneof est rempli, sans avoir à
        # tester HasField sur chacun des 5 champs possibles un par un.
        type_recu = enveloppe.WhichOneof("payload")
        print(f"[RecyOS test] Reçu : {type_recu} (message_id={enveloppe.message_id})")

        if type_recu == "sensor_report":
            print(f"    -> niveau du bac : {enveloppe.sensor_report.fill_level_pct}%")
        elif type_recu == "heartbeat":
            print(f"    -> self_test_ok : {enveloppe.heartbeat.self_test_ok}")
        elif type_recu == "ack":
            print(f"    -> ack pour message_id={enveloppe.ack.acked_message_id}, "
                  f"status={contrat_pb2.AckStatus.Name(enveloppe.ack.status)}")


def main() -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOTE, PORT))
    print("[RecyOS test] Connecté au faux M5Stack.")

    threading.Thread(target=tache_ecoute, args=(sock,), daemon=True).start()

    # On laisse le temps de recevoir quelques Heartbeat/SensorReport
    # avant d'envoyer une commande de test.
    time.sleep(3)

    print("[RecyOS test] Envoi d'une Command de test (CMD_READ_SENSORS)...")
    commande = contrat_pb2.Envelope(
        protocol_version=1,
        message_id=1,
        session_id=99,
        command=contrat_pb2.Command(
            type=contrat_pb2.CMD_READ_SENSORS,
            read_sensors=contrat_pb2.ReadSensors(),
        ),
    )
    envoyer_message(sock, commande)

    # On reste actif encore un peu pour voir l'Ack revenir, puis on arrête.
    time.sleep(3)
    print("[RecyOS test] Fin du test.")


if __name__ == "__main__":
    main()