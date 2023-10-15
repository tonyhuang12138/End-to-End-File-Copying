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

void receivePackets(C150DgmSocket *sock, string dirName,  
                    int filenastiness);
void sendChecksumPacket(C150DgmSocket *sock, char filename[], string dirName,  
                        int filenastiness, char outgoingChecksumPacket[], 
                        int outgoingChecksumPacketSize);
void receiveConfirmationPacket(C150DgmSocket *sock, string filename, 
                               string dirName, int filenastiness,
                               char outgoingChecksumPacket[]);
void renameOrRemove(string filename, string dirName, int filenastiness, 
                    char incomingPacket[]);
void sendFinishPacket(C150DgmSocket *sock, string filename);


const int networknastinessArg = 1;        // networknastiness is 1st arg
const int filenastinessArg = 2;           // filenastiness is 2nd arg
const int targetdirArg = 3;               // targetdir is 3rd arg

  
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

        // receiveDataPackets(sock, argv[targetdirArg], filenastiness);
        receivePackets(sock, argv[targetdirArg], filenastiness);
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



void receivePackets(C150DgmSocket *sock, string dirName,  
                    int filenastiness) {
    char incomingPacket[MAX_PACKET_LEN];   // received message data
    string currFilename = "";
    string incomingPacketFilename;
    ssize_t readlen;             // amount of data read from socket
    int packetType;
    // unordered_set<string> processedFiles;
    
    // listen forever
    while (1) {
        // validate size of received packet
        readlen = sock -> read(incomingPacket, MAX_PACKET_LEN);
        if (readlen == 0) {
            c150debug->printf(C150APPLICATION,"Read zero length packet, trying again");
            continue;
        }

        // validate filename: not here!
        // if not "" and not currFilename and not in proccessedFiles
        // -> file later in the timeline (impossible) -> abort?
        // reset currFilename to "" after proccessing file?

        packetType = getPacketType(incomingPacket);
        switch (packetType) {
            case DATA_PACKET_TYPE:
                printf("Data packet received.\n");
                // readDataPacket(incomingPacket);
                // put the following log inside
                // *GRADING << "File: " << filename << " starting to receive file" << endl;
                break;

            case CS_REQUEST_PACKET_TYPE:
                printf("Checksum request packet received.\n");
                
                // TODO: can we have duplicates logs?
                *GRADING << "File: " << currFilename << " received, beginning end-to-end check" << endl;

                sendChecksumPacket(sock, (char *) currFilename.c_str(), dirName, filenastiness);

                printf("Sent checksum packet for file %s, retry %d\n", currFilename.c_str(), 0);
                break;

            case CS_COMPARISON_PACKET_TYPE: 
                printf("Checksum comparison packet received.\n");

                // perform necessary filename checks

                renameOrRemove(currFilename, dirName, filenastiness, incomingPacket);

                sendFinishPacket(sock, (char *) currFilename.c_str());
                break;

            default:
                fprintf(stderr, "Invalid packet type %d received.\n", packetType);
                break;
        }
    }
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
                        int filenastiness) {
    assert(sock != NULL);
    assert(filename != NULL);
    
    printf("In sendChecksumPacket with filename %s, directory name %s and file nastiness %d\n", filename, dirName.c_str(), filenastiness);

    char outgoingResponsePacket[MAX_PACKET_LEN];
    ChecksumResponsePacket responsePacket;
    unsigned char checksum[HASH_CODE_LENGTH];

    // set filename
    memcpy(responsePacket.filename, filename, strlen(filename) + 1);
    cout << "strcmp " << strcmp(filename, responsePacket.filename) << endl;
    cout << responsePacket.packetType << " " << responsePacket.filename << endl;


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
    memcpy(responsePacket.checksum, checksum, HASH_CODE_LENGTH);

    printf("Printing calculated checksum: ");
    for (int i = 0; i < 20; i++)
    {
        printf ("%02x", (unsigned int) responsePacket.checksum[i]);
    }
    printf ("\n");

    // TODO: remove
    assert(memcmp(responsePacket.checksum, checksum, HASH_CODE_LENGTH) == 0);

    memcpy(outgoingResponsePacket, &responsePacket, sizeof(responsePacket));
    sock->write(outgoingResponsePacket, MAX_PACKET_LEN);
}


void renameOrRemove(string filename, string dirName, int filenastiness, 
                    char incomingPacket[]) {
    cout << "In rename or remove\n";
    
    NASTYFILE file(filenastiness);
    ChecksumComparisonPacket *comparisonPacket = reinterpret_cast<ChecksumComparisonPacket *>(incomingPacket);
    string tempFilename = filename;
    tempFilename += "-TMP";
    string tempFullPath = dirName + '/' + tempFilename;

    // TODO: check if the filename matches with current file
    if (strcmp((char *) filename.c_str(), comparisonPacket->filename) != 0){
        cout << memcmp((char *) filename.c_str(), comparisonPacket->filename, FILENAME_LEN);
        fprintf(stderr,"Filename inconsistent when comparing hash. Expected file %s but received file %s\n", filename.c_str(), comparisonPacket->filename);
        return; // ??
    }

    // check if file transfer was successful
    if (comparisonPacket->comparisonResult) {  // rename
        *GRADING << "File: " << filename << " end-to-end check succeeded" << endl;
    
        cout << "Renaming" << endl;
        string fullPath = dirName + '/' + filename;

        // check if rename success
        // retry if failed?
        if (rename(tempFullPath.c_str(), fullPath.c_str()) != 0) {
            fprintf(stderr, "Error renaming file %s to %s\n", tempFullPath.c_str(), fullPath.c_str());
        } else {
            cout << "File renamed successfully" << endl;
        }
    } else {      
        *GRADING << "File: " << filename << " end-to-end check failed" << endl;                    // remove
        cout << "Removing" << endl;
        if (remove(tempFullPath.c_str()) != 0) {
            fprintf(stderr, "Unable to remove the file\n");

        } else {
            printf("File removed successfully\n");
        }
    }
}


void sendFinishPacket(C150DgmSocket *sock, string filename) {
    assert(sock != NULL);

    char outgoingFinishPacket[MAX_PACKET_LEN];
    FinishPacket finishPacket;

    memcpy(finishPacket.filename, filename.c_str(), strlen(filename.c_str()) + 1);
    cout << "strcmp " << strcmp(filename.c_str(), finishPacket.filename) << endl;
    cout << finishPacket.packetType << " " << finishPacket.filename << endl;
    memcpy(outgoingFinishPacket, &finishPacket, sizeof(finishPacket));

    // write
    cout << "write len " <<  outgoingFinishPacket << endl;
    sock -> write(outgoingFinishPacket, MAX_PACKET_LEN);
    printf("Sent finish packet for file %s\n", filename.c_str());
}
