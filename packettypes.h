#ifndef PACKETTYPES_H
#define PACKETTYPES_H

#include "sha1.h"

#define DATA_PACKET_TYPE 1
#define CHECKSUM_PACKET_TYPE 2
#define CHECKSUMCMP_PACKET_TYPE 3

#define PACKET_TYPE_OFFSET 0
#define PACKET_TYPE_LEN 4

#define DATA_PACKET_LEN 512
#define CHECKSUM_PACKET_LEN 74
#define CONFIRMATION_PACKET_LEN 55

#define FILENAME_LEN 50


// client to server
struct DataPacket {
    const int packetType = 1;
    // investigate safety hazard: when buffer size is 1 it still worked; why didn't it overflow?
    char filename[FILENAME_LEN]; 
    int numTotalPackets;
    int packetNumber;
    // char data[412];
};

// server to client
struct ChecksumPacket {
    const int packetType = 2;
    char filename[FILENAME_LEN];
    unsigned char checksum[HASH_CODE_LENGTH];
};

// client to server
struct ConfirmationPacket {
    const int packetType = 3;
    char filename[FILENAME_LEN];
    bool result;
};

#endif
