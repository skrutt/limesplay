//Petcol Cpp
#include "petprotocol.h"

//Constructor
petcol::petcol(pet_TL TL)
{
    this->TL = TL;
}
//Constructor
petcol::petcol(void(*sendfunc)(const void*, uint16_t ))
{
    this->TL.sendfunc = sendfunc;
}
//Constructor
petcol::petcol(void(*sendfunc)(const void*, uint16_t ), void (*extra_data_callback)(uint8_t)):
    petcol(sendfunc)
{
    this->extra_data_callback = extra_data_callback;
}
//Destructor
petcol::~petcol()
{
}

//CRC, heart of packet verification
uint32_t petcol::make_CRC(const void * data, uint16_t len)
{
    uint32_t result = 0;
    uint8_t * ptr = (uint8_t *)&result;

    //Loop and add all data
    uint16_t i = 0;
    for(; i < len; i++)
    {
        ptr[i%4] += ((uint8_t*)data)[i];
    }

    //Now do len
    uint8_t * lenptr = (uint8_t*)&len;

    ptr[i%4] += lenptr[0];
    i++;
    ptr[i%4] += lenptr[1];

    return result;
}
// Send data in a packet
bool petcol::sendFunc(const void * data, uint16_t len)
{
    if(!data || !len)
    {
        return false;
    }
    //Set packet_header
    packet_header send_header;

    send_header.CRC = make_CRC(data, len);
    send_header.length = len;

    //Send data
    TL.sendfunc(data, len);

    TL.sendfunc(&send_header, sizeof(send_header));

    uint8_t endbyte;
    endbyte = PETCOL_BYTE;
    TL.sendfunc(&endbyte, 1);

    return true;
}
// #define COPY_FROM_BUF_TO_PTR(PTR, SIZE) for (uint16_t i = 0; i < SIZE; i++)
#define COPY_FROM_BUF_TO_PTR(PTR, SIZE) for (uint16_t i = SIZE ; i != 0; i--) \
{\
    ((uint8_t*)PTR)[i - 1] = recv_ring_buf.getlast();\
}

// #define COPY_FROM_PTR_TO_BUF(PTR, SIZE) for (uint16_t i = SIZE ; i != 0; i--)
#define COPY_FROM_PTR_TO_BUF(PTR, SIZE) for (uint16_t i = 0; i < SIZE; i++) \
{\
    recv_ring_buf.put(((uint8_t*)PTR)[i]);\
}

//Recv.. Put byte in buffer, check for packet, return packet if accurat
packet_recieved *petcol::recv_byte_input(uint8_t byte)
{

    if ( (byte == PETCOL_BYTE) && ((sizeof(packet_header) + 1) <= recv_ring_buf.sizeavailable()) )
    {
        //We found a closebyte, check if this is a valid packet!
        packet_header header;

        //Get len
        COPY_FROM_BUF_TO_PTR(&header.length, 2);

        //Check if lengh is sane
        if ( (header.length < PACKETSIZE_MAX) && ((header.length + 4 ) <= recv_ring_buf.sizeavailable()) )
        {
            //Seems like we have a packet
            //Next up is to copy data and check CRC
            COPY_FROM_BUF_TO_PTR(&header.CRC, 4);

            COPY_FROM_BUF_TO_PTR(recv_packet.data, header.length);


            uint32_t CRC = make_CRC(recv_packet.data, header.length);

            // Serial.print("\nHeader: ");
            // for (size_t i = 0; i < sizeof(packet_header); i++)
            // {
            //     Serial.print(((uint8_t*)&header)[i]);
            //     Serial.print(' ');
            // }
            // Serial.print("\nlength: ");
            // Serial.print(header.length);
            // Serial.print("\nCRC: ");
            // Serial.print(header.CRC);
            // Serial.print("\nCalculated CRC: ");
            // Serial.print(CRC);
            // Serial.println(" end");

            if (header.CRC == CRC)
            {
                //Match!
                recv_packet.length = header.length;

                //Add spitting out any remaining data here!
                if (extra_data_callback)    //If callback for extra data is activated, spit them out here
                {
                    while (recv_ring_buf.available())
                    {                                   //This should not be a serial write, but a return of bytes to our owner
                        extra_data_callback(recv_ring_buf.get());
                    }
                }

                //Since there is a return here, all below code is for not matching packets!
                return &recv_packet;
            }

            //Not match,  we have to restore the mess we made
            COPY_FROM_PTR_TO_BUF(recv_packet.data, header.length);      //FIXME, err, this should probably be ptr to buf? FIXD
            COPY_FROM_PTR_TO_BUF(&header.CRC, 4);
        }
        //Seems like not
        COPY_FROM_PTR_TO_BUF(&header.length, 2);

    }
    //No go, add byte to buffer
    recv_ring_buf.put(byte);

    //TODO; add passing on of extra bytes in excess of max packet length.. or possibly exceeding our buffer
    if (recv_ring_buf.sizeavailable() >= PACKETSIZE_MAX)
    {
        if (extra_data_callback)    //If callback for extra data is activated, spit them out here
        {
            extra_data_callback(recv_ring_buf.get());
        }
        else
        {
            recv_ring_buf.get();
        }
    }

    return NULL;
}
