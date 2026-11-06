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

import random
import socket
import threading
import time

import contrat_pb2
from RecyOS.transport import envoyer_message, recevoir_message

HOTE = "127.0.0.1"
PORT = 9000
PROTOCOL_VERSION = 2


def tache_envoi_periodique(sock: socket.socket) -> None:
    """
    Tourne en continu, en tâche de fond : envoie Heartbeat (2s) et
    SensorReport (500ms), comme le ferait le vrai M5Stack.
    """
    debut = time.monotonic()
    dernier_heartbeat = 0.0
    dernier_rapport = 0.0
    prochain_message_id = 1
    session_id = random.getrandbits(32) or 1  # tiré au sort à chaque démarrage

    while True:
        maintenant = time.monotonic() - debut

        if maintenant - dernier_rapport >= 0.5:
            dernier_rapport = maintenant
            enveloppe = contrat_pb2.Envelope(
                protocol_version=PROTOCOL_VERSION,
                message_id=prochain_message_id,
                session_id=session_id,
                device_uptime_ms=int(maintenant * 1000),
                sensor_report=contrat_pb2.SensorReport(
                    levels=[
                        contrat_pb2.BacLevel(bac=contrat_pb2.BAC_PLASTIQUE, fill_level_pct=37.5),
                        contrat_pb2.BacLevel(bac=contrat_pb2.BAC_PAPIER, fill_level_pct=12.0),
                        contrat_pb2.BacLevel(bac=contrat_pb2.BAC_NON_RECYCLABLE, fill_level_pct=55.0),
                    ],
                    bus_current_a=0.0,
                    battery_voltage_v=25.1,
                    trapdoor_closed=True,
                    valid_mask=(contrat_pb2.VALID_NIVEAU_PLASTIQUE
                                | contrat_pb2.VALID_NIVEAU_PAPIER
                                | contrat_pb2.VALID_NIVEAU_NON_RECYCLABLE
                                | contrat_pb2.VALID_COURANT_BUS
                                | contrat_pb2.VALID_TENSION_BATTERIE
                                | contrat_pb2.VALID_TRAPPE),
                ),
            )
            prochain_message_id += 1
            envoyer_message(sock, enveloppe)
            print(f"[faux M5Stack] SensorReport envoyé (id={enveloppe.message_id})")

        if maintenant - dernier_heartbeat >= 2.0:
            dernier_heartbeat = maintenant
            enveloppe = contrat_pb2.Envelope(
                protocol_version=PROTOCOL_VERSION,
                message_id=prochain_message_id,
                session_id=session_id,
                device_uptime_ms=int(maintenant * 1000),
                heartbeat=contrat_pb2.Heartbeat(
                    emergency_active=False,
                    self_test_ok=True,
                    firmware_version=0x00010000,
                ),
            )
            prochain_message_id += 1
            envoyer_message(sock, enveloppe)
            print(f"[faux M5Stack] Heartbeat envoyé (id={enveloppe.message_id})")

        time.sleep(0.05)  # évite de saturer le CPU en boucle serrée


def statut_pour(commande: "contrat_pb2.Command") -> int:
    """Règles du contrat v2 : une commande vide, incohérente ou interdite est refusée."""
    if commande.type == contrat_pb2.CMD_UNSPECIFIED:
        return contrat_pb2.ACK_INVALID
    if commande.WhichOneof("params") is None and commande.type != contrat_pb2.CMD_CLEAR_EMERGENCY:
        return contrat_pb2.ACK_INVALID
    attendu = {
        contrat_pb2.CMD_OPEN_TRAPDOOR: "open_trapdoor",
        contrat_pb2.CMD_START_COMPACTION: "start_compaction",
        contrat_pb2.CMD_SET_LED: "set_led",
        contrat_pb2.CMD_EMERGENCY_STOP: "emergency_stop",
        contrat_pb2.CMD_READ_SENSORS: "read_sensors",
        contrat_pb2.CMD_SET_CHARGE_PORT: "set_charge_port",
        contrat_pb2.CMD_CLEAR_EMERGENCY: "clear_emergency",
        contrat_pb2.CMD_SET_TIME: "set_time",
    }
    if commande.WhichOneof("params") != attendu.get(commande.type):
        return contrat_pb2.ACK_INVALID
    if commande.type == contrat_pb2.CMD_OPEN_TRAPDOOR and commande.open_trapdoor.bac == contrat_pb2.BAC_UNSPECIFIED:
        return contrat_pb2.ACK_INVALID
    if commande.type == contrat_pb2.CMD_START_COMPACTION and commande.start_compaction.bac in (
            contrat_pb2.BAC_UNSPECIFIED, contrat_pb2.BAC_PAPIER):
        return contrat_pb2.ACK_INVALID  # le bac Papier n'est pas compacté
    return contrat_pb2.ACK_OK


def tache_reception(sock: socket.socket) -> None:
    """
    Tourne en continu, en tâche de fond : écoute les Command envoyées par
    RecyOS, et répond systématiquement par un Ack (ACK_OK pour l'instant,
    la vraie logique de refus viendra avec les capteurs/actionneurs réels).
    """
    session_ack = random.getrandbits(32) or 1
    compteur_ack = 1
    while True:
        try:
            enveloppe_recue = recevoir_message(sock)
        except (ConnectionError, ValueError) as erreur:
            print(f"[faux M5Stack] Réception interrompue : {erreur}")
            return

        if enveloppe_recue.HasField("command"):
            print(f"[faux M5Stack] Command reçue : {enveloppe_recue.command}")
            ack = contrat_pb2.Envelope(
                protocol_version=PROTOCOL_VERSION,
                message_id=compteur_ack,
                session_id=session_ack,
                ack=contrat_pb2.Ack(
                    acked_message_id=enveloppe_recue.message_id,
                    status=statut_pour(enveloppe_recue.command),
                ),
            )
            compteur_ack += 1
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