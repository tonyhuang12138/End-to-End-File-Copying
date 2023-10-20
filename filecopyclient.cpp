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
//     - https://stackoverflow.com/questions/9370945/
//       finding-the-max-value-in-a-map
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
#include "sha1.h"
#include "nastyfileio.h"
#include <string.h>
#include <vector>
#include <unordered_map>


using namespace std;          // for C++ std library
using namespace C150NETWORK;  // for all the comp150 utilities 

#define MAX_FILE_RETRIES 5
#define MAX_PKT_RETRIES 20
#define MAX_FLUSH_RETRIES 15
#define MAX_CHUNK_RETRIES 40
#define MAX_DATA_SAMPLES 15


// file copy functions
void copyFile(C150DgmSocket *sock, string filename, string dirName,  
              int filenastiness, int attempt);
void sendBeginRequest(C150DgmSocket *sock, char filename[], string dirName,
                      size_t *fileSize, size_t *numTotalPackets, int attempt,
                      size_t *numTotalChunks, int *numPacketsInLastChunk);
void sendChunk(C150DgmSocket *sock, string filename, string dirName, 
               int filenastiness, size_t numTotalChunks, size_t currChunkNum, 
               int numPacketsInChunk);
unsigned char *findMostFrequentData(string sourceName, size_t offset, 
                                    size_t readAmount, int filenastiness);
unsigned char *extractDataFromFile(string sourceName, size_t offset, 
                                   size_t readAmount, int filenastiness);
char *sendDataPacket(C150DgmSocket *sock, string filename, 
                              string dirName, int filenastiness, 
                              size_t currChunkNum, int currPacketNum);
void resendFailedPackets(C150DgmSocket *sock, char incomingResponsePacket[], 
                        char outgoingRequestPacket[], vector<char *> chunk,
                        size_t numTotalChunks);

// end to end functions
void sendChecksumRequest(C150DgmSocket *sock, char filename[], 
                         char incomingResponsePacket[]);
bool sendChecksumConfirmation(C150DgmSocket *sock, char filename[], 
                              string dirName, int filenastiness, int attempt,
                              char incomingResponsePacket[]);
bool compareHash(char filename[], string dirName, int filenastiness, 
                 char incomingResponsePacket[], int attempt);

// utility functions
void sendAndRetry(C150DgmSocket *sock, char filename[], char outgoingPacket[], 
                  char incomingPacket[], int outgoingPacketType, 
                  int incomingPacketType, size_t currChunkNum);
void validatePacket(char incomingPacket[], int incomingPacketType, 
                    char filename[], int readlen, bool *timeout, 
                    int *numRetried, int *numFlushed, bool retryFlag,
                    size_t currChunkNum);


const int serverArg = 1;                  // server name is 1st arg
const int networknastinessArg = 2;        // networknastiness is 2nd arg
const int filenastinessArg = 3;           // filenastiness is 3rd arg
const int srcdirArg = 4;                  // srcdir is 4th arg  

  
int main(int argc, char *argv[]) {
    //
    //  DO THIS FIRST OR YOUR ASSIGNMENT WON'T BE GRADED!
    //
    GRADEME(argc, argv);

    // validate input format
    if (argc != 5) {
        fprintf(stderr,"Correct syntax is: %s <server> <networknastiness> <filenastiness> <srcdir>\n", argv[0]);
        exit(1);
    }

    // read nastiness from input
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

        char incomingResponsePacket[MAX_PACKET_LEN];
        bool transferSuccess = false;
        int numRetried = 0;
        char path[500];

        // check all files in the directory
        while (dirent *f = readdir(SRC)) {
            sprintf(path, "%s/%s", argv[srcdirArg], f->d_name);

            // skip all subdirectories
            if (!f->d_name || isDirectory(path)) continue; 
            
            // recopy a file up to MAX_FILE_RETRIES times
            while (transferSuccess == false && numRetried < MAX_FILE_RETRIES) {
                numRetried++;

                // file copy
                copyFile(sock, f->d_name, argv[srcdirArg], filenastiness, numRetried);

                // end to end check
                sendChecksumRequest(sock, f->d_name, incomingResponsePacket);
                transferSuccess = sendChecksumConfirmation(sock, f->d_name, argv[srcdirArg], filenastiness, numRetried, incomingResponsePacket);
            }

            transferSuccess = false;
            numRetried = 0;
        }

        closedir(SRC);
    } catch (C150NetworkException& e) {
        // Write to debug log
        c150debug->printf(C150ALWAYSLOG,"Caught C150NetworkException: %s\n",
                        e.formattedExplanation().c_str());
        // In case we're logging to a file, write to the console too
        cerr << argv[0] << ": caught C150NetworkException: " 
             << e.formattedExplanation() << endl;
    }

    return 0;
}


