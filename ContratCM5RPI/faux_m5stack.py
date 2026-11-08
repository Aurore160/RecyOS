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
PROTOCOL_VERSION = 3


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
                    source=contrat_pb2.HEARTBEAT_SOURCE_STAMPLC,
                    mode=_ETAT.mode,
                ),
            )
            prochain_message_id += 1
            envoyer_message(sock, enveloppe)
            print(f"[faux M5Stack] Heartbeat envoyé (id={enveloppe.message_id})")

        time.sleep(0.05)  # évite de saturer le CPU en boucle serrée


class EtatSimule:
    """État minimal du StamPLC simulé : applique les règles de refus du contrat v3.

    Principe : le CM5 décide QUAND, le StamPLC décide COMMENT et refuse.
    """

    ATTENDU = {
        contrat_pb2.CMD_START_COMPACTION: "start_compaction",
        contrat_pb2.CMD_SET_LED: "set_led",
        contrat_pb2.CMD_EMERGENCY_STOP: "emergency_stop",
        contrat_pb2.CMD_READ_SENSORS: "read_sensors",
        contrat_pb2.CMD_SET_CHARGE_PORT: "set_charge_port",
        contrat_pb2.CMD_CLEAR_EMERGENCY: "clear_emergency",
        contrat_pb2.CMD_SET_TIME: "set_time",
        contrat_pb2.CMD_SORT_WASTE: "sort_waste",
        contrat_pb2.CMD_HOME: "home",
        contrat_pb2.CMD_SET_MODE: "set_mode",
        contrat_pb2.CMD_TEST_ACTUATOR: "test_actuator",
        contrat_pb2.CMD_SET_OUTPUT: "set_output",
        contrat_pb2.CMD_ABORT_CYCLE: "abort_cycle",
    }

    def __init__(self) -> None:
        self.mode = contrat_pb2.MODE_NORMAL
        self.homed = False
        self.urgence = False
        self.cycle_en_cours = False

    def appliquer(self, c: "contrat_pb2.Command") -> int:
        ack = contrat_pb2
        if c.type == ack.CMD_UNSPECIFIED:
            return ack.ACK_INVALID
        if c.WhichOneof("params") != self.ATTENDU.get(c.type):
            # seules les commandes sans paramètre utile peuvent arriver sans oneof
            return ack.ACK_INVALID
        t = c.type
        # arrêt d'urgence : toujours accepté ; le reste est refusé tant qu'il est actif
        if t == ack.CMD_EMERGENCY_STOP:
            self.urgence = True
            self.cycle_en_cours = False
            self.homed = False  # un arrêt impose un nouveau référencement
            return ack.ACK_OK
        if t == ack.CMD_CLEAR_EMERGENCY:
            self.urgence = False
            return ack.ACK_OK
        if t in (ack.CMD_READ_SENSORS, ack.CMD_SET_TIME, ack.CMD_SET_LED):
            return ack.ACK_OK
        if self.urgence:
            return ack.ACK_REFUSED_EMERGENCY
        if t == ack.CMD_SET_MODE:
            m = c.set_mode.mode
            if m == ack.MODE_UNSPECIFIED:
                return ack.ACK_INVALID
            if self.cycle_en_cours:
                return ack.ACK_BUSY
            self.mode = m
            return ack.ACK_OK
        if t == ack.CMD_ABORT_CYCLE:
            self.cycle_en_cours = False
            return ack.ACK_OK
        if t == ack.CMD_SET_CHARGE_PORT:
            return ack.ACK_OK
        if t == ack.CMD_HOME:
            if self.cycle_en_cours:
                return ack.ACK_BUSY
            self.homed = True
            return ack.ACK_OK
        if t in (ack.CMD_SET_OUTPUT, ack.CMD_TEST_ACTUATOR):
            if self.mode != ack.MODE_MAINTENANCE:
                return ack.ACK_REFUSED_MODE
            if t == ack.CMD_TEST_ACTUATOR and c.test_actuator.actuator == ack.ACTUATOR_UNSPECIFIED:
                return ack.ACK_INVALID
            if t == ack.CMD_SET_OUTPUT and c.set_output.output == ack.OUTPUT_UNSPECIFIED:
                return ack.ACK_INVALID
            return ack.ACK_OK
        # SORT_WASTE et START_COMPACTION : mode normal, référencé, libre
        if t == ack.CMD_SORT_WASTE and c.sort_waste.bac == ack.BAC_UNSPECIFIED:
            return ack.ACK_INVALID
        if t == ack.CMD_START_COMPACTION and c.start_compaction.bac in (
                ack.BAC_UNSPECIFIED, ack.BAC_PAPIER):
            return ack.ACK_INVALID  # le bac Papier n'est pas compacté
        if self.mode != ack.MODE_NORMAL:
            return ack.ACK_REFUSED_MODE
        if not self.homed:
            return ack.ACK_REFUSED_NOT_HOMED
        if self.cycle_en_cours:
            return ack.ACK_BUSY
        return ack.ACK_OK


_ETAT = EtatSimule()


def statut_pour(commande: "contrat_pb2.Command") -> int:
    """Compatibilité : applique la commande à l'état simulé global."""
    return _ETAT.appliquer(commande)


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