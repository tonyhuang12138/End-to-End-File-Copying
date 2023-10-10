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
// #include <string.h>

using namespace std;          // for C++ std library
using namespace C150NETWORK;  // for all the comp150 utilities 

void receiveDataPackets(NASTYFILE *output, C150DgmSocket **sock);
int getPacketType(char incomingPacket[]);

const int networknastinessArg = 1;        // networknastiness is 1st arg
const int filenastinessArg = 2;           // filenastiness is 2nd arg
const int targetdirArg = 3;               // targetdir is 3rd arg

// my function pointer

  
int main(int argc, char *argv[])  {
    //
    //  DO THIS FIRST OR YOUR ASSIGNMENT WON'T BE GRADED!
    //
    GRADEME(argc, argv);

    // create file pointer with given nastiness
    int networknastiness = atoi(argv[networknastinessArg]); 
    int filenastiness = atoi(argv[filenastinessArg]);

    // create file stream and socket
    NASTYFILE output(filenastiness);
    C150DgmSocket *sock = new C150NastyDgmSocket(networknastiness);

    // validate input format
    if (argc != 4) {
        fprintf(stderr,"Correct syntax is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(1);
    }

    try {
        // open target dir
        DIR *SRC = opendir(argv[targetdirArg]);
        if (SRC == NULL) {
            fprintf(stderr,"Error opening target directory %s\n", argv[targetdirArg]);     
            exit(8);
        } 
        
        receiveDataPackets(&output, &sock);
        closedir(SRC);
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
void receiveDataPackets(NASTYFILE *output, C150DgmSocket **sock) {
    // TODO: don't think this is while 1; need to investigate
    char incomingPacket[MAX_PKT_LEN];   // received message data
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
        readlen = (*sock) -> read(incomingPacket, sizeof(incomingPacket));
        // validate received packet
        if (readlen == 0) {
            c150debug->printf(C150APPLICATION,"Read zero length message, trying again");
            continue;
        }

        packetType = getPacketType(incomingPacket);
        // validate packet type
        if (packetType != DATA_PACKET_TYPE) {
            fprintf(stderr,"Should be receiving data packets but packet of packetType %d received.\n", packetType);
            // TODO: remove for final submission
            // exit(100);
            continue;
        }

        DataPacket *dataPacket = reinterpret_cast<DataPacket *>(incomingPacket);

        // TODO: remove assert in final submission
        assert(packetType == dataPacket->packetType);

        // check if packet belongs to expected file
        if (filename == "") {
            filename = dataPacket->filename;
        } else if (filename != dataPacket->filename) {
            fprintf(stderr,"Should be receiving packets of file %s, but received packets from file %s\n", filename.c_str(), dataPacket->filename);
        }

        printf("Received incoming packet with filename %s\n", filename.c_str());
        
        *GRADING << "File: " << filename << " starting to receive file" << endl;

        packetsReceived.insert(dataPacket->packetNumber);

        // send checksum packet to client when transmission 
        // completes
        if (packetsReceived.size() == 
                    (size_t) dataPacket->numTotalPackets) {
            *GRADING << "File: " << filename << " received, beginning end-to-end check" << endl;

            // sendChecksumPacket();

            // reset transmission stats for next file
            filename = "";
            packetsReceived.clear();
        }

        // begin end-to-end check
        // send msg and wait for timeout to counter network nastiness
    }
}


// ------------------------------------------------------
//
//                   getPacketType
//
//  Given an incoming packet, extract and return the 
//  packet type
//     
// ------------------------------------------------------
int getPacketType(char incomingPacket[]) {
    int packetType;
    memcpy(&packetType, incomingPacket, sizeof(int));
    printf("packetTypeArr %s %d\n", incomingPacket, packetType);
    
    return packetType;
}

// ------------------------------------------------------
//
//                   sendChecksumPacket
//
//  Assuming that file has been received in whole, send
//  calculate and send checksum as a packet to client
//     
// ------------------------------------------------------
// void sendChecksumPacket() {
//     // File: <name> end-to-end check succeeded
//     // File: <name> end-to-end check failed


//     while (1) { // Question: can we modularize read packet later?
//         //
//         // Read a packet
//         // -1 in size below is to leave room for null
//         //
//         readlen = sock -> read(incomingPacket, sizeof(incomingPacket));

//         if (readlen == 0) {
//             c150debug->printf(C150APPLICATION,"Read zero length message, trying again");
//             continue;
//         }

//         packetType = getPacketType(incomingPacket);

//         if (packetType != CHECKSUMCMP_PACKET_TYPE) { 
//             fprintf(stderr,"Should be receiving data packets but packet of packetType %d received.\n", filename.c_str(), packetType);
//         }
//         // Note: this is the num for ChecksumComparisonPacket
//         // TODO: Compute Sha1code for the local file

//         // TODO: Send a checksum packet back

//         // TODO: wait (loop here) for the client response 
//     }
// }

// ------------------------------------------------------
//
//                   computeSha1code
//
//  Given a filename, compute the Sha1code of that file
//     
// ------------------------------------------------------

string computeSha1code (string filename, int nastiness) {
    C150NastyFile stream(nastiness);

    return "";
}