// ------------------------------------------------------
//
//                   copyFile
//
//  Given a filename, copy it to the server
//     
// ------------------------------------------------------
void copyFile(C150DgmSocket *sock, string filename, string dirName,  
              int filenastiness, int attempt) {
    size_t fileSize, numTotalPackets, numTotalChunks, numPacketsInChunk, currChunkNum = 0;
    int numPacketsInLastChunk, numRetried = 0;

    // send a begin packet to server to notify it to receive a new file
    sendBeginRequest(sock, (char *) filename.c_str(), dirName, &fileSize, 
                     &numTotalPackets, attempt, &numTotalChunks, 
                     &numPacketsInLastChunk);
    
    // keep sending while there are more chunks left
    while (currChunkNum < numTotalChunks) {
        // check if current chunk is last chunk
        numPacketsInChunk = (currChunkNum == numTotalChunks - 1) ? 
                                             numPacketsInLastChunk : CHUNK_SIZE;

        sendChunk(sock, filename, dirName, filenastiness, numTotalChunks, currChunkNum, numPacketsInChunk);

        currChunkNum++;
    }
}


/* --------------------------FILE COPY FUNCTIONS--------------------------- */

// --------------------------------------------------------------------------
//
//                            sendBeginRequest
//  Given a filename, calculate the file's stats and notify the server to
//  receive the file
//     
// --------------------------------------------------------------------------
void sendBeginRequest(C150DgmSocket *sock, char filename[], string dirName,
                      size_t *fileSize, size_t *numTotalPackets, int attempt,
                      size_t *numTotalChunks, int *numPacketsInLastChunk) {
    assert(sock != NULL);
    assert(filename != NULL);
    
    *GRADING << "File: " << filename << ", beginning transmission, attempt " << attempt << endl;

    char outgoingRequestPacket[MAX_PACKET_LEN];
    char incomingResponsePacket[MAX_PACKET_LEN];

    // load struct with members and write to packet
    BeginRequestPacket requestPacket;
    requestPacket.fileSize = *fileSize = getFileSize(filename, dirName);
    requestPacket.numTotalPackets = *numTotalPackets = (*fileSize + DATA_LEN - 1) / DATA_LEN; // see references up top
    requestPacket.numTotalChunks = *numTotalChunks = (*numTotalPackets + CHUNK_SIZE - 1) / CHUNK_SIZE;
    *numPacketsInLastChunk = *numTotalPackets % CHUNK_SIZE == 0 ? CHUNK_SIZE : *numTotalPackets % CHUNK_SIZE;
    memcpy(requestPacket.filename, filename, strlen(filename) + 1);
    memcpy(outgoingRequestPacket, &requestPacket, sizeof(requestPacket));

    // send begin request and wait for response
    sendAndRetry(sock, filename, outgoingRequestPacket, incomingResponsePacket, BEGIN_REQUEST_PACKET_TYPE, BEGIN_RESPONSE_PACKET_TYPE, SIZE_MAX);
}


// --------------------------------------------------------------------------
//
//                                sendChunk
//  Given a filename and a chunk number, keep sending untill all packets in
//  chunk are delivered, or give up after MAX_CHUNK_RETRIES attempts
//     
// --------------------------------------------------------------------------
void sendChunk(C150DgmSocket *sock, string filename, string dirName, 
               int filenastiness, size_t numTotalChunks, size_t currChunkNum, 
               int numPacketsInChunk) {
    assert(sock != NULL);

    char *outgoingDataPacket;
    vector<char *> chunk;
    char outgoingRequestPacket[MAX_PACKET_LEN];
    char incomingResponsePacket[MAX_PACKET_LEN];
    ChunkCheckRequestPacket requestPacket;

    // send chunk to server
    for (int i = 0; i < numPacketsInChunk; i++) {
        outgoingDataPacket = sendDataPacket(sock, filename, dirName, 
                                            filenastiness, currChunkNum, i);
        chunk.push_back(outgoingDataPacket);
    }

    // load struct with members and write to packet
    requestPacket.chunkNumber = currChunkNum;
    requestPacket.numPacketsInChunk = numPacketsInChunk;
    memcpy(requestPacket.filename, filename.c_str(), strlen(filename.c_str()) + 1);
    memcpy(outgoingRequestPacket, &requestPacket, sizeof(requestPacket));
    
    // send chunk check request and check if the received response has right chunk number
    sendAndRetry(sock, (char *) filename.c_str(), outgoingRequestPacket, incomingResponsePacket, CC_REQUEST_PACKET_TYPE, CC_RESPONSE_PACKET_TYPE, currChunkNum);

    // chunk check response received, start check bytemap and resend failed packets until all succeed
    resendFailedPackets(sock, incomingResponsePacket, outgoingRequestPacket, chunk, numTotalChunks);

    // free chunk
    for (auto dataPacket : chunk) free(dataPacket);
}


