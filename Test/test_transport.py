"""Tests du transport (CRC-16, trames, resynchronisation). Lancer : python -m unittest discover Test"""
import json
import os
import sys
import unittest

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path[:0] = [os.path.join(RACINE, "ContratCM5RPI"), RACINE]

import contrat_pb2  # noqa: E402
from RecyOS.transport import (  # noqa: E402
    Liaison, TransportTrames, crc16, decoder_corps, encoder_trame, TAILLE_MAX_PROTOBUF)


class LiaisonMemoire(Liaison):
    """Liaison de test : on lui donne des morceaux d'octets à 'recevoir'."""

    def __init__(self, morceaux):
        self.morceaux = list(morceaux)
        self.ecrits = b""

    def ecrire(self, octets):
        self.ecrits += octets

    def lire(self, maximum, delai_s):
        if not self.morceaux:
            if delai_s is None:
                raise ConnectionError("fermée")
            return b""
        return self.morceaux.pop(0)


with open(os.path.join(RACINE, "ContratCM5RPI", "vecteurs_trame.json"), encoding="utf8") as f:
    VECTEURS = json.load(f)


class TestCrc(unittest.TestCase):
    def test_vecteurs(self):
        for v in VECTEURS["crc16"]:
            self.assertEqual(crc16(bytes.fromhex(v["entree_hex"])), int(v["crc"], 16))


class TestTrames(unittest.TestCase):
    def test_vecteurs_communs(self):
        for v in VECTEURS["trames"]:
            self.assertEqual(encoder_trame(bytes.fromhex(v["proto_hex"])).hex(), v["trame_hex"])

    def test_aller_retour(self):
        for v in VECTEURS["trames"]:
            trame = bytes.fromhex(v["trame_hex"])
            self.assertEqual(decoder_corps(trame[1:-1]).hex(), v["proto_hex"])

    def test_pas_de_zero_dans_le_corps(self):
        for v in VECTEURS["trames"]:
            self.assertNotIn(0, bytes.fromhex(v["trame_hex"])[1:-1])

    def test_crc_invalide_rejete(self):
        trame = bytearray(bytes.fromhex(VECTEURS["trames"][0]["trame_hex"]))
        trame[5] ^= 0x01
        with self.assertRaises(ValueError):
            decoder_corps(bytes(trame[1:-1]))

    def test_trop_long_refuse_a_l_envoi(self):
        with self.assertRaises(ValueError):
            encoder_trame(b"\x01" * (TAILLE_MAX_PROTOBUF + 1))


class TestReception(unittest.TestCase):
    def setUp(self):
        self.t1 = bytes.fromhex(VECTEURS["trames"][0]["trame_hex"])
        self.t2 = bytes.fromhex(VECTEURS["trames"][1]["trame_hex"])

    def test_trame_en_plusieurs_morceaux(self):
        liaison = LiaisonMemoire([self.t1[:5], self.t1[5:12], self.t1[12:]])
        env = TransportTrames(liaison).recevoir(1.0)
        self.assertEqual(env.heartbeat.firmware_version, 0x10000)

    def test_deux_trames_collees(self):
        tr = TransportTrames(LiaisonMemoire([self.t1 + self.t2]))
        self.assertEqual(tr.recevoir(1.0).WhichOneof("payload"), "heartbeat")
        self.assertEqual(tr.recevoir(1.0).WhichOneof("payload"), "command")

    def test_resynchronisation_apres_bruit_et_trame_corrompue(self):
        corrompue = bytearray(self.t1)
        corrompue[6] ^= 0xFF
        flux = b"\x55\xAA" + bytes(corrompue) + self.t2
        tr = TransportTrames(LiaisonMemoire([flux]))
        with self.assertRaises(ValueError):
            tr.recevoir(1.0)
        self.assertEqual(tr.recevoir(1.0).WhichOneof("payload"), "command")

    def test_delai_expire(self):
        self.assertIsNone(TransportTrames(LiaisonMemoire([])).recevoir(0.05))

    def test_bruit_sans_fin_borne(self):
        tr = TransportTrames(LiaisonMemoire([b"\x01" * 600]))
        with self.assertRaises(ValueError):
            tr.recevoir(1.0)

    def test_envoi(self):
        liaison = LiaisonMemoire([])
        env = contrat_pb2.Envelope(protocol_version=3, message_id=1,
                                   heartbeat=contrat_pb2.Heartbeat(self_test_ok=True))
        TransportTrames(liaison).envoyer(env)
        self.assertEqual(liaison.ecrits[0], 0)
        self.assertEqual(liaison.ecrits[-1], 0)


if __name__ == "__main__":
    unittest.main()
