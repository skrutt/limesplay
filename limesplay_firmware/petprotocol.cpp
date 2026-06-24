//Petcol Cpp
#include "petprotocol.h"

//Constructor
petcol::petcol(void(*sendfunc)(const void*, uint16_t ), uint8_t delimiter)
{
    this->send_data_callback = sendfunc;
    this->delimiter = delimiter;
}
//Constructor
petcol::petcol(void(*sendfunc)(const void*, uint16_t ), void (*extra_data_callback)(uint8_t), uint8_t delimiter):
    petcol(sendfunc, delimiter)
{
    this->extra_data_callback = extra_data_callback;
}
//Destructor
petcol::~petcol()
{
}

//CRC, heart of packet verification
//Standard CRC-32 (IEEE 802.3 / zlib), reflected, poly 0xEDB88320. Bitwise so
//no lookup table is kept in flash. Computed over the payload bytes only.
uint32_t petcol::make_CRC(const void * data, uint16_t len)
{
    const uint8_t * ptr = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFUL;

    for(uint16_t i = 0; i < len; i++)
    {
        crc ^= ptr[i];
        for(uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc >> 1) ^ (0xEDB88320UL & (-(crc & 1)));
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}
// Send data in a packet: [payload][crc u32 LE][length u16 LE][delimiter].
// The header is serialised byte by byte so the wire format never depends on
// struct padding/alignment.
bool petcol::sendFunc(const void * data, uint16_t len)
{
    if(!data || !len)
    {
        return false;
    }

    uint32_t crc = make_CRC(data, len);

    send_data_callback(data, len);

    uint8_t trailer[7];
    trailer[0] = (uint8_t)(crc      );
    trailer[1] = (uint8_t)(crc >>  8);
    trailer[2] = (uint8_t)(crc >> 16);
    trailer[3] = (uint8_t)(crc >> 24);
    trailer[4] = (uint8_t)(len      );
    trailer[5] = (uint8_t)(len >>  8);
    trailer[6] = delimiter;
    send_data_callback(trailer, sizeof(trailer));

    return true;
}

// Trailer size on the wire: crc (4) + length (2). The delimiter is not buffered.
#define PETCOL_TRAILER 6

// Recv: buffer the byte; when a delimiter could close a frame, inspect the
// candidate trailing frame *without* disturbing the buffer and only consume it
// once the CRC matches. A delimiter that does not complete a valid frame leaves
// the buffer untouched - no copy-out/copy-back.
packet_recieved *petcol::recv_byte_input(uint8_t byte)
{
    if (byte == delimiter && recv_ring_buf.count() >= PETCOL_TRAILER)
    {
        const uint16_t count = recv_ring_buf.count();

        // The two newest bytes are the length (little-endian).
        uint16_t length = (uint16_t)recv_ring_buf.at(count - 2)
                        | ((uint16_t)recv_ring_buf.at(count - 1) << 8);

        if (length > 0 && length < PACKETSIZE_MAX
            && count >= (uint16_t)(length + PETCOL_TRAILER))
        {
            // Layout at the tail (oldest -> newest): [payload][crc][length].
            const uint16_t crc_pos     = count - PETCOL_TRAILER;
            const uint16_t payload_pos = crc_pos - length;

            uint32_t want_crc = (uint32_t)recv_ring_buf.at(crc_pos)
                              | ((uint32_t)recv_ring_buf.at(crc_pos + 1) <<  8)
                              | ((uint32_t)recv_ring_buf.at(crc_pos + 2) << 16)
                              | ((uint32_t)recv_ring_buf.at(crc_pos + 3) << 24);

            for (uint16_t i = 0; i < length; i++)
            {
                recv_packet.data[i] = recv_ring_buf.at(payload_pos + i);
            }

            if (make_CRC(recv_packet.data, length) == want_crc)
            {
                recv_packet.length = length;

                // Everything before the payload is non-packet "extra" data;
                // hand it to the owner in arrival order.
                for (uint16_t i = 0; i < payload_pos; i++)
                {
                    uint8_t extra = recv_ring_buf.pop();
                    if (extra_data_callback)
                    {
                        extra_data_callback(extra);
                    }
                }

                // Drop the consumed frame (the delimiter was never buffered).
                recv_ring_buf.clear();

                return &recv_packet;
            }
        }
    }

    // Not a frame boundary (or not a valid frame): ordinary data.
    recv_ring_buf.push(byte);

    // Keep the backlog bounded: a byte older than a max-size packet can never be
    // part of a pending frame, so release it as extra data.
    if (recv_ring_buf.count() >= PACKETSIZE_MAX)
    {
        uint8_t extra = recv_ring_buf.pop();
        if (extra_data_callback)
        {
            extra_data_callback(extra);
        }
    }

    return nullptr;
}
