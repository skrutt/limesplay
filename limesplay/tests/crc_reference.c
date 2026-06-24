/* Standalone copy of petcol::make_CRC from petprotocol.cpp, used to verify the
 * Python checksum stays byte-compatible with the firmware.
 *
 * Reads the payload as hex bytes from argv[1] (e.g. "0148656c6c6f") and prints
 * the resulting checksum as a decimal uint32.
 *
 * Build: cc -o crc_reference crc_reference.c
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t make_CRC(const void *data, uint16_t len)
{
    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFUL;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= ptr[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc >> 1) ^ (0xEDB88320UL & (-(crc & 1)));
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <hex-payload>\n", argv[0]);
        return 2;
    }

    const char *hex = argv[1];
    size_t hexlen = strlen(hex);
    if (hexlen % 2 != 0)
    {
        fprintf(stderr, "hex payload must have an even number of digits\n");
        return 2;
    }

    size_t n = hexlen / 2;
    uint8_t *buf = (uint8_t *)malloc(n ? n : 1);
    for (size_t i = 0; i < n; i++)
    {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1)
        {
            fprintf(stderr, "bad hex at offset %zu\n", i * 2);
            free(buf);
            return 2;
        }
        buf[i] = (uint8_t)byte;
    }

    printf("%u\n", make_CRC(buf, (uint16_t)n));
    free(buf);
    return 0;
}
