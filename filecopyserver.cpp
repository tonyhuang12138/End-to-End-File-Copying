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

# define MAX_RETRIES 5

void receiveDataPackets(C150DgmSocket *sock, string dirName, 
                        int filenastiness);
void checksumPhase(C150DgmSocket *sock, string filename, string dirName,  
                  int filenastiness);
void sendChecksumPacket(C150DgmSocket *sock, char filename[], string dirName,  
                        int filenastiness, char outgoingChecksumPacket[], 
                        int outgoingChecksumPacketSize);
void receiveConfirmationPacket(C150DgmSocket *sock, string filename, 
                               char outgoingChecksumPacket[]);
void finishPhase(C150DgmSocket *sock, string filename, string dirName, 
                 int filenastiness);
void renameOrRemove(string filename, string dirName, int filenastiness, 
                    char incomingConfirmationPacket[], 
                    int incomingConfirmationPacketSize);

const int networknastinessArg = 1;        // networknastiness is 1st arg
const int filenastinessArg = 2;           // filenastiness is 2nd arg
const int targetdirArg = 3;               // targetdir is 3rd arg

// my function pointer

  
int main(int argc, char *argv[])  {
    // validate input format
    if (argc != 4) {
        fprintf(stderr,"Correct syntax is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(1);
    }

    //
    //  DO THIS FIRST OR YOUR ASSIGNMENT WON'T BE GRADED!
    //
    GRADEME(argc, argv);

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

        receiveDataPackets(sock, argv[targetdirArg], filenastiness);
    // TODO: can the exception thrown within receiveDataPackets be caught?
    } catch (C150NetworkException& e) { 
        // Write to debug log
        c150debug->printf(C150ALWAYSLOG,"Caught C150NetworkException: %s\n",
                          e.formattedExplanation().c_str());
        // In case we're logging to a file, write to the console too
        cerr << argv[0] << ": caught C150NetworkException: " << e.formattedExplanation() << endl;
    }
    
    return 0;
}

// ------------------------------------------------------
//
//                   receiveDataPackets
//
//  Given an incoming packet, extract and return the 
//  packet type
//     
// ------------------------------------------------------
void receiveDataPackets(C150DgmSocket *sock, string dirName,  
                        int filenastiness) {
    // TODO: don't think this is while 1; need to investigate
    char incomingDataPacket[DATA_PACKET_LEN];   // received message data
    string filename = "";
    ssize_t readlen;             // amount of data read from socket
    int packetType;
    unordered_set<int> packetsReceived;  // tracks which packets of the
                                         // file had been received

    while (1) {
        //
        // TODO: investigate if +1/-1 is necessary
        // Read a packet
        // -1 in size below is to leave room for null
        //
        readlen = sock -> read(incomingDataPacket, sizeof(incomingDataPacket));
        // ignore empty packets
        if (readlen == 0) {
            c150debug->printf(C150APPLICATION,"Read zero length message, trying again");
            continue;
        }

        packetType = getPacketType(incomingDataPacket);
        // ignore unexpected packets
        if (packetType != DATA_PACKET_TYPE) {
            fprintf(stderr,"Should be receiving data packets but packet of packetType %d received.\n", packetType);
            continue;
        }

        DataPacket *dataPacket = reinterpret_cast<DataPacket *>(incomingDataPacket);

        // TODO: remove assert in final submission
        assert(packetType == dataPacket->packetType);

        // check if packet belongs to expected file
        if (filename == "") {
            filename = dataPacket->filename;
        } else if (filename != dataPacket->filename) {
            fprintf(stderr,"Should be receiving packets of file %s, but received packets from file %s\n", filename.c_str(), dataPacket->filename);
            continue;
        }

        printf("Received incoming packet with filename %s\n", filename.c_str());
        
        *GRADING << "File: " << filename << " starting to receive file" << endl;

        packetsReceived.insert(dataPacket->packetNumber);

        // send checksum packet to client when transmission completes
        if (packetsReceived.size() == 
                    (size_t) dataPacket->numTotalPackets) {
            *GRADING << "File: " << filename << " received, beginning end-to-end check" << endl;

            checksumPhase(sock, filename, dirName, filenastiness);

            // reset transmission stats for next file
            filename = "";
            packetsReceived.clear();
        }
    }
}


// ------------------------------------------------------
//
//                   checksum
//
//  Assuming that file has been received in whole, send
//  calculate and send checksum as a packet to client
//     
// ------------------------------------------------------
void checksumPhase(C150DgmSocket *sock, string filename, string dirName,  
                  int filenastiness) {
    assert(sock != NULL);

    char outgoingChecksumPacket[CHECKSUM_PACKET_LEN];
    
    sendChecksumPacket(sock, (char *) filename.c_str(), dirName, filenastiness, 
                       outgoingChecksumPacket, CHECKSUM_PACKET_LEN);
    printf("Sent checksum packet for file %s, retry %d\n", filename.c_str(), 0);

    receiveConfirmationPacket(sock, filename, outgoingChecksumPacket);
}

