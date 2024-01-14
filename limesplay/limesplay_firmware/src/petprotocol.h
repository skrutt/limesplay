#ifndef PETCOL_H
#define PETCOL_H
#include <stdint.h>
#include <buf.h>

//Let's go with a fixed packetsize for the first low spec iteration
#define PACKETSIZE_MAX  128
#define PETCOL_BYTE     0xAA    //0b1010101010


typedef struct {
    // uint32_t  type;
    uint32_t  CRC;
    uint16_t  length;
} packet_header;

//Not sure if we actually need the packet?
typedef struct {
    uint16_t length;
#ifdef PACKETSIZE_MAX
    uint8_t data[PACKETSIZE_MAX];
#else
    void * data;
#endif
} packet_recieved;

typedef struct {
    void(*sendfunc)(const void*, uint16_t size);
    bool(*recvfunc)(void*);
} pet_TL;

#define SENDSIZE_MAX PACKETSIZE_MAX + sizeof(packet_header)

class petcol
{
public:
    petcol(pet_TL );
    petcol(void(*sendfunc)(const void*, uint16_t ), bool(*recvfunc)(uint8_t*));
    petcol(void(*sendfunc)(const void*, uint16_t ));
    petcol(void(*sendfunc)(const void*, uint16_t ), void (*extra_data_callback)(uint8_t) );
    ~petcol();

    bool sendFunc(const void * data, uint16_t len);

    packet_recieved *recv_byte_input(uint8_t byte);

private:
    pet_TL TL;
    // uint8_t send_buf[SENDSIZE_MAX];
    packet_recieved recv_packet;
    MyBuffer <uint8_t>recv_ring_buf;
    void (*extra_data_callback)(uint8_t byte);

    uint32_t make_CRC(const void * data, uint16_t len);

};

#endif
