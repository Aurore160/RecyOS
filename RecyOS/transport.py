"""
transport.py — Couche de transport partagée entre le RPi et le M5Stack (ou son simulateur).

Combine 3 briques déjà validées séparément :
  1. contrat_pb2 (Protobuf)  : transforme un objet Python en octets structurés.
  2. cobs                    : encode ces octets pour qu'ils ne contiennent jamais 0x00.
  3. socket                  : envoie/reçoit les octets sur le réseau (remplacera l'UART plus tard).
"""

import socket
from cobs import cobs
import contrat_pb2


def envoyer_message(sock: socket.socket, envelope: contrat_pb2.Envelope) -> None:
    """
    Envoie une Envelope Protobuf sur la connexion `sock`, encodée en trame COBS.

    Étapes (dans l'ordre inverse du décodage) :
      Envelope (objet Python)
        -> octets Protobuf     (SerializeToString)
        -> octets COBS         (cobs.encode, plus de 0x00 à l'intérieur)
        -> trame complète      (0x00 + octets COBS + 0x00)
    """
    octets_proto = envelope.SerializeToString()
    octets_cobs = cobs.encode(octets_proto)
    trame = b'\x00' + octets_cobs + b'\x00'
    sock.sendall(trame)


def recevoir_message(sock: socket.socket) -> contrat_pb2.Envelope:
    """
    Lit UNE trame complète depuis `sock` (jusqu'au prochain 0x00 de fin),
    puis la décode en objet Envelope Protobuf.

    Cas limite géré : TCP peut livrer les octets en plusieurs morceaux
    (ex. un message coupé en deux paquets réseau). On lit donc octet par
    octet jusqu'à trouver le 0x00 de fin, plutôt que de supposer qu'un
    seul appel réseau suffit.
    """
    octets_recus = bytearray()
    while True:
        octet = sock.recv(1)
        if octet == b'':
            # La connexion a été fermée par l'autre côté en pleine lecture.
            raise ConnectionError("Connexion fermée pendant la réception d'un message.")
        if octet == b'\x00':
            if len(octets_recus) == 0:
                # 0x00 de début de trame : on l'ignore et on continue à lire.
                continue
            else:
                # 0x00 de fin de trame : on s'arrête ici.
                break
        octets_recus += octet

    try:
        octets_proto = cobs.decode(bytes(octets_recus))
    except cobs.DecodeError as erreur:
        # Cas limite du module 0 : trame corrompue. On ne plante pas le
        # programme, on signale clairement l'erreur à l'appelant.
        raise ValueError(f"Trame COBS invalide, rejetée : {erreur}")

    envelope = contrat_pb2.Envelope()
    envelope.ParseFromString(octets_proto)
    return envelope