// --------------------------------------------------------------------------
//
//                                sendDataPacket
//  Given the identifier/location of a particular packet, extract it from
//  file and send to server
//     
// --------------------------------------------------------------------------
char *sendDataPacket(C150DgmSocket *sock, string filename, 
                              string dirName, int filenastiness, 
                              size_t currChunkNum, int currPacketNum) {
    // allocate memory to store data packet
    char *outgoingDataPacket = (char *) malloc(sizeof(DataPacket));

    // calculating data location
    string sourceName = makeFileName(dirName, filename);
    size_t fileSize = getFileSize(filename, dirName);
    size_t offset = (currChunkNum * CHUNK_SIZE + currPacketNum) * DATA_LEN;
    size_t readAmount = (offset + DATA_LEN < fileSize) ? DATA_LEN : fileSize - offset;

    // extract data over multiple samples and take the mode
    unsigned char *dataBuffer = findMostFrequentData(sourceName, offset, readAmount, filenastiness);

    // load struct with members and write to packet
    DataPacket dataPacket;
    dataPacket.chunkNumber = currChunkNum;
    dataPacket.packetNumber = currPacketNum;
    memcpy(dataPacket.filename, filename.c_str(), strlen(filename.c_str()) + 1);
    memcpy(dataPacket.data, dataBuffer, readAmount); 
    memcpy(outgoingDataPacket, &dataPacket, sizeof(dataPacket));

    // send packet to server
    sock -> write(outgoingDataPacket, MAX_PACKET_LEN);
    free(dataBuffer);

    return outgoingDataPacket;
}


// --------------------------------------------------------------------------
//
//                            findMostFrequentData
//  Given the identifier/location of a particular packet, sample it from file
//  MAX_DATA_SAMPLES times and return the most frequently appearing sample
//     
// --------------------------------------------------------------------------
unsigned char *findMostFrequentData(string sourceName, size_t offset, 
                                    size_t readAmount, int filenastiness) {
    unsigned char *dataBuffer = (unsigned char *) malloc(readAmount);
    unsigned char *extractedData;
    unordered_map<string, int> frequencyCount;
    int currentMax = 0;
    string mode;

    // sample data MAX_DATA_SAMPLES times
    for (int i = 0; i < MAX_DATA_SAMPLES; i++) {
        extractedData = extractDataFromFile(sourceName, offset, readAmount, filenastiness);
        string key(extractedData, extractedData + readAmount);
        frequencyCount[key]++;

        free(extractedData);
    }

    // find mode (see references) and copy to buffer
    for (auto it = frequencyCount.cbegin(); it != frequencyCount.cend(); ++it) {
        if (it ->second > currentMax) {
            mode = it->first;
            currentMax = it->second;
        }
    }

    memcpy(dataBuffer, mode.data(), readAmount);

    return dataBuffer;
}


// --------------------------------------------------------------------------
//
//                            extractDataFromFile
//  Given the identifier/location of a particular packet, extract it from file
//     
// --------------------------------------------------------------------------
unsigned char *extractDataFromFile(string sourceName, size_t offset, 
                                   size_t readAmount, int filenastiness) {
    NASTYFILE inputFile(filenastiness);
    unsigned char *buffer = (unsigned char *) malloc(readAmount);
    void *fopenretval;
    size_t len;

    // open file
    fopenretval = inputFile.fopen(sourceName.c_str(), "rb");
    if (fopenretval == NULL) {
      cerr << "Error opening input file " << sourceName << 
        " errno=" << strerror(errno) << endl;
      exit(12);
    }
    
    inputFile.fseek(offset, SEEK_SET);

    // read DATA_LEN bytes of data
    len = inputFile.fread(buffer, 1, readAmount);
    if (len != readAmount) {
      cerr << "Error reading file " << sourceName << 
        "  errno=" << strerror(errno) << endl;
      exit(16);
    }

    // close file
    if (inputFile.fclose() != 0 ) {
      cerr << "Error closing input file " << sourceName << 
        " errno=" << strerror(errno) << endl;
      exit(16);
    }

    return buffer;
}


