# Firmware M5Stack StamPLC (RecyStation)

Projet ESP-IDF (cible ESP32-S3) du StamPLC K141. Il dialogue avec le Raspberry Pi CM5 via UART
(trames COBS + Protobuf, contrat dans `../ContratCM5RPI/contrat.proto`).

## Structure
- `main/` : `app_main`, démarrage et création des tâches.
- `components/contrat/` : code nanopb généré depuis le `.proto` (ne pas modifier à la main).
- `components/transport/` : UART, trames COBS, ACK, détection de redémarrage (session_id).
- `components/drivers/` : PCA9685, PCF8574, JSN-SR04T, relais DO, Nema 23 (STEP/DIR/EN), BTS7960.
- `components/securite/` : arrêt d'urgence, état sûr, perte du heartbeat.
- `components/logique/` : trappe, compactage, rapport capteurs (500 ms), heartbeat (2 s).
- `test/` : tests unitaires.
- `tools/gen_contrat.sh` : régénère `components/contrat` à partir du `.proto`.

## Compiler
```
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```
Prérequis : ESP-IDF 5.x installé et activé (`export.bat` / `export.sh`).

## Règles
- Le `.proto` est la source unique : le modifier seulement dans `ContratCM5RPI/`, puis lancer `tools/gen_contrat.sh`.
- Jamais réutiliser un numéro de champ existant dans le `.proto`.
- Broches et adresses : voir la spec StamPLC v3.2 et le mapping PoC v1.3 (espace CTO). Aucune broche n'est encore codée ici.
