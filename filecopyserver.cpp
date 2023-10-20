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
bool validatePacket(char incomingPacket[], char filename[], int readlen);


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

    // create file pointer with given nastiness
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
    char incomingPacket[MAX_PACKET_LEN];   // received message data
    char currFilename[FILENAME_LEN] = "";
    char incomingFilename[FILENAME_LEN];
    ssize_t readlen, fileSize, numTotalPackets, numTotalChunks;
    int packetType;
    size_t currChunkNum = 0;
    bool bytemap[CHUNK_SIZE] = {0}; // for current chunk
    unsigned char *fileBuffer;
    bool startOfFile = true;

    BeginRequestPacket *requestPacketPacket;
    DataPacket *dataPacket;
    ChunkCheckRequestPacket *requestPacket;

    
    cout << "INITIAL BYTEMAP: Printing out bytemap for chunk " << currChunkNum << " : ";
    for (bool b: bytemap) {
        printf("%s ", b ? "true" : "false");
    }
    cout << endl;
    
    // listen forever
    while (1) {
        // validate size of received packet
        readlen = sock -> read(incomingPacket, MAX_PACKET_LEN);

        // validate filename: not here!
        getFilename(incomingPacket, incomingFilename);
        printf("Received packet of filename %s\n", incomingFilename);

        // validate packet invariants
        if (!validatePacket(incomingPacket, currFilename, readlen)) continue;

        packetType = getPacketType(incomingPacket);
        switch (packetType) {
            case BEGIN_REQUEST_PACKET_TYPE:
                printf(" * * * Begin request packet received. * * * \n");
                requestPacketPacket = reinterpret_cast<BeginRequestPacket *>(incomingPacket);
                // start new file
                if (startOfFile) {
                    strcpy(currFilename, incomingFilename);

                    fileSize = requestPacketPacket->fileSize;
                    numTotalPackets = requestPacketPacket->numTotalPackets;
                    numTotalChunks = requestPacketPacket->numTotalChunks;
                    fileBuffer = (unsigned char *) malloc(fileSize + 1);
                    startOfFile = false;
                    currChunkNum = 0;
                    memset(bytemap, 0, CHUNK_SIZE);

                    *GRADING << "File: " << currFilename << " starting to receive file, expecting a total of " << numTotalPackets << " data packets delivered in " << numTotalChunks << " chunks of size " << CHUNK_SIZE << endl;

                }
                sendBeginResponse(sock, currFilename);
                break;

            case DATA_PACKET_TYPE:
                printf(" * * * Data packet received. * * * \n");
                dataPacket = reinterpret_cast<DataPacket *>(incomingPacket);

                // validate chunk number
                if (dataPacket->chunkNumber < currChunkNum) {
                    printf("FLUSHING: should be receiving data of chunk number %ld but received %ld instead\n", currChunkNum, dataPacket->chunkNumber);
                    continue;
                } else if (dataPacket->chunkNumber > currChunkNum) {
                    memset(bytemap, 0, CHUNK_SIZE);
                    currChunkNum++;
                    printf("NEW CHUNK: start receiving data for new chunk %ld\n", currChunkNum);
                }

                printf("FILENAME: packet has filename %s\n", dataPacket->filename);

                // write to buffer
                readDataPacket(dataPacket, bytemap, fileSize, fileBuffer);

                printf("Double checking flipping result: %d\n", bytemap[dataPacket->packetNumber]);

                cout << "Printing out bytemap for chunk " << currChunkNum << " : ";
                for (bool b: bytemap) {
                    printf("%s ", b ? "true" : "false");
                }
                cout << endl;

                break;

            case CC_REQUEST_PACKET_TYPE:
                printf(" * * * Chunk check request packet received. * * * \n");
                requestPacket = reinterpret_cast<ChunkCheckRequestPacket *>(incomingPacket);
                printf("Current chunk number is: %ld\n", currChunkNum);

                // validate chunk number
                if (requestPacket->chunkNumber != currChunkNum) {
                    printf("Error: should be receiving chunk check request of  chunk number %ld but received %ld instead\n", currChunkNum, requestPacket->chunkNumber);
                    continue;
                }

                sendChunkCheckResponse(sock, bytemap, requestPacket);

                printf("BYTEMAP - after chunk check response: ");
                cout << "Printing out bytemap for chunk " << currChunkNum << " : ";
                for (bool b: bytemap) {
                    printf("%s ", b ? "true" : "false");
                }
                cout << endl;

                break;

            case CS_REQUEST_PACKET_TYPE:
                printf(" * * * Checksum request packet received. * * * \n");
                
                // TODO: can we have duplicates logs?
                *GRADING << "File: " << currFilename << " received, beginning end-to-end check" << endl;
                
                // flush if already written and freed
                if (fileBuffer == NULL) break;

                // TODO: add if not written before
                writeFileBufferToDisk(currFilename, dirName, filenastiness,fileSize, fileBuffer);
                sendChecksumResponse(sock, currFilename, dirName, filenastiness);

                printf("Sent checksum packet for file %s\n", currFilename);
                break;

            case CS_COMPARISON_PACKET_TYPE: 
                printf(" * * * Checksum comparison packet received. * * * \n");

                // perform necessary filename checks
                // renameOrRemove(currFilename, dirName, filenastiness, incomingPacket);

                sendFinishPacket(sock, currFilename);

                // reset state variables
                printf("VERIFY: filename reset from %s ", currFilename);
                strcpy(currFilename, "");
                printf("to %s\n", currFilename);
                if (fileBuffer != NULL) {
                    cout << "Freeing" << endl;
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


void sendBeginResponse(C150DgmSocket *sock, char filename[]) {
    assert(sock != NULL);
    assert(filename != NULL);

    char outgoingResponsePacket[MAX_PACKET_LEN];
    BeginResponsePacket responsePacket;

    memcpy(responsePacket.filename, filename, strlen(filename) + 1);
    cout << "strcmp " << strcmp(filename, responsePacket.filename) << endl;
    cout << responsePacket.packetType << " " << responsePacket.filename << endl;
    memcpy(outgoingResponsePacket, &responsePacket, sizeof(responsePacket));

    // write
    sock -> write(outgoingResponsePacket, MAX_PACKET_LEN);
    printf("Sent begin response packet for file %s\n", filename);
}


void readDataPacket(DataPacket *dataPacket, bool bytemap[], 
                    size_t fileSize, unsigned char fileBuffer[]) {
    // TODO: remove assert
    assert(dataPacket != NULL);
    assert(bytemap != NULL);

    printf("Data packet has chunk number %ld and packet number %d\n", dataPacket->chunkNumber, dataPacket->packetNumber);

    // skip write if already written
    if (bytemap[dataPacket->packetNumber] == true) return;

    // write to buffer
    size_t offset = (dataPacket->chunkNumber * CHUNK_SIZE + dataPacket->packetNumber) * DATA_LEN;
    size_t writeAmount = (offset + DATA_LEN < fileSize) ? DATA_LEN : fileSize - offset;

    printf("OFFSET: %ld and write amount: %ld\n", offset, writeAmount);
    memcpy(fileBuffer + offset, dataPacket->data, writeAmount);

    printf("Flipping byte in byte map: %d\n", dataPacket->packetNumber);
    printf("Before flipping %d\n", bytemap[dataPacket->packetNumber]);
    bytemap[dataPacket->packetNumber] = true;
    printf("After flipping %d\n", bytemap[dataPacket->packetNumber]);
    printf("Finish readDataPacket\n");
}


void sendChunkCheckResponse(C150DgmSocket *sock, bool *bytemap,
                            ChunkCheckRequestPacket *requestPacket) {
    assert(sock != NULL);
    assert(bytemap != NULL);
    assert(requestPacket != NULL);

    char outgoingResponsePacket[MAX_PACKET_LEN];
    char chunkCheck[CHUNK_SIZE]; 
    ChunkCheckResponsePacket responsePacket;

    // check if all packets had been delivered
    printf("In chunk check response, printing results: ");
    for (int i = 0; i < requestPacket->numPacketsInChunk; i++) {
        if (bytemap[i] != true) {
            printf("Packet failed: %d\n", i);
            break;
        }
        cout << bytemap[i];
    }
    cout << endl;

    // set filename
    memcpy(responsePacket.filename, requestPacket->filename, strlen(requestPacket->filename) + 1);
    memcpy(responsePacket.chunkCheck, bytemap, CHUNK_SIZE);
    responsePacket.chunkNumber = requestPacket->chunkNumber;
    responsePacket.numPacketsInChunk = requestPacket->numPacketsInChunk;
    memcpy(outgoingResponsePacket, &responsePacket, sizeof(responsePacket));
    
    sock->write(outgoingResponsePacket, MAX_PACKET_LEN);

    printf("Sent chunk check response for %s\n", responsePacket.filename);
}

// ------------------------------------------------------
//
//                   sendChecksumResponse
//
//  Assuming that file has been received in whole, send
//  calculate and send checksum as a packet to client
//     
// ------------------------------------------------------
void sendChecksumResponse(C150DgmSocket *sock, char filename[], 
                          string dirName, int filenastiness) {
    assert(sock != NULL);
    assert(filename != NULL);
    
    printf("In sendChecksumResponse with filename %s, directory name %s and file nastiness %d\n", filename, dirName.c_str(), filenastiness);

    char outgoingResponsePacket[MAX_PACKET_LEN];
    ChecksumResponsePacket responsePacket;
    unsigned char *checksum;
    string tempFilename = filename;
    tempFilename += "-TMP";

    printf("Original filename is %s, adjusted temp filename is %s\n", filename, tempFilename.c_str());

    // set filename
    memcpy(responsePacket.filename, filename, strlen(filename) + 1);

    cout << "Entering sha1\n";
    checksum = findMostFrequentSHA(tempFilename, dirName, filenastiness);

    memcpy(responsePacket.checksum, checksum, HASH_CODE_LENGTH);
    memcpy(outgoingResponsePacket, &responsePacket, sizeof(responsePacket));

    sock->write(outgoingResponsePacket, MAX_PACKET_LEN);

    free(checksum);

    printf("Checksum response package for file %s sent\n", filename);
}


void writeFileBufferToDisk(char filename[], string dirName, int filenastiness, 
                           size_t fileSize, unsigned char *fileBuffer) {
    assert(filename != NULL);
    assert(fileBuffer != NULL);

    if (strcmp(filename, ".hi") == 0) {
        cerr << "SPECIAL: ";
        for (int i = 0; i < 15; i++) {
            cerr << fileBuffer[i];
        }
        cerr << endl;
        fprintf(stderr, "SPECIAL: '%s' %s file size should be %ld\n", fileBuffer, filename, fileSize);
    }

    NASTYFILE outputFile(filenastiness);
    string tempFilename = filename;
    tempFilename += "-TMP";
    string outputpath = makeFileName(dirName, tempFilename);
    void *fopenretval;
    size_t writeLen, readLen;
    int numRetried = 0;
    unsigned char *diskread = NULL;
    (void) fopenretval;

    while (diskread == NULL || ((writeLen != fileSize || readLen != fileSize || memcmp(fileBuffer, diskread, fileSize) != 0) && numRetried < MAX_RETRIES)) {
        numRetried++;

        if (diskread != NULL) {
            fprintf(stderr, "File write %s failed %d times\n", filename, numRetried);
            free(diskread);
            diskread = NULL;
        }

        fopenretval = outputFile.fopen(outputpath.c_str(), "wb");  
    
        // write entire file
        writeLen = outputFile.fwrite(fileBuffer, 1, fileSize);
        fprintf(stdout, "Writelen is %ld and file size shoudl be %ld\n", writeLen, fileSize);
        if (writeLen != fileSize) {
            fprintf(stderr, "Expecting write len of %ld but wrote %ld\n", fileSize, writeLen);
            continue;
        }
    
        if (outputFile.fclose() == 0 ) {
        cout << "Finished writing file " << outputpath.c_str() <<endl;
        } else {
        cerr << "Error closing output file " << outputpath.c_str() << 
            " errno=" << strerror(errno) << endl;
            continue;
        }

        diskread = bufferFile(dirName.c_str(), tempFilename, filenastiness, &readLen);

        fprintf(stdout, "Readlen is %ld and file size shoudl be %ld\n", readLen, fileSize);
        if (readLen != fileSize) {
            fprintf(stderr, "Expecting read len of %ld but read %ld\n", fileSize, readLen);
            continue;
        }
    }

    if (numRetried == MAX_RETRIES) fprintf(stderr, "WRITE FAILED: %s after %d tries.\n", filename, numRetried);

    free(diskread);
}


void renameOrRemove(char filename[], string dirName, int filenastiness, 
                    char incomingPacket[]) {
    assert(filename != NULL);
    assert(incomingPacket != NULL);
    
    cout << "In rename or remove\n";
    ChecksumComparisonPacket *comparisonPacket = reinterpret_cast<ChecksumComparisonPacket *>(incomingPacket);
    string fullPath = makeFileName(dirName, filename);
    string tempFullPath = fullPath + "-TMP";

    if (strcmp((char *) filename, comparisonPacket->filename) != 0){
        cout << memcmp((char *) filename, comparisonPacket->filename, FILENAME_LEN);
        fprintf(stderr,"Filename inconsistent when comparing hash. Expected file %s but received file %s\n", filename, comparisonPacket->filename);
        return; // ??
    }

    // check if file transfer was successful
    if (comparisonPacket->comparisonResult) {  // rename
        *GRADING << "File: " << filename << " end-to-end check succeeded" << endl;
    
        cout << "Renaming" << endl;

        // check if rename success
        if (rename(tempFullPath.c_str(), fullPath.c_str()) != 0) {
            fprintf(stderr, "Error renaming file %s to %s\n", tempFullPath.c_str(), fullPath.c_str());
        } else {
            cout << "File renamed successfully" << endl;
        }
    } else {                                    // remove
        *GRADING << "File: " << filename << " end-to-end check failed" << endl;                    
        cout << "Removing" << endl;
        if (remove(tempFullPath.c_str()) != 0) {
            fprintf(stderr, "Unable to remove the file\n");

        } else {
            printf("File removed successfully\n");
        }
    }
}


void sendFinishPacket(C150DgmSocket *sock, char filename[]) {
    assert(sock != NULL);
    assert(filename != NULL);

    char outgoingFinishPacket[MAX_PACKET_LEN];
    FinishPacket finishPacket;

    memcpy(finishPacket.filename, filename, strlen(filename) + 1);
    memcpy(outgoingFinishPacket, &finishPacket, sizeof(finishPacket));

    // write
    sock -> write(outgoingFinishPacket, MAX_PACKET_LEN);
    printf("Sent finish packet for file %s\n", filename);
}


// --------------------------------------------------------------------------
//
//                           validatePacket
//
//  Given an incoming packet, validate that it is not empty, is of the correct
//  file and has the correct packet type
//     
// --------------------------------------------------------------------------
bool validatePacket(char incomingPacket[], char filename[], int readlen) {
    assert(filename != NULL);
    assert(incomingPacket != NULL);

    char receivedFilename[FILENAME_LEN];
    int receivedPacketType;   

    printf("Validating packet...\n");

    // validate size of received packet
    if (readlen == 0) {
        // fprintf(stderr, "Read zero length message, trying again");
        return false;
    }

    // validate filename
    getFilename(incomingPacket, receivedFilename);
    if (strcmp(filename, "") != 0 && strcmp(receivedFilename, filename) != 0) { 
        fprintf(stderr,"Should be receiving packet for file %s but received packets for file %s instead.\n", filename, receivedFilename);
        return false;
    }

    printf("Packet validated\n");

    return true;
}
