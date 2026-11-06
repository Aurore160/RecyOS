/* Test sur PC : gcc -I../components/transport/include test_trame.c ../components/transport/trame.c -o test_trame && ./test_trame */
#include <stdio.h>
#include <string.h>
#include "trame.h"
#include "vecteurs_trame.h"

static int echecs = 0;
#define VERIFIER(c) do { if (!(c)) { printf("ECHEC ligne %d : %s\n", __LINE__, #c); echecs++; } } while (0)

int main(void)
{
    VERIFIER(crc16_ccitt((const uint8_t *)"123456789", 9) == 0x29B1);
    VERIFIER(crc16_ccitt((const uint8_t *)"", 0) == 0xFFFF);

    const uint8_t *protos[NB_TRAMES] = { PROTO_0, PROTO_1 };
    const uint8_t *trames[NB_TRAMES] = { TRAME_0, TRAME_1 };
    size_t lp[NB_TRAMES] = { sizeof PROTO_0, sizeof PROTO_1 };
    size_t lt[NB_TRAMES] = { sizeof TRAME_0, sizeof TRAME_1 };
    uint8_t buf[TRAME_TAILLE_MAX], proto[TRAME_TAILLE_MAX_PROTOBUF];

    for (int i = 0; i < NB_TRAMES; i++) {
        int n = trame_encoder(protos[i], lp[i], buf, sizeof buf);
        VERIFIER(n == (int)lt[i]);
        VERIFIER(memcmp(buf, trames[i], lt[i]) == 0);       /* identique a Python */
        int m = trame_decoder_corps(trames[i] + 1, lt[i] - 2, proto, sizeof proto);
        VERIFIER(m == (int)lp[i]);
        VERIFIER(memcmp(proto, protos[i], lp[i]) == 0);
        uint8_t corrompue[TRAME_TAILLE_MAX];
        memcpy(corrompue, trames[i], lt[i]);
        corrompue[5] ^= 0x01;
        VERIFIER(trame_decoder_corps(corrompue + 1, lt[i] - 2, proto, sizeof proto) < 0);
    }
    /* messages contenant des 0x00 et taille maximale */
    uint8_t gros[TRAME_TAILLE_MAX_PROTOBUF];
    for (size_t i = 0; i < sizeof gros; i++) gros[i] = (uint8_t)(i % 7 == 0 ? 0 : i);
    int n = trame_encoder(gros, sizeof gros, buf, sizeof buf);
    VERIFIER(n > 0);
    for (int i = 1; i < n - 1; i++) VERIFIER(buf[i] != 0);
    int m = trame_decoder_corps(buf + 1, (size_t)n - 2, proto, sizeof proto);
    VERIFIER(m == (int)sizeof gros && memcmp(proto, gros, sizeof gros) == 0);
    VERIFIER(trame_encoder(gros, sizeof gros + 1, buf, sizeof buf) == TRAME_ERR_TAILLE);

    printf(echecs ? "%d echec(s)\n" : "OK : tous les tests passent\n", echecs);
    return echecs != 0;
}
