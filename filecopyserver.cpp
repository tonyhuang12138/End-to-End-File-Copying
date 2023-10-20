// 
//            filecopyclient.cpp
//
//     Author: Tony Huang and Bill Liu


//    COMMAND LINE
//
//          fileserver <networknastiness> <filenastiness> <targetdir>
//
//     The server should do its part of the end of the end-to-end check, and 
//     inform the client. (Exactly how to split the checking between server 
//     and client is a design decision you will have to make.)
// 
//     The server should acknowledge to the client, and if the client times out 
//     waiting, it should resend the confirmation. (The confirmation is idempotent!). 
//     Of course, that means the server needs to quietly flush any duplicate 
//     confirmations it might get too. The client should retry several times, and if 
//     none of those are acknowledged, it should declare the network down and give up.
//
//     References:
//     - https://www.youtube.com/watch?v=xvFZjo5PgG0&ab_channel=Duran
//


#include "packettypes.h"
#include "c150nastyfile.h"        // for c150nastyfile & framework
#include "c150grading.h"
#include "c150nastydgmsocket.h"
#include "c150debug.h"
#include <iostream>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <bits/stdc++.h>
#include "sha1.h"
#include "nastyfileio.h"
#include <string>

using namespace std;          // for C++ std library
using namespace C150NETWORK;  // for all the comp150 utilities 

# define MAX_RETRIES 10

void receivePackets(C150DgmSocket *sock, string dirName,  
                    int filenastiness);
// begin phase
void sendBeginResponse(C150DgmSocket *sock, char filename[]);

// receive data/chunk phase
void readDataPacket(DataPacket *dataPacket, bool bytemap[], 
                    size_t fileSize, unsigned char fileBuffer[]);

// chunk check phase
void sendChunkCheckResponse(C150DgmSocket *sock, bool *bytemap,
                            ChunkCheckRequestPacket *requestPacket);

// checksum phase
void sendChecksumResponse(C150DgmSocket *sock, char filename[], 
                          string dirName, int filenastiness);
void receiveConfirmationPacket(C150DgmSocket *sock, char filename[], 
                               string dirName, int filenastiness,
                               char outgoingChecksumPacket[]);

// finish phase
void writeFileBufferToDisk(char filename[], string dirName, int filenastiness, 
                           size_t fileSize, unsigned char *fileBuffer);
void renameOrRemove(char filename[], string dirName, int filenastiness, 
                    char incomingPacket[]);
void sendFinishPacket(C150DgmSocket *sock, char filename[]);

// utility functions
bool validatePacket(char incomingPacket[], char filename[], int packetType, 
                    int readlen);


