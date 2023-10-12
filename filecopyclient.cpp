// 
//            filecopyclient.cpp
//
//     Author: Tony Huang and Bill Liu


//    COMMAND LINE
//
//          fileclient <server> <networknastiness> <filenastiness> <srcdir>
//
//     The client should loop through all the filenames in the source directory 
//     named on its command line. For each one, it should use its end-to-end 
//     protocol to tell the server that a check is necessary.
// 
//     The client should then confirm to the server that it knows about the 
//     success (or failure), at which point the server should indicate in its 
//     output the name of the file, and whether there was success or failure. 
//     Be sure to write your log GRADELOG entries as specified in What to put 
//     in the grading logs. (This is the point where, in later versions, the 
//     server will either rename or delete the file).
//
//     References:
//     - https://stackoverflow.com/questions/306533/
//       how-do-i-get-a-list-of-files-in-a-directory-in-c
//     - https://stackoverflow.com/questions/4553012/
//       checking-if-a-file-is-a-directory-or-just-a-file
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
#include <assert.h>
// #include <string.h>

using namespace std;          // for C++ std library
using namespace C150NETWORK;  // for all the comp150 utilities 

# define MAX_RETRIES 5

bool isDirectory(char *dirname);
int getPacketType(char incomingPacket[]);
void copyFile(C150DgmSocket *sock, string filename, string dirName,  
              int filenastiness);
void sendDataPacket(C150DgmSocket *sock, char filename[], 
                    char outgoingDataPacket[], int outgoingDataPacketSize);
void receiveChecksumPacket(C150DgmSocket *sock, string filename, 
                           string dirName, int filenastiness,
                           char outgoingDataPacket[]);
void sendConfirmation(C150DgmSocket *sock, string filename, string dirName,  
                        int filenastiness, char incomingChecksumPacket[], 
                        int incomingChecksumPacketSize);
void sendConfirmationPacket(C150DgmSocket *sock, char filename[],
                            string dirName, int filenastiness, 
                            bool comparisonResult,
                            char outgoingConfirmationPacket[], 
                            int outgoingConfirmationPacketSize);
bool compareHash(string filename, string dirName, 
                 int filenastiness, char incomingChecksumPacket[]);

const int serverArg = 1;                  // server name is 1st arg
const int networknastinessArg = 2;        // networknastiness is 2nd arg
const int filenastinessArg = 3;           // filenastiness is 3rd arg
const int srcdirArg = 4;                  // srcdir is 4th arg  

  
int main(int argc, char *argv[]) {
    // validate input format
    if (argc != 5) {
        fprintf(stderr,"Correct syntax is: %s <server> <networknastiness> <filenastiness> <srcdir>\n", argv[0]);
        exit(1);
    }

    //
    //  DO THIS FIRST OR YOUR ASSIGNMENT WON'T BE GRADED!
    //
    GRADEME(argc, argv);

    //
    //  Set up debug message logging
    //
    // setUpDebugLogging("filecopyclientdebug.txt",argc, argv);
    
    int networknastiness = atoi(argv[networknastinessArg]); 
    int filenastiness = atoi(argv[filenastinessArg]);

    // Create the socket
    // TODO: maybe setup debugging log?
    C150DgmSocket *sock = new C150NastyDgmSocket(networknastiness);
    // Tell the DGMSocket which server to talk to
    sock -> setServerName(argv[serverArg]);  
    sock -> turnOnTimeouts(3000);

    try {
        // check if target dir exists
        DIR *SRC = opendir(argv[srcdirArg]);
        if (SRC == NULL) {
            fprintf(stderr,"Error opening source directory %s\n", argv[srcdirArg]);     
            exit(8);
        }

        // loop through all filenames in src dir
        while (dirent *f = readdir(SRC)) {
            char path[500];

            sprintf(path, "%s/%s", argv[srcdirArg], f->d_name);

            // skip all subdirectories
            if (!f->d_name || isDirectory(path)) {
                continue; 
            }

            // can there be null characters in a filename?
            // printf("%ld\n", strlen(f->d_name));
            // string filename;
            // strcpy(filename, f->d_name);
            // printf("%s\n", filename.c_str());


            // the final submission should include a while loop to send all packets of a file. here we are simulating data transmission init and complete in one dummy data packet.

            // timeout logic
            copyFile(sock, f->d_name, argv[srcdirArg], filenastiness);
        }

        closedir(SRC);
    }
    //
    //  Handle networking errors -- for now, just print message and give up!
    //
    catch (C150NetworkException& e) {
        // Write to debug log
        c150debug->printf(C150ALWAYSLOG,"Caught C150NetworkException: %s\n",
                        e.formattedExplanation().c_str());
        // In case we're logging to a file, write to the console too
        cerr << argv[0] << ": caught C150NetworkException: " << e.formattedExplanation()\
                        << endl;
    }
    

    return 0;
}