// ------------------------------------------------------
//
//                   sendChecksumPacket
//
//  Assuming that file has been received in whole, send
//  calculate and send checksum as a packet to client
//     
// ------------------------------------------------------
void sendChecksumPacket(C150DgmSocket *sock, char filename[], string dirName,  
                        int filenastiness, char outgoingChecksumPacket[], 
                        int outgoingChecksumPacketSize) {
    assert(sock != NULL);
    assert(filename != NULL);
    
    printf("In sendChecksumPacket with filename %s, directory name %s and file nastiness %d\n", filename, dirName.c_str(), filenastiness);
    ChecksumPacket checksumPacket;
    unsigned char checksum[HASH_CODE_LENGTH];

    // set filename
    memcpy(checksumPacket.filename, filename, strlen(filename) + 1);
    cout << "strcmp " << strcmp(filename, checksumPacket.filename) << endl;
    cout << checksumPacket.packetType << " " << checksumPacket.filename << endl;

    // HERE!
    string tempFilename = filename;
    tempFilename += "-TMP";

    printf("Original filename is %s, adjusted temp filename is %s\n", filename, tempFilename.c_str());
    
    sha1(tempFilename, dirName, filenastiness, checksum);
    
    printf("Printing calculated checksum: ");
    for (int i = 0; i < 20; i++)
    {
        printf ("%02x", (unsigned int) checksum[i]);
    }
    printf ("\n");

    // TODO: +1??
    memcpy(checksumPacket.checksum, checksum, HASH_CODE_LENGTH);

    printf("Printing calculated checksum: ");
    for (int i = 0; i < 20; i++)
    {
        printf ("%02x", (unsigned int) checksumPacket.checksum[i]);
    }
    printf ("\n");

    // TODO: remove
    assert(memcmp(checksumPacket.checksum, checksum, HASH_CODE_LENGTH) == 0);

    memcpy(outgoingChecksumPacket, &checksumPacket, sizeof(checksumPacket));
    sock->write(outgoingChecksumPacket, outgoingChecksumPacketSize);
}


void receiveConfirmationPacket(C150DgmSocket *sock, string filename, 
                               char outgoingChecksumPacket[]) {
    assert(sock != NULL);

    ssize_t readlen;              // amount of data read from socket
    char incomingConfirmationPacket[DATA_PACKET_LEN];
    int retry_i = 0;
    bool timeoutStatus;
    int packetType;

    readlen = sock -> read(incomingConfirmationPacket, sizeof(incomingConfirmationPacket));
    timeoutStatus = sock -> timedout();

    // validate size of received packet
    if (readlen == 0) {
        c150debug->printf(C150APPLICATION,"Read zero length message, trying again");
        timeoutStatus = true;
    }

    // keep resending message up to MAX_RETRIES times when read timedout
    while (timeoutStatus == true && retry_i < MAX_RETRIES) {
        retry_i++;

        // Send the message to the server
        // c150debug->printf(C150APPLICATION,"%s: Writing message: \"%s\"",
        //                 argv[0], outgoingMsg);
        sock -> write(outgoingChecksumPacket, CONFIRMATION_PACKET_LEN);
        printf("Sent checksum packet for file %s, retry %d\n", filename.c_str(), retry_i);

        // Read the response from the server
        // c150debug->printf(C150APPLICATION,"%s: Returned from write, doing read()", argv[0]);
        readlen = sock -> read(incomingConfirmationPacket, 
                                sizeof(incomingConfirmationPacket));
        timeoutStatus = sock -> timedout();
    }

    // throw exception if all retries exceeded
    if (retry_i == MAX_RETRIES) {
        throw C150NetworkException("Timed out after 5 retries.");
    }

    // write flush logic

    // validate packet type
    packetType = getPacketType(incomingConfirmationPacket);
    if (packetType != CONFIRMATION_PACKET_TYPE) { 
        fprintf(stderr,"Should be receiving checksum confirmation packets but packet of packetType %d received.\n", packetType);
        timeoutStatus = true;
    }

    // TODO: compute hash again after writing
    // finishPhase(sock, filename, dirName, filenastiness, incomingConfirmationPacket, CHECKSUM_PACKET_LEN);
    // File: <name> end-to-end check succeeded
    // File: <name> end-to-end check failed
}


void finishPhase(C150DgmSocket *sock, string filename, string dirName, 
                 int filenastiness, char incomingConfirmationPacket[], 
                 int incomingConfirmationPacketSize) {
    assert(sock != NULL);

    int packetType;
    char outgoingFinishPacket[FINISH_PACKET_LEN];

    // validate packet type
    packetType = getPacketType(incomingConfirmationPacket);
    if (packetType != CHECKSUM_PACKET_TYPE) { 
        fprintf(stderr,"Should be receiving checksum confirmation packets but packet of packetType %d received.\n", packetType);
        return; // retry ???
    }

    renameOrRemove(filename, dirName, filenastiness, incomingConfirmationPacket, incomingConfirmationPacketSize);

    (void) outgoingFinishPacket;
    // sendFinishPacket(sock, (char *) filename.c_str(), dirName, filenastiness, outgoingFinishPacket, FINISH_PACKET_LEN);
    // printf("Sent finish packet for file %s, retry %d\n", filename.c_str(), 0);
}


void renameOrRemove(string filename, string dirName, int filenastiness, 
                    char incomingConfirmationPacket[], 
                    int incomingConfirmationPacketSize) {
    cout << "In rename or remove\n";
    
    NASTYFILE file(filenastiness);
    ConfirmationPacket *confirmationPacket = reinterpret_cast<ConfirmationPacket *>(incomingConfirmationPacket);

    // TODO: check if the filename matches with current file
    if (strcmp((char *) filename.c_str(), confirmationPacket->filename) != 0){
        cout << memcmp((char *) filename.c_str(), confirmationPacket->filename, FILENAME_LEN);
        fprintf(stderr,"Filename inconsistent when comparing hash. Expected file %s but received file %s\n", filename.c_str(), confirmationPacket->filename);
        return; // ??
    }

    // // opem file, retry if failed?
    // fopenretval = inputFile.fopen(sourceName.c_str(), "rb");

    // if (fopenretval == NULL) {
    //   cerr << "Error opening input file " << sourceName << 
    //     " errno=" << strerror(errno) << endl;
    //   exit(12);
    // }

    // check if file transfer was successful
    if (confirmationPacket->result) {
        // rename

        // check if rename success
    } else {
        // remove
    }
}

