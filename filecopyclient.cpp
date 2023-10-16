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
//     - https://stackoverflow.com/questions/2745074/
//       fast-ceiling-of-an-integer-division-in-c-c
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
#include <vector>
#include "sha1.h"
#include "nastyfileio.h"
// #include <string.h>

using namespace std;          // for C++ std library
using namespace C150NETWORK;  // for all the comp150 utilities 

#define MAX_RETRIES 20
#define MAX_CHUNK_RETRIES 20
#define MAX_FLUSH_RETRIES 15

// file copy functions
void copyFile(C150DgmSocket *sock, string filename, string dirName,  
              int filenastiness);
size_t getTotalChunks(string filename, string dirName);
void freeChunk(vector<char *> chunk);
char *generateDataPacket(string filename, string dirName, int filenastiness
                         size_t currChunkNum, int currPacketNum);


// end to end functions
void sendChecksumRequest(C150DgmSocket *sock, char filename[], 
                         char incomingResponsePacket[]);
void sendChecksumConfirmation(C150DgmSocket *sock, char filename[], 
                              string dirName, int filenastiness, 
                              char incomingResponsePacket[]);
bool compareHash(char filename[], string dirName, 
                 int filenastiness, char incomingResponsePacket[]);
void sendAndRetry(C150DgmSocket *sock, char filename[], char outgoingPacket[], 
                  char incomingPacket[], int outgoingPacketType, 
                  int incomingPacketType);
void validatePacket(char incomingPacket[], int incomingPacketType, 
                    char filename[], int readlen, bool *timeout, 
                    int *numRetried, int *numFlushed, bool retryFlag);


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
    
    int networknastiness = atoi(argv[networknastinessArg]); 
    int filenastiness = atoi(argv[filenastinessArg]);

    // create the socket and tell the DGMSocket which server to talk to
    C150DgmSocket *sock = new C150NastyDgmSocket(networknastiness);
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
            
            copyFile(sock, f->d_name, argv[srcdirArg], filenastiness);

            // end to end
            // char incomingResponsePacket[MAX_PACKET_LEN];
            // sendChecksumRequest(sock, f->d_name, incomingResponsePacket);
            // sendChecksumConfirmation(sock, f->d_name, argv[srcdirArg], filenastiness, incomingResponsePacket);
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


// void sendChunk() {
//     // load chunk with packets
// }

// ------------------------------------------------------
//
//                   copyFile
//
//  Given a filename, copy it to server
//     
// ------------------------------------------------------
void copyFile(C150DgmSocket *sock, string filename, string dirName,  
              int filenastiness) {
    // Start sending
    *GRADING << "File: " << filename << ", beginning transmission, attempt <" << 1 << ">" << endl;

    char incomingCheckPacket[MAX_PACKET_LEN];
    size_t numTotalChunks;
    size_t currChunkNum = 0;
    int numRetried = 0;

    // calculate number of chunks in file
    numTotalChunks = getTotalChunks(filename, dirName);
    (void) numTotalChunks;
    // keep sending while there are more chunks left
    // while (currChunkNum < numTotalChunks) {
    //     chunk = getChunk(filename, dirName, currChunkNum);
    //     sendAndRetry(sock, filename, chunk, incomingCheckPacket, DATA_PACKET_TYPE, CHUNK_CHECK_PACKET_TYPE, true);
    //     readChunkCheck(incomingCheckPacket);

    //     // keep sending chunks until full chunk is successfully delivered
    //     while (sizeof(failedPackets) != 0 && numRetried < MAX_CHUNK_RETRIES) {
    //         readChunkCheck(incomingCheckPacket);
    //         numRetried++;
    //     }

    //     currChunkNum++;
    // }

    printf("%s copied to server.\n", filename.c_str());
}


// ------------------------------------------------------
//
//                   getTotalChunks
//
//  Given a filename and a directory name, find how many 
//  chunks of 8 are in it
//     
// ------------------------------------------------------
size_t getTotalChunks(string filename, string dirName) {
    size_t fileSize = getFileSize(filename, dirName);
    // see references up top
    size_t numTotalChunks = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE; 

    printf("%s: total size is %ld and number of chunks is %ld\n", filename.c_str(), fileSize, numTotalChunks);

    return numTotalChunks;
}


vector<char *> getChunk(string filename, string dirName, size_t currChunkNum) {
    vector<string> chunk;

    // load up chunk with data packets
    for (int i = 0; i < CHUNK_SIZE; i++) {
        char *dataPacket = generateDataPacket(filename, dirName, currChunkNum, i);
        chunk.push_back(dataPacket);
    }

    return chunk;
}

void freeChunk(vector<char *> chunk) {
    for (char *dataPacket : chunk) {
        free(dataPacket);
    }
}


