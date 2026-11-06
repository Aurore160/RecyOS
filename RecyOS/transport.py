"""
transport.py — Couche de transport entre le RPi (CM5) et le M5Stack StamPLC.

Format d'une trame sur le fil (contrat v2) :

    0x00 | COBS( Protobuf(Envelope) + CRC-16 ) | 0x00

  - Protobuf : l'Envelope sérialisée (voir ContratCM5RPI/contrat.proto).
  - CRC-16   : CRC-16/CCITT-FALSE (polynôme 0x1021, valeur initiale 0xFFFF),
               calculé sur les octets Protobuf, ajouté en 2 octets, poids fort d'abord.
  - COBS     : encode le tout pour qu'il ne contienne jamais 0x00 ; le 0x00 sert
               donc de délimiteur de trame et permet de se resynchroniser.
  - Taille maximale : TAILLE_MAX_PROTOBUF octets de Protobuf (le plus gros message
    actuel fait 116 octets). Une trame plus longue est rejetée.

Le même format est implémenté en C côté firmware
(firmware_m5stack/components/transport). Les vecteurs de test communs sont dans
ContratCM5RPI/vecteurs_trame.json.

Deux liaisons sont prévues : un socket TCP (simulateur) et un port série
(UART réelle, 921 600 bauds, voir la spécification StamPLC).
"""

import socket
import time
from typing import Optional

from cobs import cobs
import contrat_pb2

TAILLE_MAX_PROTOBUF = 256          # octets Protobuf maximum par trame
TAILLE_MAX_CORPS = TAILLE_MAX_PROTOBUF + 2 + 2   # + CRC + surcoût COBS (large)
BAUDS_UART = 921_600


# ---------------------------------------------------------------------------
# CRC-16/CCITT-FALSE
# ---------------------------------------------------------------------------
def crc16(donnees: bytes) -> int:
    """CRC-16/CCITT-FALSE. Vecteur de contrôle : crc16(b"123456789") == 0x29B1."""
    crc = 0xFFFF
    for octet in donnees:
        crc ^= octet << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


# ---------------------------------------------------------------------------
# Trames (sans réseau : fonctions pures, faciles à tester)
# ---------------------------------------------------------------------------
def encoder_trame(octets_proto: bytes) -> bytes:
    """Protobuf -> trame complète (0x00 + COBS(proto + CRC) + 0x00)."""
    if len(octets_proto) > TAILLE_MAX_PROTOBUF:
        raise ValueError(f"Message trop long : {len(octets_proto)} octets "
                         f"(maximum {TAILLE_MAX_PROTOBUF}).")
    crc = crc16(octets_proto)
    corps = octets_proto + crc.to_bytes(2, "big")
    return b"\x00" + cobs.encode(corps) + b"\x00"


def decoder_corps(corps_cobs: bytes) -> bytes:
    """Corps COBS (sans les 0x00) -> octets Protobuf, après contrôle du CRC."""
    try:
        decode = cobs.decode(corps_cobs)
    except cobs.DecodeError as erreur:
        raise ValueError(f"Trame COBS invalide, rejetée : {erreur}")
    if len(decode) < 2:
        raise ValueError("Trame trop courte : pas de CRC.")
    octets_proto, crc_recu = decode[:-2], int.from_bytes(decode[-2:], "big")
    if crc16(octets_proto) != crc_recu:
        raise ValueError("CRC-16 invalide : trame corrompue, rejetée.")
    return octets_proto


# ---------------------------------------------------------------------------
# Liaisons (ce qui transporte les octets)
# ---------------------------------------------------------------------------
class Liaison:
    """Interface minimale : écrire des octets, lire des octets avec un délai maximal."""

    def ecrire(self, octets: bytes) -> None:
        raise NotImplementedError

    def lire(self, maximum: int, delai_s: Optional[float]) -> bytes:
        """Lit au plus `maximum` octets. Renvoie b"" si le délai expire.
        Lève ConnectionError si la liaison est fermée."""
        raise NotImplementedError


