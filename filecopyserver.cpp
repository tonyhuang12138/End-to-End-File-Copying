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
// #include <string.h>

using namespace std;          // for C++ std library
using namespace C150NETWORK;  // for all the comp150 utilities 

# define MAX_MSG_LEN 512

const int networknastinessArg = 1;        // networknastiness is 1st arg
const int filenastinessArg = 2;           // filenastiness is 2nd arg
const int targetdirArg = 3;               // targetdir is 3rd arg

// my function pointer

  
int main(int argc, char *argv[])  {
    //
    //  DO THIS FIRST OR YOUR ASSIGNMENT WON'T BE GRADED!
    //
    GRADEME(argc, argv);

    //
    // Variable declarations
    //

    // create file pointer with given nastiness
    int networknastiness = atoi(argv[networknastinessArg]); 
    int filenastiness = atoi(argv[filenastinessArg]);

    // create file stream
    NASTYFILE output(filenastiness);

    // create socket
    C150DgmSocket *sock = new C150NastyDgmSocket(networknastiness);

    ssize_t readlen;             // amount of data read from socket
    char incomingMessage[512];   // received message data


    // validate input format
    if (argc != 4) {
        fprintf(stderr,"Correct syntax is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(1);
    }


    try {
        // open target dir
        DIR *SRC = opendir(argv[targetdirArg]);
        if (SRC == NULL) {

            fprintf(stderr,"Error opening source directory %s\n", argv[targetdirArg]);     
            exit(8);

        } else {

            // recursively process request send to server
            // TODO: don't think this is while 1; need to investigate
            while (1) {
                //
                // Read a packet
                // -1 in size below is to leave room for null
                //
                readlen = sock -> read(incomingMessage, sizeof(incomingMessage)-1);

                if (readlen == 0) {
                    c150debug->printf(C150APPLICATION,"Read zero length message, trying again");
                    continue;
                }

                printf("Received incoming packet %s\n", incomingMessage);

                // TODO: reverse stringfy the incoming message to be a struct
                Packet curr;
                (void) curr;

                // ask struct name
                int packetType = 0;         // TODO: extract packet type from struct
                switch (packetType) {
                    case 0:                 // BeginTransmissionPacket            
                        break;

                    case 2:                 // ChecksumComparisonPacket   
                        break;
                        
                    case 3:                 // DataPacket   
                        break;
                }
            }

            // end session
            closedir(SRC);
        }
    }

    catch (C150NetworkException& e) {
        // Write to debug log
        c150debug->printf(C150ALWAYSLOG,"Caught C150NetworkException: %s\n",
                          e.formattedExplanation().c_str());
        // In case we're logging to a file, write to the console too
        cerr << argv[0] << ": caught C150NetworkException: " << e.formattedExplanation() << endl;
    }
    
    return 0;
}