char *generateDataPacket(string filename, string dirName, int filenastiness
                         size_t currChunkNum, int currPacketNum) {
    char dataPacket[MAX_PACKET_LEN];
    string sourceName = makeFileName(dirName, fileName);
    size_t fileSize = getFileSize(filename, dirName);
    void *fopenretval;
    size_t offset;
    unsigned char *buffer = (unsigned char *) malloc(DATA_LEN);
    NASTYFILE inputFile(nastiness);
    DataPacket dataPacket;

    // open file
    offset = (currChunkNum * CHUNK_SIZE + currPacketNum) * DATA_LEN
    inputFile.fseek(offset, SEEK_SET);

    fopenretval = inputFile.fopen(sourceName.c_str(), "rb");
    if (fopenretval == NULL) {
      cerr << "Error opening input file " << sourceName << 
        " errno=" << strerror(errno) << endl;
      exit(12);
    }


    // read DATA_LEN bytes of data
    readAmount = (offset + DATA_LEN < fileSize) ? DATA_LEN : fileSize - offset;
    len = inputFile.fread(buffer, 1, readAmount);
    if (len != readAmount) {
      cerr << "Error reading file " << sourceName << 
        "  errno=" << strerror(errno) << endl;
      exit(16);
    }

    if (inputFile.fclose() != 0 ) {
      cerr << "Error closing input file " << sourceName << 
        " errno=" << strerror(errno) << endl;
      exit(16);
    }


    // compare with file size
    // TODO: compare cache with disk?

    return dataPacket;
}



/* --------------------------END TO END FUNCTIONS--------------------------- */

// --------------------------------------------------------------------------
//
//                           sendChecksumRequest
//
//  
//     
// --------------------------------------------------------------------------
void sendChecksumRequest(C150DgmSocket *sock, char filename[], 
                         char incomingResponsePacket[]) {
    assert(sock != NULL);
    assert(filename != NULL);
    
    char outgoingRequestPacket[MAX_PACKET_LEN];
    ChecksumRequestPacket requestPacket;

    // generating request packet
    memcpy(requestPacket.filename, filename, strlen(filename) + 1);
    memcpy(outgoingRequestPacket, &requestPacket, sizeof(requestPacket));

    printf("Checksum request packet for %s generated\n", requestPacket.filename);

    char packagedFilename[FILENAME_MAX];
    getFilename(outgoingRequestPacket, packagedFilename);

    printf("Double checking before send: filename in packet is %s\n", packagedFilename);

    *GRADING << "File: " << filename << " transmission complete, waiting for end-to-end check, attempt " << 1 << endl;

    sendAndRetry(sock, filename, outgoingRequestPacket, incomingResponsePacket, CS_REQUEST_PACKET_TYPE, CS_RESPONSE_PACKET_TYPE);
}


void sendChecksumConfirmation(C150DgmSocket *sock, char filename[], 
                              string dirName, int filenastiness, 
                              char incomingResponsePacket[]) {
    assert(sock != NULL);
    assert(filename != NULL);
    
    char outgoingComparisonPacket[MAX_PACKET_LEN];
    char incomingFinishPacket[MAX_PACKET_LEN];
    ChecksumComparisonPacket comparisonPacket;

    comparisonPacket.comparisonResult = compareHash(filename, dirName, filenastiness, incomingResponsePacket);

    memcpy(comparisonPacket.filename, filename, strlen(filename) + 1);
    cout << "strcmp " << strcmp(filename, comparisonPacket.filename) << endl;
    cout << comparisonPacket.packetType << " " << comparisonPacket.filename << endl;
    memcpy(outgoingComparisonPacket, &comparisonPacket, sizeof(comparisonPacket));

    sendAndRetry(sock, filename, outgoingComparisonPacket, incomingFinishPacket, CS_COMPARISON_PACKET_TYPE, FINISH_PACKET_TYPE);
}


// --------------------------------------------------------------------------
//
//                                compareHash
//
//  Given an incoming checksum response packet, compare its value with locally
//  computed checksum
//     
// --------------------------------------------------------------------------
bool compareHash(char filename[], string dirName, 
                 int filenastiness, char incomingResponsePacket[]) {
    cout << "In compare hash\n";

    // TODO: remove
    int receivedPacketType = getPacketType(incomingResponsePacket);
    if (receivedPacketType != CS_RESPONSE_PACKET_TYPE) { 
        fprintf(stderr,"Should be receiving checksum response packet but packet of packetType %s received.\n", packetTypeStringMatch(receivedPacketType).c_str());
    }
    
    unsigned char localChecksum[HASH_CODE_LENGTH];
    ChecksumResponsePacket *responsePacket = reinterpret_cast<ChecksumResponsePacket *>(incomingResponsePacket);

    // TODO: check if the filename matches with current file
    if (strcmp(filename, responsePacket->filename) != 0){
        cout << memcmp(filename, responsePacket->filename, FILENAME_LEN);
        fprintf(stderr,"Filename inconsistent when comparing hash. Expected file %s but received file %s\n", filename, responsePacket->filename);
        return 2; // ??
    }
        
    // compute local checksum
    // TODO: sample over several times
    sha1(filename, dirName, filenastiness,  localChecksum);

    // compare checksums
    cout << "Comparing hash\n";
    if (memcmp(localChecksum, responsePacket->checksum, HASH_CODE_LENGTH) == 0) {
        printf("End to end succeeded for file %s\n", filename);
        *GRADING << "File: " << filename << " end-to-end check succeeded, attempt " << 1 << endl;
        return true;
    } else {
        printf("End to end failed for file %s\n", filename);
        *GRADING << "File: " << filename << " end-to-end check failed, attempt " << 1 << endl;
        return false;
    }
}