// ------------------------------------------------------
//
//                   isDirectory
//
//  Check if the supplied file name is a directory
//  
//  See references up top
//     
// ------------------------------------------------------

bool isDirectory(char *dirname) {
    struct stat statbuf;

    if (stat(dirname, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
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
//                   copyFile
//
//  Given a filename, copy it to server
//     
// ------------------------------------------------------
void copyFile(C150DgmSocket *sock, string filename, string dirName,  
              int filenastiness) {
    char outgoingDataPacket[DATA_PACKET_LEN]; // TODO: null terminate this?

    // Start sending
    *GRADING << "File: " << filename << ", beginning transmission, attempt <" << 1 << ">" << endl;

    sendDataPacket(sock, (char *) filename.c_str(), outgoingDataPacket, DATA_PACKET_LEN);
    printf("Sent data packet for file %s, retry %d\n", filename.c_str(), 0);

    *GRADING << "File: " << filename << " transmission complete, waiting for end-to-end check, attempt " << 1 << endl;
        
    receiveChecksumPacket(sock, filename, dirName, filenastiness, 
                          outgoingDataPacket);
}


// ------------------------------------------------------
//
//                   sendDataPacket
//
//  Given a filename, send the parts of the data as a packet 
//  to the server and write to outgoingDataPacket
//     
// ------------------------------------------------------
void sendDataPacket(C150DgmSocket *sock, char filename[], 
                    char outgoingDataPacket[], int outgoingDataPacketSize) {
    // maybe remove it in final submission?
    assert(sock != NULL);
    assert(filename != NULL);

    DataPacket dataPacket;

    // TODO: hardcoding only for the end to end submission, change later
    dataPacket.numTotalPackets = 1;
    dataPacket.packetNumber = 1;

    memcpy(dataPacket.filename, filename, strlen(filename) + 1);
    cout << "strcmp " << strcmp(filename, dataPacket.filename) << endl;
    cout << dataPacket.packetType << " " << dataPacket.filename << endl;
    memcpy(outgoingDataPacket, &dataPacket, sizeof(dataPacket));

    // write
    cout << "write len " <<  outgoingDataPacketSize << endl;
    sock -> write(outgoingDataPacket, outgoingDataPacketSize);
}


// the later version should resend cycle/file instead of packet. here the packet symbols the entire file
// ------------------------------------------------------
//
//                   receiveChecksumPacket
//
//  Expect a checksum packet from server. If received, 
//  compute a checksum for the file and compares it with
//  that of the server packet and sends the comparison
//  result to server.
//     
// ------------------------------------------------------
void receiveChecksumPacket(C150DgmSocket *sock, string filename, 
                           string dirName, int filenastiness,
                           char outgoingDataPacket[]) {
    assert(sock != NULL);
    //
    // Variable declarations
    //
    ssize_t readlen;              // amount of data read from socket
    char incomingChecksumPacket[CHECKSUM_PACKET_LEN];
    int retry_i = 0;
    bool timeoutStatus;

    printf("Receiving checksum packet for file %s\n", filename.c_str());
    assert(CHECKSUM_PACKET_LEN == sizeof(incomingChecksumPacket));
    readlen = sock -> read(incomingChecksumPacket, CHECKSUM_PACKET_LEN);
    timeoutStatus = sock -> timedout();

    cout << "Timeout status is: " << timeoutStatus << endl;

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
        sock -> write(outgoingDataPacket, DATA_PACKET_LEN);
        printf("Sent data packet for file %s, retry %d\n", filename.c_str(), retry_i);

        // Read the response from the server
        // c150debug->printf(C150APPLICATION,"%s: Returned from write, doing read()", argv[0]);
        readlen = sock -> read(incomingChecksumPacket, CHECKSUM_PACKET_LEN);
        timeoutStatus = sock -> timedout();
    }

    // throw exception if all retries exceeded
    if (retry_i == MAX_RETRIES) {
        throw C150NetworkException("Timed out after 5 retries.");
    }

    // write flush logic


    sendConfirmation(sock, filename, dirName, filenastiness, incomingChecksumPacket, CHECKSUM_PACKET_LEN);
}

void sendConfirmation(C150DgmSocket *sock, string filename, string dirName,  
                        int filenastiness, char incomingChecksumPacket[], 
                        int incomingChecksumPacketSize) {
    assert(sock != NULL);
    
    int packetType;
    char outgoingConfirmationPacket[CONFIRMATION_PACKET_LEN];
    bool comparisonResult;

    // validate packet type
    packetType = getPacketType(incomingChecksumPacket);
    if (packetType != CHECKSUM_PACKET_TYPE) { 
        fprintf(stderr,"Should be receiving checksum confirmation packets but packet of packetType %d received.\n", packetType);
        return; // retry ???
    }
    
    comparisonResult = compareHash(filename, dirName, filenastiness, incomingChecksumPacket);
    sendConfirmationPacket(sock, (char *) filename.c_str(), dirName, filenastiness, comparisonResult, outgoingConfirmationPacket, CONFIRMATION_PACKET_LEN);
    printf("Sent confirmation packet for file %s, retry %d\n", filename.c_str(), 0);

    // receiveFinishPacket(sock, filename, outgoingConfirmationPacket);
}


bool compareHash(string filename, string dirName, 
                 int filenastiness, char incomingChecksumPacket[]) {
    bool comparisonResult;
    
    unsigned char localChecksum[HASH_CODE_LENGTH];
    ChecksumPacket *checksumPacket = reinterpret_cast<ChecksumPacket *>(incomingChecksumPacket);

    // TODO: check if the filename matches with current file
    if (memcmp((char *) filename.c_str(), checksumPacket->filename, FILENAME_LEN) != 0){
        return 2; // ??
    }
        
    // compute local checksum
    // TODO: sample over several times
    sha1(filename, dirName, filenastiness,  localChecksum);

    // compare checksums
    if (memcmp(localChecksum, checksumPacket->checksum, HASH_CODE_LENGTH) == 0) {
        comparisonResult = true;
        *GRADING << "File: " << filename << " end-to-end check succeeded, attempt " << 1 << endl;
    } else {
        comparisonResult = false;
        *GRADING << "File: " << filename << " end-to-end check failed, attempt " << 1 << endl;
    }

    return comparisonResult;
}


void sendConfirmationPacket(C150DgmSocket *sock, char filename[],
                            string dirName, int filenastiness, 
                            bool comparisonResult,
                            char outgoingConfirmationPacket[], 
                            int outgoingConfirmationPacketSize) {
    assert(sock != NULL);
    assert(filename != NULL);

    // send comparison result
    // wait for server response
    ConfirmationPacket confirmationPacket;

    memcpy(confirmationPacket.filename, filename, strlen(filename) + 1);
    cout << "strcmp " << strcmp(filename, confirmationPacket.filename) << endl;
    cout << confirmationPacket.packetType << " " << confirmationPacket.filename << endl;
    memcpy(outgoingConfirmationPacket, &confirmationPacket, sizeof(confirmationPacket));

    // write
    cout << "write len " <<  outgoingConfirmationPacket << endl;
    sock -> write(outgoingConfirmationPacket, outgoingConfirmationPacketSize);
}


