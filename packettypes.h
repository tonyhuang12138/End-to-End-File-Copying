#ifndef PACKETTYPES_H
#define PACKETTYPES_H

#include "sha1.h"

#define BEGIN_REQUEST_PACKET_TYPE 0
#define BEGIN_RESPONSE_PACKET_TYPE 1
#define DATA_PACKET_TYPE 2
#define CC_REQUEST_PACKET_TYPE 3
#define CC_RESPONSE_PACKET_TYPE 4
#define CS_REQUEST_PACKET_TYPE 5
#define CS_RESPONSE_PACKET_TYPE 6
#define CS_COMPARISON_PACKET_TYPE 7
#define FINISH_PACKET_TYPE 8

#define MAX_PACKET_LEN 512
#define PACKET_TYPE_LEN 4
#define FILENAME_LEN 50
#define DATA_LEN 440 // 512 - 4 - 50 - 8 - 4 (and padding)

#define CHUNK_SIZE 8

// client to server
struct BeginRequestPacket {
    const int packetType = 0;
    char filename[FILENAME_LEN];
    size_t fileSize;
    size_t numTotalPackets;
    size_t numTotalChunks;
};

// server to client
struct BeginResponsePacket {
    const int packetType = 1;
    char filename[FILENAME_LEN];
};

// client to server
struct DataPacket {
    const int packetType = 2;
    // investigate safety hazard: when buffer size is 1 it still worked; why didn't it overflow?
    char filename[FILENAME_LEN]; 
    size_t chunkNumber;
    int packetNumber;
    unsigned char data[DATA_LEN];
};

// client to server
struct ChunkCheckRequestPacket {
    const int packetType = 3;
    char filename[FILENAME_LEN]; 
    size_t chunkNumber;
    int numPacketsInChunk;
};

// server to client
struct ChunkCheckResponsePacket {
    const int packetType = 4;
    char filename[FILENAME_LEN]; 
    size_t chunkNumber;
    int numPacketsInChunk; // only check numPacketsInChunk packets
    bool chunkCheck[CHUNK_SIZE]; 
};

// client to server
struct ChecksumRequestPacket {
    const int packetType = 5;
    char filename[FILENAME_LEN]; 
};

// server to client
struct ChecksumResponsePacket {
    const int packetType = 6;
    char filename[FILENAME_LEN];
    unsigned char checksum[HASH_CODE_LENGTH];
};

// client to server
struct ChecksumComparisonPacket {
    const int packetType = 7;
    char filename[FILENAME_LEN];
    bool comparisonResult;
};

// server to client
struct FinishPacket {
    const int packetType = 8;
    char filename[FILENAME_LEN];
};

#endif
