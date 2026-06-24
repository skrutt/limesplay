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
    uint32_t result = 0;
    uint8_t *ptr = (uint8_t *)&result;

    uint16_t i = 0;
    for (; i < len; i++)
    {
        ptr[i % 4] += ((uint8_t *)data)[i];
    }

    uint8_t *lenptr = (uint8_t *)&len;

    ptr[i % 4] += lenptr[0];
    i++;
    ptr[i % 4] += lenptr[1];

    return result;
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
