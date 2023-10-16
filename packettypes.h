#ifndef PACKETTYPES_H
#define PACKETTYPES_H

#include "sha1.h"

#define DATA_PACKET_TYPE 1
#define CHUNK_CHECK_PACKET_TYPE 2
#define CS_REQUEST_PACKET_TYPE 3
#define CS_RESPONSE_PACKET_TYPE 4
#define CS_COMPARISON_PACKET_TYPE 5
#define FINISH_PACKET_TYPE 6

#define MAX_PACKET_LEN 512
#define PACKET_TYPE_LEN 4
#define FILENAME_LEN 50
#define DATA_LEN 450 // 512 - 4 - 50 - 4 - 4

#define CHUNK_SIZE 8

// client to server
struct DataPacket {
    const int packetType = 1;
    // investigate safety hazard: when buffer size is 1 it still worked; why didn't it overflow?
    char filename[FILENAME_LEN]; 
    int numTotalPackets;
    int packetNumber;
    char data[DATA_LEN];
};

// server to client
struct ChunkCheckPacket {
    const int packetType = 2;
    char filename[FILENAME_LEN]; 
    int cycleNumber;
    char chunkCheck[CHUNK_SIZE];
};

// client to server
struct ChecksumRequestPacket {
    const int packetType = 3;
    char filename[FILENAME_LEN]; 
};

// server to client
struct ChecksumResponsePacket {
    const int packetType = 4;
    char filename[FILENAME_LEN];
    unsigned char checksum[HASH_CODE_LENGTH];
};

// client to server
struct ChecksumComparisonPacket {
    const int packetType = 5;
    char filename[FILENAME_LEN];
    bool comparisonResult;
};

// server to client
struct FinishPacket {
    const int packetType = 6;
    char filename[FILENAME_LEN];
};

#endif
