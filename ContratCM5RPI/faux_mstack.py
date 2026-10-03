"""
faux_m5stack.py — Simulateur du M5Stack/StamPLC, pour développer et tester
le M5Stack Bridge (côté RPi) sans matériel, et comme référence exécutable
du contrat pour qui écrira le vrai firmware ESP32.

Rôle : se comporte comme le VRAI M5Stack est censé se comporter, d'après
contrat.proto :
  - envoie un Heartbeat toutes les 2 secondes
  - envoie un SensorReport toutes les 500 ms
  - répond à toute Command reçue par un Ack
"""

import socket
import threading
import time

import contrat_pb2
from RecyOS.transport import envoyer_message, recevoir_message

HOTE = "127.0.0.1"
PORT = 9000


def tache_envoi_periodique(sock: socket.socket) -> None:
    """
    Tourne en continu, en tâche de fond : envoie Heartbeat (2s) et
    SensorReport (500ms), comme le ferait le vrai M5Stack.
    """
    debut = time.monotonic()
    dernier_heartbeat = 0.0
    dernier_rapport = 0.0
    prochain_message_id = 1

    while True:
        maintenant = time.monotonic() - debut

        if maintenant - dernier_rapport >= 0.5:
            dernier_rapport = maintenant
            enveloppe = contrat_pb2.Envelope(
                protocol_version=1,
                message_id=prochain_message_id,
                session_id=42,  # fixe pour l'instant, sera aléatoire plus tard
                device_uptime_ms=int(maintenant * 1000),
                sensor_report=contrat_pb2.SensorReport(
                    bac_id=1,
                    fill_level_pct=37.5,   # valeur fixe pour l'instant, juste pour tester
                    motor_current_a=0.0,
                    trapdoor_closed=True,
                    battery_voltage=25.1,
                ),
            )
            prochain_message_id += 1
            envoyer_message(sock, enveloppe)
            print(f"[faux M5Stack] SensorReport envoyé (id={enveloppe.message_id})")

        if maintenant - dernier_heartbeat >= 2.0:
            dernier_heartbeat = maintenant
            enveloppe = contrat_pb2.Envelope(
                protocol_version=1,
                message_id=prochain_message_id,
                session_id=42,
                device_uptime_ms=int(maintenant * 1000),
                heartbeat=contrat_pb2.Heartbeat(
                    emergency_active=False,
                    self_test_ok=True,
                ),
            )
            prochain_message_id += 1
            envoyer_message(sock, enveloppe)
            print(f"[faux M5Stack] Heartbeat envoyé (id={enveloppe.message_id})")

        time.sleep(0.05)  # évite de saturer le CPU en boucle serrée


def tache_reception(sock: socket.socket) -> None:
    """
    Tourne en continu, en tâche de fond : écoute les Command envoyées par
    RecyOS, et répond systématiquement par un Ack (ACK_OK pour l'instant,
    la vraie logique de refus viendra avec les capteurs/actionneurs réels).
    """
    while True:
        try:
            enveloppe_recue = recevoir_message(sock)
        except (ConnectionError, ValueError) as erreur:
            print(f"[faux M5Stack] Réception interrompue : {erreur}")
            return

        if enveloppe_recue.HasField("command"):
            print(f"[faux M5Stack] Command reçue : {enveloppe_recue.command}")
            ack = contrat_pb2.Envelope(
                protocol_version=1,
                message_id=enveloppe_recue.message_id + 100000,  # provisoire, évite les collisions
                session_id=42,
                ack=contrat_pb2.Ack(
                    acked_message_id=enveloppe_recue.message_id,
                    status=contrat_pb2.ACK_OK,
                ),
            )
            envoyer_message(sock, ack)
            print(f"[faux M5Stack] Ack envoyé pour message_id={enveloppe_recue.message_id}")


def main() -> None:
    serveur = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serveur.bind((HOTE, PORT))
    serveur.listen(1)
    print(f"[faux M5Stack] En attente de connexion sur {HOTE}:{PORT}...")

    connexion, adresse = serveur.accept()
    print(f"[faux M5Stack] RecyOS connecté depuis {adresse}")

    threading.Thread(target=tache_envoi_periodique, args=(connexion,), daemon=True).start()
    tache_reception(connexion)  # occupe le thread principal


if __name__ == "__main__":
    main()