const int networknastinessArg = 1;        // networknastiness is 1st arg
const int filenastinessArg = 2;           // filenastiness is 2nd arg
const int targetdirArg = 3;               // targetdir is 3rd arg

  
int main(int argc, char *argv[])  {
    //
    //  DO THIS FIRST OR YOUR ASSIGNMENT WON'T BE GRADED!
    //
    GRADEME(argc, argv);

    // validate input format
    if (argc != 4) {
        fprintf(stderr,"Correct syntax is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(1);
    }

    // read nastiness from input
    int networknastiness = atoi(argv[networknastinessArg]); 
    int filenastiness = atoi(argv[filenastinessArg]);

    // create socket with given nastiness
    C150DgmSocket *sock = new C150NastyDgmSocket(networknastiness);
    sock -> turnOnTimeouts(3000);
    
    try {
        // check if target dir exists
        DIR *SRC = opendir(argv[targetdirArg]);
        if (SRC == NULL) {
            fprintf(stderr,"Error opening target directory %s\n", argv[targetdirArg]);     
            exit(8);
        } 
        closedir(SRC);

        // receiveDataPackets(sock, argv[targetdirArg], filenastiness);
        receivePackets(sock, argv[targetdirArg], filenastiness);
    } catch (C150NetworkException& e) { 
        // Write to debug log
        c150debug->printf(C150ALWAYSLOG,"Caught C150NetworkException: %s\n",
                          e.formattedExplanation().c_str());
        // In case we're logging to a file, write to the console too
        cerr << argv[0] << ": caught C150NetworkException: " << e.formattedExplanation() << endl;
    }
    
    return 0;
}



void receivePackets(C150DgmSocket *sock, string dirName,  
                    int filenastiness) {
    // variables that keep track of the current state of the server
    char incomingPacket[MAX_PACKET_LEN];   // buffer for message data
    char incomingFilename[FILENAME_LEN];   // buffer for message filename
    int packetType;                        // packet type of message
    ssize_t readlen, fileSize;             // curr file info
    size_t currChunkNum = 0;               // chunk being currently processed
    char currFilename[FILENAME_LEN] = "";  // file being currently processed
    bool bytemap[CHUNK_SIZE] = {0}; // receive status of packets of currently 
                                    // processing file
    unsigned char *fileBuffer;      // buffer for currently reading file
    bool startOfFile = true;        // flag to prevent reinitiating file

    // buffers from different packet types; interpreted in advance to check 
    // chunk number
    BeginRequestPacket *requestPacketPacket;  
    DataPacket *dataPacket;
    ChunkCheckRequestPacket *requestPacket;
    
    // listen for packets from client forever
    while (1) {
        readlen = sock -> read(incomingPacket, MAX_PACKET_LEN);

        // validate packet invariants
        packetType = getPacketType(incomingPacket);
        getFilename(incomingPacket, incomingFilename);
        if (!validatePacket(incomingPacket, currFilename, packetType, readlen)) continue;

        // pattern match behavior on client delivered packets
        switch (packetType) {
            case BEGIN_REQUEST_PACKET_TYPE:
                requestPacketPacket = reinterpret_cast<BeginRequestPacket *>(incomingPacket);

                // start new file and initialize state variables
                if (startOfFile) {
                    strcpy(currFilename, incomingFilename);
                    fileSize = requestPacketPacket->fileSize;
                    fileBuffer = (unsigned char *) malloc(fileSize + 1);
                    startOfFile = false;
                    currChunkNum = 0;
                    memset(bytemap, 0, CHUNK_SIZE);
                }

                // notify client that server is ready for file transfer
                sendBeginResponse(sock, currFilename);
                break;

            case DATA_PACKET_TYPE:
                dataPacket = reinterpret_cast<DataPacket *>(incomingPacket);

                // validate chunk number
                if (dataPacket->chunkNumber < currChunkNum) {
                    continue;
                } else if (dataPacket->chunkNumber > currChunkNum) {
                    memset(bytemap, 0, CHUNK_SIZE);
                    currChunkNum++;
                }

                // write data to buffer
                readDataPacket(dataPacket, bytemap, fileSize, fileBuffer);
                break;

            case CC_REQUEST_PACKET_TYPE: // chunk check request
                requestPacket = reinterpret_cast<ChunkCheckRequestPacket *>(incomingPacket);

                // validate chunk number
                if (requestPacket->chunkNumber != currChunkNum) continue;

                // process chunk check request and respond accordingly
                sendChunkCheckResponse(sock, bytemap, requestPacket);
                break;

            case CS_REQUEST_PACKET_TYPE: // checksum request                
                *GRADING << "File: " << currFilename << " received, beginning end-to-end check" << endl;
                
                // flush if already written and freed
                if (fileBuffer == NULL) break;

                writeFileBufferToDisk(currFilename, dirName, filenastiness,fileSize, fileBuffer);

                // process checksum request and respond accordingly
                sendChecksumResponse(sock, currFilename, dirName, filenastiness);
                break;

            case CS_COMPARISON_PACKET_TYPE: // checksum comparison
                // perform necessary filename checks
                renameOrRemove(currFilename, dirName, filenastiness, incomingPacket);

                sendFinishPacket(sock, currFilename);

                // reset state variables
                if (fileBuffer != NULL) {
                    free(fileBuffer);
                    fileBuffer = NULL;
                }

                startOfFile = true;
                currChunkNum = 0;
                memset(bytemap, 0, CHUNK_SIZE);
                break;

            default:
                fprintf(stderr, "Invalid packet type %d received.\n", packetType);
                break;
        }
    }
}


// --------------------------------------------------------------------------
//
//                           sendBeginResponse
//
//  Inform client that begin request was received
//     
// --------------------------------------------------------------------------
void sendBeginResponse(C150DgmSocket *sock, char filename[]) {
    assert(sock != NULL);
    assert(filename != NULL);

    // buffers for outgoing request and incoming request
    char outgoingResponsePacket[MAX_PACKET_LEN];
    BeginResponsePacket responsePacket;

    // load struct with members and write to packet
    memcpy(responsePacket.filename, filename, strlen(filename) + 1);
    memcpy(outgoingResponsePacket, &responsePacket, sizeof(responsePacket));

    // send packet to server
    sock -> write(outgoingResponsePacket, MAX_PACKET_LEN);
}


// --------------------------------------------------------------------------
//
//                           readDataPacket
//
//  Given a data packet, write it to buffer
//     
// --------------------------------------------------------------------------
void readDataPacket(DataPacket *dataPacket, bool bytemap[], 
                    size_t fileSize, unsigned char fileBuffer[]) {
    assert(dataPacket != NULL);
    assert(bytemap != NULL);

    // flush if file was already freed
    if (fileBuffer == NULL) return;

    // skip write if already written
    if (bytemap[dataPacket->packetNumber] == true) return;

    // write data to buffer
    size_t offset = (dataPacket->chunkNumber * CHUNK_SIZE + dataPacket->packetNumber) * DATA_LEN;
    size_t writeAmount = (offset + DATA_LEN < fileSize) ? DATA_LEN : fileSize - offset;
    memcpy(fileBuffer + offset, dataPacket->data, writeAmount);

    // mark packet as delivered
    bytemap[dataPacket->packetNumber] = true;
}


// --------------------------------------------------------------------------
//
//                           sendChunkCheckResponse
//
//  Given a chunk of a file, inform client which packets in the chunk had not
//  been received
//     
// --------------------------------------------------------------------------
void sendChunkCheckResponse(C150DgmSocket *sock, bool *bytemap,
                            ChunkCheckRequestPacket *requestPacket) {
    assert(sock != NULL);
    assert(bytemap != NULL);
    assert(requestPacket != NULL);

    // buffers for outgoing request and incoming request
    char outgoingResponsePacket[MAX_PACKET_LEN];
    char chunkCheck[CHUNK_SIZE]; 
    ChunkCheckResponsePacket responsePacket;

    // load struct with members and write to packet
    memcpy(responsePacket.filename, requestPacket->filename, strlen(requestPacket->filename) + 1);
    memcpy(responsePacket.chunkCheck, bytemap, CHUNK_SIZE);
    responsePacket.chunkNumber = requestPacket->chunkNumber;
    responsePacket.numPacketsInChunk = requestPacket->numPacketsInChunk;
    memcpy(outgoingResponsePacket, &responsePacket, sizeof(responsePacket));
    
    // send packet to server
    sock->write(outgoingResponsePacket, MAX_PACKET_LEN);
}


// --------------------------------------------------------------------------
//
//                           sendChecksumResponse
//
//  Assuming that file has been received in whole, calculate checksum of 
//  received file and send it to client
//     
// --------------------------------------------------------------------------
void sendChecksumResponse(C150DgmSocket *sock, char filename[], 
                          string dirName, int filenastiness) {
    assert(sock != NULL);
    assert(filename != NULL);

    char outgoingResponsePacket[MAX_PACKET_LEN]; // buffer for outgoing request
    ChecksumResponsePacket responsePacket;
    unsigned char *checksum;

    // generate filename for temporary file
    string tempFilename = filename;
    tempFilename += "-TMP";

    checksum = findMostFrequentSHA(tempFilename, dirName, filenastiness);

    // load struct with members and write to packet
    memcpy(responsePacket.filename, filename, strlen(filename) + 1);
    memcpy(responsePacket.checksum, checksum, HASH_CODE_LENGTH);
    memcpy(outgoingResponsePacket, &responsePacket, sizeof(responsePacket));

    // send packet to server
    sock->write(outgoingResponsePacket, MAX_PACKET_LEN);

    free(checksum);
}


// --------------------------------------------------------------------------
//
//                           writeFileBufferToDisk
//
//  Given some data in buffer, write it to file and try to ensure that it
//  was written correctly
//     
// --------------------------------------------------------------------------
void writeFileBufferToDisk(char filename[], string dirName, int filenastiness, 
                           size_t fileSize, unsigned char *fileBuffer) {
    assert(filename != NULL);

    // flush message if buffer was already freed
    if (fileBuffer == NULL) return;

    // file IO variables
    NASTYFILE outputFile(filenastiness); // constructs a new file pointer
    void *fopenretval;                   // 
    size_t writeLen, readLen;
    int numRetried = 0;
    unsigned char *diskread = NULL;      // buffer for reading data that was 
                                         // just wrote
    (void) fopenretval;

    // generate filename for temporary file
    string tempFilename = filename;
    tempFilename += "-TMP";
    string outputpath = makeFileName(dirName, tempFilename);

    // write up to MAX_RETRIES times if data was not correctly written
    while (diskread == NULL || ((writeLen != fileSize || readLen != fileSize 
           || memcmp(fileBuffer, diskread, fileSize) != 0) 
           && numRetried < MAX_RETRIES)) {
        numRetried++;
        
        // reset diskread pointer
        if (diskread != NULL) {
            free(diskread);
            diskread = NULL;
        }
    
        // open and write entire file
        fopenretval = outputFile.fopen(outputpath.c_str(), "wb");  
        writeLen = outputFile.fwrite(fileBuffer, 1, fileSize);

        // retry if write error
        if (writeLen != fileSize || outputFile.fclose() != 0) continue;
    
        // read data that was just wrote to file
        diskread = bufferFile(dirName.c_str(), tempFilename, filenastiness, 
                              &readLen);

        // retry if read error
        if (readLen != fileSize) continue;
    }

    if (numRetried == MAX_RETRIES) {
        fprintf(stderr, "WRITE FAILED: %s after %d tries.\n", filename, numRetried);
    }

    free(diskread);
}


// --------------------------------------------------------------------------
//
//                           renameOrRemove
//
//  Read checksum comparison result of file between SRC and TARGET. If they
//  agree, rename the temporary file; else, remove it
//     
// --------------------------------------------------------------------------
void renameOrRemove(char filename[], string dirName, int filenastiness, 
                    char incomingPacket[]) {
    assert(filename != NULL);
    assert(incomingPacket != NULL);
    
    // interpreting incoming packet
    ChecksumComparisonPacket *comparisonPacket = reinterpret_cast<ChecksumComparisonPacket *>(incomingPacket);

    // generate filename for temporary file
    string fullPath = makeFileName(dirName, filename);
    string tempFullPath = fullPath + "-TMP";

    // flush if wrong file
    if (strcmp((char *) filename, comparisonPacket->filename) != 0) return;

    // check if file transfer was successful
    if (comparisonPacket->comparisonResult) {  // rename
        *GRADING << "File: " << filename << " end-to-end check succeeded" << endl;

        // check if rename success
        if (rename(tempFullPath.c_str(), fullPath.c_str()) != 0) {
            fprintf(stderr, "Error renaming file %s to %s\n", 
                    tempFullPath.c_str(), fullPath.c_str());
        } 
    } else {                                    // remove
        *GRADING << "File: " << filename << " end-to-end check failed" << endl;                    

        if (remove(tempFullPath.c_str()) != 0) {
            fprintf(stderr, "Unable to remove the file\n");
        }
    }
}


// --------------------------------------------------------------------------
//
//                           sendFinishPacket
//
//  Notify client that the current file finished processing
//     
// --------------------------------------------------------------------------
void sendFinishPacket(C150DgmSocket *sock, char filename[]) {
    assert(sock != NULL);
    assert(filename != NULL);
    
    // buffer for outgoing request
    char outgoingFinishPacket[MAX_PACKET_LEN];
    FinishPacket finishPacket;

    // load struct with members and write to packet
    memcpy(finishPacket.filename, filename, strlen(filename) + 1);
    memcpy(outgoingFinishPacket, &finishPacket, sizeof(finishPacket));

    // send packet to server
    sock -> write(outgoingFinishPacket, MAX_PACKET_LEN);
}


// --------------------------------------------------------------------------
//
//                           validatePacket
//
//  Given an incoming packet, validate that it is not empty, is of the correct
//  file and has the correct packet type
//     
// --------------------------------------------------------------------------
bool validatePacket(char incomingPacket[], char filename[], int packetType, 
                    int readlen) {
    assert(filename != NULL);
    assert(incomingPacket != NULL);
    
    char receivedFilename[FILENAME_LEN];
    int receivedPacketType;   

    // validate size of received packet
    if (readlen == 0) return false;

    // skip name check for begin packets as they provide a new filename
    if (packetType == BEGIN_REQUEST_PACKET_TYPE) return true;

    // validate filename
    getFilename(incomingPacket, receivedFilename);
    if (strcmp(filename, "") != 0 && strcmp(receivedFilename, filename) != 0) { 
        fprintf(stderr,"Should be receiving packet for file %s but received packets for file %s instead.\n", filename, receivedFilename);
        return false;
    }

    return true;
}
