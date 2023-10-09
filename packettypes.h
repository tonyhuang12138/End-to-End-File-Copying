#ifndef PACKETTYPES_H
#define PACKETTYPES_H

#define FILENAME_PACKET_TYPE 1
#define CHECKSUM_PACKET_TYPE 2
#define CHECKSUMCMP_PACKET_TYPE 3
#define DATA_PACKET_TYPE 4

#define MAX_MSG_LEN 512
#define PACKET_TYPE_OFFSET 0
#define PACKET_TYPE_LEN 4


struct FilenamePacket {
    int packetType = 1;               // Question: should we do this in 
                                            // child struct?
    char filename[50]; // investigate safety hazard: when buffer size is 1 it still worked; why didn't it overflow?
};

struct ChecksumPacket {
    const int packetType = 2;
    char sha1[21];
};

struct ChecksumComparisonPacket {
    const int packetType = 3;
    bool result;
};

// subject to change
struct DataPacket {
    const int packetType = 4;
    char header[100];
    char data[412];
};

#endif