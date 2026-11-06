#include "trame.h"

uint16_t crc16_ccitt(const uint8_t *donnees, size_t n)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        crc ^= (uint16_t)((uint16_t)donnees[i] << 8);
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/* COBS : encode `entree` dans `sortie`; renvoie la longueur ou -1 si capacité insuffisante. */
static int cobs_encoder(const uint8_t *entree, size_t n, uint8_t *sortie, size_t capacite)
{
    size_t lecture = 0, ecriture = 1, code_pos = 0;
    uint8_t code = 1;
    if (capacite < 1) return -1;
    while (lecture < n) {
        if (entree[lecture] == 0) {
            sortie[code_pos] = code;
            code = 1;
            code_pos = ecriture++;
            if (ecriture > capacite) return -1;
        } else {
            if (ecriture >= capacite) return -1;
            sortie[ecriture++] = entree[lecture];
            code++;
            if (code == 0xFF) {
                sortie[code_pos] = code;
                code = 1;
                code_pos = ecriture++;
                if (ecriture > capacite) return -1;
            }
        }
        lecture++;
    }
    sortie[code_pos] = code;
    return (int)ecriture;
}

static int cobs_decoder(const uint8_t *entree, size_t n, uint8_t *sortie, size_t capacite)
{
    size_t lecture = 0, ecriture = 0;
    while (lecture < n) {
        uint8_t code = entree[lecture++];
        if (code == 0) return -1;
        for (uint8_t i = 1; i < code; i++) {
            if (lecture >= n) return -1;           /* corps tronqué */
            if (ecriture >= capacite) return -1;
            sortie[ecriture++] = entree[lecture++];
        }
        if (code != 0xFF && lecture < n) {
            if (ecriture >= capacite) return -1;
            sortie[ecriture++] = 0;
        }
    }
    return (int)ecriture;
}

int trame_encoder(const uint8_t *proto, size_t n, uint8_t *sortie, size_t capacite)
{
    uint8_t brut[TRAME_TAILLE_MAX_PROTOBUF + 2];
    if (n > TRAME_TAILLE_MAX_PROTOBUF || capacite < 3) return TRAME_ERR_TAILLE;
    for (size_t i = 0; i < n; i++) brut[i] = proto[i];
    uint16_t crc = crc16_ccitt(proto, n);
    brut[n] = (uint8_t)(crc >> 8);
    brut[n + 1] = (uint8_t)(crc & 0xFF);
    sortie[0] = 0;
    int c = cobs_encoder(brut, n + 2, sortie + 1, capacite - 2);
    if (c < 0) return TRAME_ERR_TAILLE;
    sortie[1 + c] = 0;
    return c + 2;
}

int trame_decoder_corps(const uint8_t *corps, size_t n, uint8_t *proto, size_t capacite)
{
    uint8_t brut[TRAME_TAILLE_MAX_PROTOBUF + 2];
    if (n > TRAME_TAILLE_MAX_CORPS) return TRAME_ERR_TAILLE;
    int d = cobs_decoder(corps, n, brut, sizeof brut);
    if (d < 2) return TRAME_ERR_COBS;
    size_t m = (size_t)d - 2;
    uint16_t recu = (uint16_t)(((uint16_t)brut[m] << 8) | brut[m + 1]);
    if (crc16_ccitt(brut, m) != recu) return TRAME_ERR_CRC;
    if (m > capacite) return TRAME_ERR_TAILLE;
    for (size_t i = 0; i < m; i++) proto[i] = brut[i];
    return (int)m;
}
