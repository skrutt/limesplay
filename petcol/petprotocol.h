#ifndef PETCOL_H
#define PETCOL_H
#include <stdint.h>
#include "buf.h"

//Let's go with a fixed packetsize for the first low spec iteration
#define PACKETSIZE_MAX  128
#define PETCOL_BYTE     0xAA    //0b10101010
#define RECV_BUFSIZE    256     //ring buffer for the receive stream

//A decoded packet handed back to the caller.
typedef struct {
    uint16_t length;
    uint8_t data[PACKETSIZE_MAX];
} packet_recieved;

// petcol: a self-synchronising framing layer over an arbitrary byte transport.
//
// petcol does not own the link. You supply a function that writes raw bytes out
// (sendfunc). To transmit, call sendFunc(data, len): it frames the payload
// (CRC-32 + length + delimiter) and writes it via sendfunc. To receive, feed
// every incoming byte to recv_byte_input(byte): it returns a pointer to a
// decoded, CRC-checked packet once a full frame has arrived, or NULL otherwise.
//
// Bytes that are not part of a packet (e.g. interleaved plain-text debug) are
// handed to extra_data_callback, if one was supplied, so a petcol channel can
// share a link with ordinary serial output. Give instances distinct delimiters
// to run several independent channels over the same link.
//
// There is intentionally no "receive callback": receiving is pull-by-feeding
// (call recv_byte_input per byte), not push-by-callback.
class petcol
{
public:
    //Minimal: send only.
    petcol(void(*sendfunc)(const void*, uint16_t ), uint8_t delimiter = PETCOL_BYTE);
    //With a callback for bytes that are not part of a packet.
    petcol(void(*sendfunc)(const void*, uint16_t ), void (*extra_data_callback)(uint8_t), uint8_t delimiter = PETCOL_BYTE );
    ~petcol();

    //Frame and transmit one packet. Returns false on empty/null input.
    bool sendFunc(const void * data, uint16_t len);

    //Feed one received byte. Returns a decoded packet when a full, CRC-valid
    //frame completes, otherwise NULL.
    packet_recieved *recv_byte_input(uint8_t byte);

private:
    void(*send_data_callback)(const void*, uint16_t size);
    uint8_t delimiter;          //Frame terminator for this instance (defaults to PETCOL_BYTE)
    packet_recieved recv_packet;
    MyBuffer<uint8_t, RECV_BUFSIZE> recv_ring_buf;
    void (*extra_data_callback)(uint8_t byte) = nullptr;

    uint32_t make_CRC(const void * data, uint16_t len);

};

#endif
