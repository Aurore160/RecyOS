#!/usr/bin/env python3
"""Verifie que les valeurs des enumerations de securite.h sont celles de contrat.proto (v3).
Usage : python3 test/verifier_enums_securite.py   (depuis firmware_m5stack/)"""
import re, sys, os
ici = os.path.dirname(os.path.abspath(__file__))
proto = open(os.path.join(ici, "..", "..", "ContratCM5RPI", "contrat.proto"), encoding="utf8").read()
hdr = open(os.path.join(ici, "..", "components", "securite", "include", "securite.h"), encoding="utf8").read()

def enum_proto(nom):
    m = re.search(r"enum\s+%s\s*\{(.*?)\n\}" % nom, proto, re.S)
    return {k: int(v) for k, v in re.findall(r"^\s*([A-Z_0-9]+)\s*=\s*(\d+)\s*;", m.group(1), re.M)}

def enum_c(nom):
    for m in re.finditer(r"typedef enum\s*\{([^}]*)\}\s*([a-z_0-9]+)\s*;", hdr):
        if m.group(2) == nom:
            return {k: int(v) for k, v in re.findall(r"([A-Z_0-9]+)\s*=\s*(\d+)", m.group(1))}
    raise SystemExit("enum C introuvable : " + nom)

paires = [("OperatingMode", "sec_mode_t", "MODE_", "SEC_MODE_"),
          ("AckStatus", "sec_ack_t", "ACK_", "SEC_ACK_"),
          ("CommandType", "sec_cmd_t", "CMD_", "SEC_CMD_")]
erreurs = 0
for p, c, pp, pc in paires:
    ep, ec = enum_proto(p), enum_c(c)
    for k, v in ec.items():
        nom_proto = pp + k[len(pc):]
        if k.endswith("NONE"): nom_proto = "CMD_UNSPECIFIED"
        if ep.get(nom_proto) != v:
            print("ECART", c, k, v, "contrat :", nom_proto, ep.get(nom_proto)); erreurs += 1
    print("%s : %d valeurs C comparees a %s" % (c, len(ec), p))
sys.exit(1 if erreurs else 0)
