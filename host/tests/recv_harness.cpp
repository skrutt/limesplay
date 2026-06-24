// Test harness: compiles the *firmware* petcol decoder on the host and feeds it
// the raw byte stream on stdin. For every decoded packet it prints "PKT <hex>",
// and at the end "EXTRA <hex>" for all bytes the decoder reported as non-packet
// (extra) data. The Python tests build frames with petcol.py and assert this
// firmware decoder reproduces them - a cross-implementation check.

#include <cstdio>
#include <cstdint>
#include <vector>

#include "petprotocol.h"

static std::vector<uint8_t> g_extra;

static void sink_send(const void *, uint16_t) {}
static void on_extra(uint8_t b) { g_extra.push_back(b); }

int main()
{
    petcol p(sink_send, on_extra);   // default delimiter (PETCOL_BYTE)

    int c;
    while ((c = getchar()) != EOF)
    {
        packet_recieved *pkt = p.recv_byte_input((uint8_t)c);
        if (pkt)
        {
            printf("PKT");
            for (uint16_t i = 0; i < pkt->length; i++)
                printf(" %02x", pkt->data[i]);
            printf("\n");
        }
    }

    printf("EXTRA");
    for (size_t i = 0; i < g_extra.size(); i++)
        printf(" %02x", g_extra[i]);
    printf("\n");

    return 0;
}