// --------------------------------------------------------------------------
//
//                            resendFailedPackets
//  Resend all failed packets in chunk to server until all succeed or 
//  give up after MAX_CHUNK_RETRIES attempts
//     
// --------------------------------------------------------------------------
void resendFailedPackets(C150DgmSocket *sock, char incomingResponsePacket[], 
                        char outgoingRequestPacket[], vector<char *> chunk,
                        size_t numTotalChunks) {
    ChunkCheckResponsePacket *responsePacket;
    vector<int> failedPackets;
    bool firstRead = true;
    int numRetried = 0;

    // resend packets until all packets have been delivered or out of resends
    while (firstRead || (failedPackets.size() > 0 && numRetried < MAX_CHUNK_RETRIES)) {
        // reset state variables
        numRetried++;
        firstRead = false;
        failedPackets.clear();

        responsePacket = reinterpret_cast<ChunkCheckResponsePacket *>(incomingResponsePacket);

        // check which packets have failed
        for (int i = 0; i < responsePacket->numPacketsInChunk; i++) {
            if (responsePacket->chunkCheck[i] == false) {
                failedPackets.push_back(i);
            }
        }

        // resend all failed packets
        for (int i : failedPackets) sock -> write(chunk[i], MAX_PACKET_LEN);

        // send chunk check request to server and wait for response
        sendAndRetry(sock, responsePacket->filename, outgoingRequestPacket, incomingResponsePacket, CC_REQUEST_PACKET_TYPE, CC_RESPONSE_PACKET_TYPE, responsePacket->chunkNumber);
    }

    if (numRetried == MAX_CHUNK_RETRIES) {
        fprintf(stderr, "File %s's chunk %ld failed after %d retries", 
                responsePacket->filename, responsePacket->chunkNumber, 
                MAX_CHUNK_RETRIES);
    }
}


/* --------------------------END TO END FUNCTIONS--------------------------- */

// --------------------------------------------------------------------------
//
//                           sendChecksumRequest
//  Given a filename, assuming that the file had been delievered, send a 
//  checksum request to server and expect a response containing checksum
//  calculated on the server-side copy of the file
//     
// --------------------------------------------------------------------------
void sendChecksumRequest(C150DgmSocket *sock, char filename[], 
                         char incomingResponsePacket[]) {
    assert(sock != NULL);
    assert(filename != NULL);
    
    char outgoingRequestPacket[MAX_PACKET_LEN];
    ChecksumRequestPacket requestPacket;

    // load struct with members and write to packet
    memcpy(requestPacket.filename, filename, strlen(filename) + 1);
    memcpy(outgoingRequestPacket, &requestPacket, sizeof(requestPacket));

    *GRADING << "File: " << filename << " transmission complete, waiting for end-to-end check, attempt " << 0 << endl;

    // send checksum request to server and wait for response
    sendAndRetry(sock, filename, outgoingRequestPacket, incomingResponsePacket, CS_REQUEST_PACKET_TYPE, CS_RESPONSE_PACKET_TYPE, SIZE_MAX);
}

// --------------------------------------------------------------------------
//
//                           sendChecksumConfirmation
//  After receiving a checksum response from server, calculate checksum for
//  the local file and send the comparison result between local and server-
//  side checksums
//     
// --------------------------------------------------------------------------
bool sendChecksumConfirmation(C150DgmSocket *sock, char filename[], 
                              string dirName, int filenastiness, int attempt,
                              char incomingResponsePacket[]) {
    assert(sock != NULL);
    assert(filename != NULL);

    bool transferSuccess;
    char outgoingComparisonPacket[MAX_PACKET_LEN];
    char incomingFinishPacket[MAX_PACKET_LEN];
    ChecksumComparisonPacket comparisonPacket;

    // load struct with members and write to packet
    transferSuccess = comparisonPacket.comparisonResult = compareHash(filename, dirName, filenastiness, incomingResponsePacket, attempt);
    memcpy(comparisonPacket.filename, filename, strlen(filename) + 1);
    memcpy(outgoingComparisonPacket, &comparisonPacket, sizeof(comparisonPacket));

    // send checksum comparison to server and wait for finish packet
    sendAndRetry(sock, filename, outgoingComparisonPacket, incomingFinishPacket, CS_COMPARISON_PACKET_TYPE, FINISH_PACKET_TYPE, SIZE_MAX);

    return transferSuccess;
}


