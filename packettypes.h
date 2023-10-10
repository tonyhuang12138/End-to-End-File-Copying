#ifndef PACKETTYPES_H
#define PACKETTYPES_H

#define DATA_PACKET_TYPE 1
#define CHECKSUM_PACKET_TYPE 2
#define CHECKSUMCMP_PACKET_TYPE 3

#define MAX_PKT_LEN 512
#define PACKET_TYPE_OFFSET 0
#define PACKET_TYPE_LEN 4

// subject to change
struct DataPacket {
    const int packetType = 1;
    int numTotalPackets;
    int packetNumber;
    char filename[50]; // investigate safety hazard: when buffer size is 1 it still worked; why didn't it overflow?
    // char data[412];
};

struct ChecksumPacket {
    const int packetType = 2;
    char sha1[21];
};

struct ChecksumComparisonPacket {
    const int packetType = 3;
    bool result;
};

#endif
