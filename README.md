# RecyOS — logiciel de la borne RecyStation

Dépôt des logiciels embarqués de la borne :

- **`RecyOS/`** : côté Raspberry Pi CM5 (Python). `transport.py` envoie et reçoit les trames
  (COBS + CRC-16) ; `m5stack_bridge.py` (pont vers le StamPLC) reste à écrire.
- **`ContratCM5RPI/`** : le contrat de communication (`contrat.proto`, version 2) et le simulateur du StamPLC
  (`faux_m5stack.py`). Le `.proto` est la **seule source de vérité** : ne jamais en copier une version ailleurs.
- **`firmware_m5stack/`** : projet ESP-IDF du StamPLC K141 (ESP32-S3). Voir son README.
- **`docs/`** : décisions et points à trancher.
- **`archives_essais/`** : anciens essais (COBS, Protobuf), conservés pour mémoire.

## Format d'une trame (UART 921 600 bauds)

`0x00 | COBS( Protobuf(Envelope) + CRC-16 ) | 0x00`. CRC-16/CCITT-FALSE, poids fort d'abord, taille Protobuf maximale 256 octets.
Les vecteurs de test communs Python / C sont dans `ContratCM5RPI/vecteurs_trame.json`.

## Démarrage rapide

```
pip install -r requirements.txt
python -m unittest discover -s Test -p "test_transport.py"
python ContratCM5RPI/faux_m5stack.py          # terminal 1
python ContratCM5RPI/test_client_recyos.py    # terminal 2
```

Régénérer le code Python du contrat : `python -m grpc_tools.protoc -IContratCM5RPI --python_out=ContratCM5RPI ContratCM5RPI/contrat.proto`
(`pip install grpcio-tools`). Côté firmware : `firmware_m5stack/tools/gen_contrat.sh`.

## Branches

- `main` : version stable.
- `M5stack` : conception du firmware du StamPLC.
