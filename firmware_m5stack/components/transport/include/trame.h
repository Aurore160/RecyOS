#pragma once
/* Trames RecyStation (contrat v2) : 0x00 | COBS( Protobuf + CRC-16 ) | 0x00
   Même format que RecyOS/transport.py. Aucune dépendance ESP-IDF : testable sur PC. */
#include <stddef.h>
#include <stdint.h>

#define TRAME_TAILLE_MAX_PROTOBUF 256u
#define TRAME_TAILLE_MAX_CORPS    (TRAME_TAILLE_MAX_PROTOBUF + 4u)
#define TRAME_TAILLE_MAX          (TRAME_TAILLE_MAX_CORPS + 2u)

#define TRAME_OK            0
#define TRAME_ERR_TAILLE   -1  /* message ou tampon trop grand */
#define TRAME_ERR_COBS     -2  /* corps COBS invalide */
#define TRAME_ERR_CRC      -3  /* CRC-16 invalide */

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). crc16(\"123456789\") == 0x29B1. */
uint16_t crc16_ccitt(const uint8_t *donnees, size_t n);

/* Protobuf -> trame complète. Renvoie la longueur écrite dans `sortie`, ou une erreur négative. */
int trame_encoder(const uint8_t *proto, size_t n, uint8_t *sortie, size_t capacite);

/* Corps COBS (SANS les 0x00 délimiteurs) -> octets Protobuf. Renvoie la longueur ou une erreur négative. */
int trame_decoder_corps(const uint8_t *corps, size_t n, uint8_t *proto, size_t capacite);
