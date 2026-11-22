"""Tests des règles de refus du contrat v3 (simulateur de référence)."""
import unittest
import contrat_pb2 as c
from faux_m5stack import EtatSimule


def cmd(t, **kw):
    return c.Command(type=t, **kw)


class RegleV3(unittest.TestCase):
    def setUp(self):
        self.e = EtatSimule()

    def test_tri_refuse_si_non_reference(self):
        r = self.e.appliquer(cmd(c.CMD_SORT_WASTE, sort_waste=c.SortWaste(bac=c.BAC_PLASTIQUE)))
        self.assertEqual(r, c.ACK_REFUSED_NOT_HOMED)

    def test_tri_ok_apres_home(self):
        self.assertEqual(self.e.appliquer(cmd(c.CMD_HOME, home=c.Home())), c.ACK_OK)
        r = self.e.appliquer(cmd(c.CMD_SORT_WASTE, sort_waste=c.SortWaste(bac=c.BAC_NON_RECYCLABLE)))
        self.assertEqual(r, c.ACK_OK)

    def test_urgence_refuse_et_impose_home(self):
        self.e.appliquer(cmd(c.CMD_HOME, home=c.Home()))
        self.e.appliquer(cmd(c.CMD_EMERGENCY_STOP, emergency_stop=c.EmergencyStop()))
        r = self.e.appliquer(cmd(c.CMD_SORT_WASTE, sort_waste=c.SortWaste(bac=c.BAC_PAPIER)))
        self.assertEqual(r, c.ACK_REFUSED_EMERGENCY)
        self.e.appliquer(cmd(c.CMD_CLEAR_EMERGENCY, clear_emergency=c.ClearEmergency()))
        r = self.e.appliquer(cmd(c.CMD_SORT_WASTE, sort_waste=c.SortWaste(bac=c.BAC_PAPIER)))
        self.assertEqual(r, c.ACK_REFUSED_NOT_HOMED)

    def test_papier_non_compacte(self):
        r = self.e.appliquer(cmd(c.CMD_START_COMPACTION, start_compaction=c.StartCompaction(bac=c.BAC_PAPIER)))
        self.assertEqual(r, c.ACK_INVALID)

    def test_test_actuator_reserve_maintenance(self):
        m = cmd(c.CMD_TEST_ACTUATOR, test_actuator=c.TestActuator(actuator=c.ACTUATOR_CARRIAGE, value=10))
        self.assertEqual(self.e.appliquer(m), c.ACK_REFUSED_MODE)
        self.e.appliquer(cmd(c.CMD_SET_MODE, set_mode=c.SetMode(mode=c.MODE_MAINTENANCE)))
        self.assertEqual(self.e.appliquer(m), c.ACK_OK)

    def test_ventilateur_nobeit_quau_stamplc(self):
        # decision 17/11 : DO1 (ventilateur) refuse meme en maintenance ; DO2 (eclairage) accepte
        self.e.appliquer(cmd(c.CMD_SET_MODE, set_mode=c.SetMode(mode=c.MODE_MAINTENANCE)))
        fan = cmd(c.CMD_SET_OUTPUT, set_output=c.SetOutput(output=c.OUTPUT_FAN, enabled=True))
        self.assertEqual(self.e.appliquer(fan), c.ACK_INVALID)
        lum = cmd(c.CMD_SET_OUTPUT, set_output=c.SetOutput(output=c.OUTPUT_LIGHT, enabled=True))
        self.assertEqual(self.e.appliquer(lum), c.ACK_OK)

    def test_tri_refuse_hors_mode_normal(self):
        self.e.appliquer(cmd(c.CMD_HOME, home=c.Home()))
        self.e.appliquer(cmd(c.CMD_SET_MODE, set_mode=c.SetMode(mode=c.MODE_VIDAGE)))
        r = self.e.appliquer(cmd(c.CMD_SORT_WASTE, sort_waste=c.SortWaste(bac=c.BAC_PLASTIQUE)))
        self.assertEqual(r, c.ACK_REFUSED_MODE)

    def test_oneof_incoherent(self):
        r = self.e.appliquer(cmd(c.CMD_HOME, set_mode=c.SetMode(mode=c.MODE_NORMAL)))
        self.assertEqual(r, c.ACK_INVALID)

    def test_roundtrip_enveloppe(self):
        env = c.Envelope(protocol_version=3, message_id=1, session_id=1,
                         heartbeat=c.Heartbeat(source=c.HEARTBEAT_SOURCE_CM5, mode=c.MODE_NORMAL, cm5_reset_count=2))
        d = c.Envelope.FromString(env.SerializeToString())
        self.assertEqual(d.heartbeat.cm5_reset_count, 2)
        self.assertLessEqual(len(env.SerializeToString()), 256)

    def _mode(self, m):
        return self.e.appliquer(cmd(c.CMD_SET_MODE, set_mode=c.SetMode(mode=m)))

    def _compacter(self, bac):
        return self.e.appliquer(cmd(c.CMD_START_COMPACTION, start_compaction=c.StartCompaction(bac=bac)))

    def test_clear_emergency_exige_le_bouton_rearme(self):
        self.e.bouton_present = True
        self.e.appliquer(cmd(c.CMD_HOME, home=c.Home()))
        self.e.appliquer(cmd(c.CMD_EMERGENCY_STOP, emergency_stop=c.EmergencyStop()))
        self.e.bouton_ferme = False
        r = self.e.appliquer(cmd(c.CMD_CLEAR_EMERGENCY, clear_emergency=c.ClearEmergency()))
        self.assertEqual(r, c.ACK_REFUSED_EMERGENCY)
        self.assertTrue(self.e.urgence)
        self.e.bouton_ferme = True
        r = self.e.appliquer(cmd(c.CMD_CLEAR_EMERGENCY, clear_emergency=c.ClearEmergency()))
        self.assertEqual(r, c.ACK_OK)
        self.assertFalse(self.e.urgence)

    def test_maintenance_vers_normal_exige_home(self):
        self.assertEqual(self._mode(c.MODE_MAINTENANCE), c.ACK_OK)        # entrée permise sans référence
        self.assertEqual(self._mode(c.MODE_NORMAL), c.ACK_REFUSED_NOT_HOMED)
        self.e.appliquer(cmd(c.CMD_HOME, home=c.Home()))
        self.assertEqual(self._mode(c.MODE_NORMAL), c.ACK_OK)

    def test_vidage_vers_normal_sans_home(self):
        self.assertEqual(self._mode(c.MODE_VIDAGE), c.ACK_OK)
        self.assertEqual(self._mode(c.MODE_NORMAL), c.ACK_OK)

    def test_plafond_de_cycles_de_compactage(self):
        self.e.appliquer(cmd(c.CMD_HOME, home=c.Home()))
        self.assertEqual(self._compacter(c.BAC_PLASTIQUE), c.ACK_OK)
        self.assertEqual(self._compacter(c.BAC_PLASTIQUE), c.ACK_OK)
        self.assertEqual(self._compacter(c.BAC_PLASTIQUE), c.ACK_INVALID)
        self.assertEqual(self._compacter(c.BAC_NON_RECYCLABLE), c.ACK_OK)   # chaque bac a son compteur
        self._mode(c.MODE_VIDAGE)
        self._mode(c.MODE_NORMAL)                                           # le vidage remet à zéro
        self.assertEqual(self._compacter(c.BAC_PLASTIQUE), c.ACK_OK)


if __name__ == "__main__":
    unittest.main()
