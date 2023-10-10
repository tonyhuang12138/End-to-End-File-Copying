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
void copyFile(C150DgmSocket **sock, char filename[]);
void sendDataPacket(C150DgmSocket **sock, char filename[], 
                      char outgoingPacket[MAX_PKT_LEN]);

const int serverArg = 1;                  // server name is 1st arg
const int networknastinessArg = 2;        // networknastiness is 2nd arg
const int filenastinessArg = 3;           // filenastiness is 3rd arg
const int srcdirArg = 4;                  // srcdir is 4th arg  

  
int main(int argc, char *argv[]) {
    //
    //  DO THIS FIRST OR YOUR ASSIGNMENT WON'T BE GRADED!
    //
    GRADEME(argc, argv);
    
    // create file pointer with given nastiness
    int networknastiness = atoi(argv[networknastinessArg]); 
    int filenastiness = atoi(argv[filenastinessArg]);
    
    NASTYFILE inputFile(filenastiness);

    // Create the socket
    // TODO: maybe setup debugging log?
    C150DgmSocket *sock = new C150NastyDgmSocket(networknastiness);
    // Tell the DGMSocket which server to talk to
    sock -> setServerName(argv[serverArg]);  
    sock -> turnOnTimeouts(3000);


    // validate input format
    if (argc != 5) {
        fprintf(stderr,"Correct syntax is: %s <server> <networknastiness> <filenastiness> <srcdir>\n", argv[0]);
        exit(1);
    }


    try {
        // loop through all filenames in src dir
        DIR *SRC = opendir(argv[srcdirArg]);
        if (SRC == NULL) {
            fprintf(stderr,"Error opening source directory %s\n", argv[srcdirArg]);     
            exit(8);
        }

        while (dirent *f = readdir(SRC)) {
            char path[500];

            sprintf(path, "%s/%s", argv[srcdirArg], f->d_name);

            // skip all subdirectories
            if (!f->d_name || isDirectory(path)) {
                continue; 
            }

            // the final submission should include a while loop to send all packets of a file. here we are simulating data transmission init and complete in one dummy data packet.

            // timeout logic
            copyFile(&sock, f->d_name);
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
//                   copyFile
//
//  Given a filename, copy it to server
//     
// ------------------------------------------------------
void copyFile(C150DgmSocket **sock, char filename[]) {
    //
    // Variable declarations
    //
    ssize_t readlen;              // amount of data read from socket
    char incomingPacket[MAX_PKT_LEN];
    char outgoingPacket[MAX_PKT_LEN]; // TODO: null terminate this?
    int retry_i = 0;
    bool timeoutStatus;

    // Start sending
    *GRADING << "File: " << filename << ", beginning transmission, attempt <" << 1 << ">" << endl;

    sendDataPacket(sock, filename, outgoingPacket);

    // Read the response from the server
    // c150debug->printf(C150APPLICATION,"%s: Returned from write, doing read()", argv[0]);
    readlen = (*sock) -> read(incomingPacket, sizeof(incomingPacket));
    timeoutStatus = (*sock) -> timedout();

    // keep resending message up to MAX_RETRIES times when read timedout
    retry_i = 0;
    while (retry_i < MAX_RETRIES && timeoutStatus == true) {
        // Send the message to the server
        // c150debug->printf(C150APPLICATION,"%s: Writing message: \"%s\"",
        //                 argv[0], outgoingMsg);
        (*sock) -> write(outgoingPacket, sizeof(outgoingPacket));

        // Read the response from the server
        // c150debug->printf(C150APPLICATION,"%s: Returned from write, doing read()", argv[0]);
        readlen = (*sock) -> read(incomingPacket, 
                                sizeof(incomingPacket));
        timeoutStatus = (*sock) -> timedout();

        retry_i++;
    }

    // throw exception if all retries exceeded
    if (retry_i == MAX_RETRIES) {
        throw C150NetworkException("Timed out after 5 retries.");
    }

    // keep reading until timeout if read is successful
    while (timeoutStatus != true) {

        // Read the response from the server
        // c150debug->printf(C150APPLICATION,"%s: more reads - Returned from write, doing read()", argv[0]);
        readlen = (*sock) -> read(incomingPacket, sizeof(incomingPacket));
        timeoutStatus = (*sock) -> timedout();
    }

    timeoutStatus = false;
    (void) readlen;
    
    *GRADING << "File: " << filename << " transmission complete, waiting for end-to-end check, attempt " << 1 << endl;
}


// ------------------------------------------------------
//
//                   sendDataPacket
//
//  Given a filename, send the parts of the data as a packet 
//  to the server and write to outgoingPacket
//     
// ------------------------------------------------------
void sendDataPacket(C150DgmSocket **sock, char filename[], 
                      char outgoingPacket2[MAX_PKT_LEN]) {
    // maybe remove it in final submission?
    assert(sock != NULL && *sock != NULL);
    assert(filename != NULL);

    // TODO: is memcpy dangerous? +1 strlen?
    char outgoingPacket[MAX_PKT_LEN];
    DataPacket dataPacket;

    // TODO: hardcoding only for the end to end submission, change later
    dataPacket.numTotalPackets = 1;
    dataPacket.packetNumber = 1;

    cout << strlen(filename) + 1 << endl;
    memcpy(dataPacket.filename, filename, strlen(filename) + 1);
    cout << "strcmp " << strcmp(filename, dataPacket.filename) << endl;
    cout << dataPacket.packetType << " " << dataPacket.filename << endl;
    // TODO: do we still need to consider the +1 if char arr is in struct?
    memcpy(outgoingPacket, &dataPacket, sizeof(dataPacket));

    // TODO: testing; change name!
    memcpy(outgoingPacket2, outgoingPacket, sizeof(outgoingPacket));


    // write
    cout << "write len " <<  sizeof(outgoingPacket) << endl;
    (*sock) -> write(outgoingPacket, sizeof(outgoingPacket));
}
