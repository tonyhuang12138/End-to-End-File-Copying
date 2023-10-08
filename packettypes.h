#ifndef PACKETTYPES_H
#define PACKETTYPES_H

struct Packet {
    int type;                   //TODO: decide the format for type indication
    char filename[100];
};

struct BeginTransmissionPacket : public Packet {
    int type = 0;               //Question: should we do this in child struct?
};

struct ChecksumPacket : public Packet {
    int type = 1;
    char sha1[21];
};

struct ChecksumComparisonPacket : public Packet {
    int type = 2;
    bool result;
};

// subject to change
struct DataPacket {
    int type = 3;
    char header[100];
    char data[412];
};

#endif