class LiaisonSocket(Liaison):
    def __init__(self, sock: socket.socket):
        self.sock = sock

    def ecrire(self, octets: bytes) -> None:
        self.sock.sendall(octets)

    def lire(self, maximum: int, delai_s: Optional[float]) -> bytes:
        ancien = self.sock.gettimeout()
        self.sock.settimeout(delai_s)
        try:
            donnees = self.sock.recv(maximum)
        except socket.timeout:
            return b""
        finally:
            self.sock.settimeout(ancien)
        if donnees == b"":
            raise ConnectionError("Connexion fermée par l'autre côté.")
        return donnees


class LiaisonSerie(Liaison):
    """UART réelle via pyserial (installé seulement si on s'en sert)."""

    def __init__(self, port: str, bauds: int = BAUDS_UART):
        import serial  # import tardif : inutile pour le simulateur TCP
        self.serie = serial.Serial(port, bauds, timeout=0.1)

    def ecrire(self, octets: bytes) -> None:
        self.serie.write(octets)
        self.serie.flush()

    def lire(self, maximum: int, delai_s: Optional[float]) -> bytes:
        self.serie.timeout = delai_s
        return self.serie.read(maximum)


class TransportTrames:
    """Envoie et reçoit des Envelope sur une Liaison, avec resynchronisation."""

    def __init__(self, liaison: Liaison):
        self.liaison = liaison
        self._tampon = bytearray()

    def envoyer(self, envelope: contrat_pb2.Envelope) -> None:
        self.liaison.ecrire(encoder_trame(envelope.SerializeToString()))

    def recevoir(self, delai_s: Optional[float] = None) -> Optional[contrat_pb2.Envelope]:
        """Lit UNE trame valide. Renvoie None si `delai_s` expire avant d'en avoir une.

        Lève ValueError pour une trame corrompue (CRC, COBS, trop longue) : la trame
        est jetée et on peut rappeler `recevoir` pour lire la suivante.
        Lève ConnectionError si la liaison est fermée.
        """
        fin = None if delai_s is None else time.monotonic() + delai_s
        while True:
            corps = self._extraire_corps()
            if corps is not None:
                envelope = contrat_pb2.Envelope()
                envelope.ParseFromString(decoder_corps(corps))
                return envelope
            reste = None if fin is None else max(0.0, fin - time.monotonic())
            if reste == 0.0:
                return None
            donnees = self.liaison.lire(512, reste)
            self._tampon += donnees

    def _extraire_corps(self) -> Optional[bytes]:
        """Cherche dans le tampon un corps de trame complet (entre deux 0x00)."""
        while True:
            try:
                debut = self._tampon.index(0)
            except ValueError:
                # Aucun délimiteur : si le tampon dépasse la taille max, c'est du bruit.
                if len(self._tampon) > TAILLE_MAX_CORPS:
                    self._tampon.clear()
                    raise ValueError("Trame trop longue, rejetée.")
                return None
            del self._tampon[:debut + 1]          # on jette ce qui précède le 0x00
            try:
                fin = self._tampon.index(0)
            except ValueError:
                if len(self._tampon) > TAILLE_MAX_CORPS:
                    self._tampon.clear()
                    raise ValueError("Trame trop longue, rejetée.")
                # Corps incomplet : on remet le 0x00 de début pour la prochaine lecture.
                self._tampon[:0] = b"\x00"
                return None
            corps = bytes(self._tampon[:fin])
            del self._tampon[:fin]                # on garde le 0x00 : début possible de la trame suivante
            if len(corps) == 0:
                continue                           # 0x00 en double : on ignore
            if len(corps) > TAILLE_MAX_CORPS:
                raise ValueError("Trame trop longue, rejetée.")
            return corps


# ---------------------------------------------------------------------------
# Compatibilité : anciennes fonctions utilisées par le simulateur et les tests
# ---------------------------------------------------------------------------
def _transport_de(sock: socket.socket) -> TransportTrames:
    transport = getattr(sock, "_recyos_transport", None)
    if transport is None:
        transport = TransportTrames(LiaisonSocket(sock))
        sock._recyos_transport = transport
    return transport


def envoyer_message(sock: socket.socket, envelope: contrat_pb2.Envelope) -> None:
    """Envoie une Envelope sur un socket (trame avec CRC-16)."""
    _transport_de(sock).envoyer(envelope)


def recevoir_message(sock: socket.socket) -> contrat_pb2.Envelope:
    """Lit une Envelope depuis un socket (bloquant). Lève ConnectionError / ValueError."""
    envelope = _transport_de(sock).recevoir(None)
    return envelope