// --------------------------------------------------------------------------
//
//                                compareHash
//
//  Given an incoming checksum response packet, compare its value with locally
//  computed checksum
//     
// --------------------------------------------------------------------------
bool compareHash(char filename[], string dirName, int filenastiness, 
                 char incomingResponsePacket[], int attempt) {
    // flush wrong packe type
    if (getPacketType(incomingResponsePacket) != CS_RESPONSE_PACKET_TYPE) 
        return false; 
    
    unsigned char *localChecksum;
    ChecksumResponsePacket *responsePacket = reinterpret_cast<ChecksumResponsePacket *>(incomingResponsePacket);

    // flush if wrong file
    if (strcmp(filename, responsePacket->filename) != 0) return false;
        
    // compute local checksum
    localChecksum = findMostFrequentSHA(filename, dirName, filenastiness);

    // compare checksums
    if (memcmp(localChecksum, responsePacket->checksum, HASH_CODE_LENGTH) == 0) {
        *GRADING << "File: " << filename << " end-to-end check succeeded, attempt " << attempt << endl;
        return true;
    } else {
        *GRADING << "File: " << filename << " end-to-end check failed, attempt " << attempt << endl;
        return false;
    }

    free(localChecksum);
}


/* ----------------------------UTILITY FUNCTIONS---------------------------- */

// --------------------------------------------------------------------------
//
//                              sendAndRetry
//
//  Given a request packet, send and expect a response. If response is not 
//  received, retry up to MAX_PKT_RETRIES times before aborting on network 
//  failure.
//     
// --------------------------------------------------------------------------
void sendAndRetry(C150DgmSocket *sock, char filename[], char outgoingPacket[], 
                  char incomingPacket[], int outgoingPacketType, 
                  int incomingPacketType, size_t currChunkNum) {
    assert(sock != NULL);
    assert(filename != NULL);

    ssize_t readlen;              // amount of data read from socket
    int numRetried = 0, numFlushed = 0;
    bool timeout;
    
    // send request and read response
    sock -> write(outgoingPacket, MAX_PACKET_LEN);
    readlen = sock -> read(incomingPacket, MAX_PACKET_LEN);
    timeout = sock -> timedout();

    // retry if validation failed
    if (!timeout) validatePacket(incomingPacket, incomingPacketType, filename, readlen, &timeout, &numRetried, &numFlushed, false, currChunkNum);
    
    // keep resending message up to MAX_PKT_RETRIES times when read timedout
    while (timeout == true && numRetried < MAX_PKT_RETRIES && numFlushed < MAX_FLUSH_RETRIES) {
        numRetried++;

        // send request and read response again
        sock -> write(outgoingPacket, MAX_PACKET_LEN);
        readlen = sock -> read(incomingPacket, MAX_PACKET_LEN);
        timeout = sock -> timedout();

        // retry if validation failed
        if (!timeout) validatePacket(incomingPacket, incomingPacketType, filename, readlen, &timeout, &numRetried, &numFlushed, true, currChunkNum);
    }

    // throw exception if all retries exceeded
    string errorMsg = "Timed out after " + to_string(MAX_PKT_RETRIES) + " retries.";
    if (numRetried == MAX_PKT_RETRIES) throw C150NetworkException(errorMsg);
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
                    int *numRetried, int *numFlushed, bool retryFlag,
                    size_t currChunkNum) {
    char receivedFilename[FILENAME_LEN];
    int receivedPacketType;   
    string expectedIncomingType = packetTypeStringMatch(incomingPacketType);

    // validate size of received packet
    if (readlen == 0) {
        fprintf(stderr, "Read zero length message, trying again");
        *timeout = true;
    }

    // validate filename
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
    } else {
        // flush if wrong chunk number
        if (currChunkNum != SIZE_MAX) {
            if (receivedPacketType != CC_RESPONSE_PACKET_TYPE) {
                fprintf(stderr, "Error: should be expecting %d type but received %d type.\n", CC_RESPONSE_PACKET_TYPE, receivedPacketType);
                exit(99);
            }

            ChunkCheckResponsePacket *responsePacket = reinterpret_cast<ChunkCheckResponsePacket *>(incomingPacket);

            if (responsePacket->chunkNumber > currChunkNum) {
                fprintf(stderr, "Expected chunk number is %ld but received later chunk %ld\n", currChunkNum, responsePacket->chunkNumber);
                exit(99);
            } else if (responsePacket->chunkNumber < currChunkNum) {
                fprintf(stderr, "Expected chunk number is %ld but received earlier chunk %ld\n", currChunkNum, responsePacket->chunkNumber);
                *timeout = true;
                if (retryFlag) {
                    *numRetried -= 1;
                    *numFlushed += 1;
                }
            }
        }
    }
}