/* ----------------------------UTILITY FUNCTIONS---------------------------- */

// --------------------------------------------------------------------------
//
//                           sendAndRetry
//
//  Given a packet, send and expect a response. If response is not received,
//  retry up to five times before aborting on network failure.
//     
// --------------------------------------------------------------------------
void sendAndRetry(C150DgmSocket *sock, char filename[], char outgoingPacket[], 
                  char incomingPacket[], int outgoingPacketType, 
                  int incomingPacketType) {
    assert(sock != NULL);
    assert(filename != NULL);

    string outgoingType = packetTypeStringMatch(outgoingPacketType);
    string expectedIncomingType = packetTypeStringMatch(incomingPacketType);
    char receivedFilename[FILENAME_LEN];
    ssize_t readlen;              // amount of data read from socket
    int numRetried = 0;
    int numFlushed = 0;
    bool timeout;
    
    // send packet
    sock -> write(outgoingPacket, MAX_PACKET_LEN);
    printf("Sent %s packet for file %s\n", outgoingType.c_str(), filename);

    // receive packet
    printf("Receiving %s packet for file %s\n", expectedIncomingType.c_str(), filename);
    // assert(CHECKSUM_PACKET_LEN == sizeof(incomingResponsePacket));
    readlen = sock -> read(incomingPacket, MAX_PACKET_LEN);
    timeout = sock -> timedout();

    // retry if validation failed
    if (!timeout) validatePacket(incomingPacket, incomingPacketType, filename, readlen, &timeout, &numRetried, &numFlushed, false);
    
    // keep resending message up to MAX_RETRIES times when read timedout
    while (timeout == true && numRetried < MAX_RETRIES && numFlushed < MAX_FLUSH_RETRIES) {
        numRetried++;

        // send again
        sock -> write(outgoingPacket, MAX_PACKET_LEN);
        printf("Sent %s packet for file %s, retry %d\n", outgoingType.c_str(), filename, numRetried);

        // read again
        readlen = sock -> read(incomingPacket, MAX_PACKET_LEN);
        timeout = sock -> timedout();

        // retry if validation failed
        if (!timeout) validatePacket(incomingPacket, incomingPacketType, filename, readlen, &timeout, &numRetried, &numFlushed, true);
    }

    // throw exception if all retries exceeded
    if (numRetried == MAX_RETRIES) throw C150NetworkException("Timed out after 5 retries.");
}

// --------------------------------------------------------------------------
//
//                           validatePacket
//
//  Given an incoming packet, validate that it is not empty, is of the correct
//  file and has the correct packet type
//     
// --------------------------------------------------------------------------
void validatePacket(char incomingPacket[], int incomingPacketType, 
                    char filename[], int readlen, bool *timeout, 
                    int *numRetried, int *numFlushed, bool retryFlag) {
    char receivedFilename[FILENAME_LEN];
    int receivedPacketType;   
    string expectedIncomingType = packetTypeStringMatch(incomingPacketType);

    // validate size of received packet
    if (readlen == 0) {
        fprintf(stderr, "Read zero length message, trying again");
        *timeout = true;
    }

    // validate filename
    // TODO: abort when filename unseen?
    getFilename(incomingPacket, receivedFilename);
    if (strcmp(receivedFilename, filename) != 0) { 
        fprintf(stderr,"Should be receiving packet for file %s but received packets for file %s instead.\n", filename, receivedFilename);
        *timeout = true;
        if (retryFlag) {
            *numRetried -= 1;
            *numFlushed += 1;
        }
    }

    // validate packet type
    receivedPacketType = getPacketType(incomingPacket);
    if (receivedPacketType != incomingPacketType) { 
        fprintf(stderr,"Should be receiving %s packet but packet of packetType %s received.\n", expectedIncomingType.c_str(), packetTypeStringMatch(receivedPacketType).c_str());
        *timeout = true;
        if (retryFlag) {
            *numRetried -= 1;
            *numFlushed += 1;
        }
    }
}

