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
size_t getTotalPackets(string filename, string dirName);
void sendChunk(C150DgmSocket *sock, string filename, string dirName, 
               int filenastiness, size_t numTotalPackets, size_t currChunkNum, 
               int numPacketsInChunk);
void sendDataPacket(C150DgmSocket *sock, string filename, string dirName, 
                    int filenastiness, size_t numTotalPackets, 
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
    size_t fileSize, numTotalPackets, numTotalChunks, currChunkNum = 0;
    int numPacketsInLastChunk, numRetried = 0;

    // calculate number of chunks and packets in file
    fileSize = getFileSize(filename, dirName);
    numTotalPackets = (fileSize + DATA_LEN - 1) / DATA_LEN; // see references up top
    numTotalChunks = (numTotalPackets + CHUNK_SIZE - 1) / CHUNK_SIZE;
    numPacketsInLastChunk = numTotalPackets % CHUNK_SIZE == 0 ? CHUNK_SIZE : numTotalPackets % CHUNK_SIZE;
    printf("%s: total size is %ld, number of packets is %ld and number of chunks is %ld. The last chunk has %d packets\n", filename.c_str(), fileSize, numTotalPackets, numTotalChunks, numPacketsInLastChunk);

    // TODO: remove. testing chunk read with local file write
    // unsigned char *file = (unsigned char *) malloc(fileSize+1);
    
    // keep sending while there are more chunks left
    while (currChunkNum < numTotalChunks) {
        printf("Getting chunk %ld\n", currChunkNum);
        if (currChunkNum == numTotalChunks - 1) {
            printf("Processing last chunk with %d packets\n", numPacketsInLastChunk);
            // extract data from file and send to server as packets
            sendChunk(sock, filename, dirName, filenastiness, numTotalPackets, currChunkNum, numPacketsInLastChunk);
            
        } else {
            sendChunk(sock, filename, dirName, filenastiness, numTotalPackets, currChunkNum, CHUNK_SIZE);
        }

        // for (size_t i = 0; i < chunk.size(); i++) {
        //     printf("Chunk size is %ld\n", chunk.size());
        //     DataPacket *dp = reinterpret_cast<DataPacket *>(chunk[i]);
        //     printf("Unpacking packet %s, %ld, %ld, %d\n", dp->filename, dp->numTotalPackets, dp->chunkNumber, dp->packetNumber);
        //     printf("Verifying packet type: %d\n", dp->packetType);
        //     printf("Offset is %ld\n", currChunkNum * CHUNK_SIZE + i);
        //     size_t offset = (dp->chunkNumber * CHUNK_SIZE + dp->packetNumber) * DATA_LEN;
        //     size_t packetSize = (offset + DATA_LEN < fileSize) ? DATA_LEN : fileSize - offset;

        //     printf("Offset: %ld and write amount: %ld\n", offset, packetSize);
        //     memcpy(file + offset, dp->data, packetSize);
        // }

        // sendAndRetry(sock, filename, chunk, incomingCheckPacket, DATA_PACKET_TYPE, CHUNK_CHECK_PACKET_TYPE, true);
        // readChunkCheck(incomingCheckPacket);

        // // keep sending chunks until full chunk is successfully delivered
        // while (sizeof(failedPackets) != 0 && numRetried < MAX_CHUNK_RETRIES) {
        //     readChunkCheck(incomingCheckPacket);
        //     numRetried++;
        // }

        currChunkNum++;
    }

    // // TODO: remove. write buffer to file
    // NASTYFILE outputFile(filenastiness);
    // string outputpath = "./write_output/" + filename;
    // void *fopenretval2 = outputFile.fopen(outputpath.c_str(), "wb");  
    // //
    // // Write the whole file
    // //
    // size_t len = outputFile.fwrite(file, 1, fileSize);
    // if (len != fileSize) {
    //   cerr << "Error writing file " << outputpath.c_str() << 
	//       "  errno=" << strerror(errno) << endl;
    //   exit(16);
    // }
  
    // if (outputFile.fclose() == 0 ) {
    //    cout << "Finished writing file " << outputpath.c_str() <<endl;
    // } else {
    //   cerr << "Error closing output file " << outputpath.c_str() << 
	//       " errno=" << strerror(errno) << endl;
    //   exit(16);
    // }

    // printf("About to free file\n");
    // free(file);

    printf("%s copied to server.\n", filename.c_str());
}


void sendChunk(C150DgmSocket *sock, string filename, string dirName, 
               int filenastiness, size_t numTotalPackets, size_t currChunkNum, 
               int numPacketsInChunk) {
    assert(sock != NULL);

    // send chunk to server
    for (int i = 0; i < numPacketsInChunk; i++) {
        sendDataPacket(sock, filename, dirName, filenastiness, numTotalPackets, currChunkNum, i);
    }

    // sendChunkCheckRequest(sock, filename, );

    ChunkCheckRequestPacket requestPacket;
    char incomingResponsePacket[MAX_PACKET_LEN];
    char outgoingRequestPacket[MAX_PACKET_LEN];

    // request for chunk check
    requestPacket.chunkNumber = currChunkNum;
    requestPacket.numPacketsInChunk = numPacketsInChunk;
    memcpy(requestPacket.filename, filename.c_str(), strlen(filename.c_str()) + 1);
    memcpy(outgoingRequestPacket, &requestPacket, sizeof(requestPacket));
    
    sendAndRetry(sock, (char *) filename.c_str(), outgoingRequestPacket, incomingResponsePacket, CC_REQUEST_PACKET_TYPE, CC_RESPONSE_PACKET_TYPE);

    // chunk check response received, start check bytemap and resend failed packets until all succeed
    resendFailedPackets(sock, );
    sendAndRetry(sock, (char *) filename.c_str(), outgoingRequestPacket, incomingResponsePacket, CC_REQUEST_PACKET_TYPE, CC_RESPONSE_PACKET_TYPE);
}


void sendDataPacket(C150DgmSocket *sock, string filename, string dirName, 
                    int filenastiness, size_t numTotalPackets, 
                    size_t currChunkNum, int currPacketNum) {
    char outgoingDataPacket[MAX_PACKET_LEN];
    unsigned char *buffer = (unsigned char *) malloc(DATA_LEN);

    string sourceName = makeFileName(dirName, filename);
    size_t fileSize = getFileSize(filename, dirName);
    void *fopenretval;
    size_t offset, readAmount, len;
    NASTYFILE inputFile(filenastiness);
    DataPacket dataPacket;

    // open file
    printf("Generating data packet for file %s, chunk %ld, packet %d\n", filename.c_str(), currChunkNum, currPacketNum);

    fopenretval = inputFile.fopen(sourceName.c_str(), "rb");
    if (fopenretval == NULL) {
      cerr << "Error opening input file " << sourceName << 
        " errno=" << strerror(errno) << endl;
      exit(12);
    }
    printf("File read succeeded\n");

    offset = (currChunkNum * CHUNK_SIZE + currPacketNum) * DATA_LEN;
    printf("Offset is %ld\n", offset);
    printf("About to read\n");
    inputFile.fseek(offset, SEEK_SET);

    // read DATA_LEN bytes of data
    // TOOD: < or <=?
    readAmount = (offset + DATA_LEN < fileSize) ? DATA_LEN : fileSize - offset;
    printf("Read amount is %ld\n", readAmount);
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

    // TODO: compare cache with disk?

    // generating data packet
    dataPacket.numTotalPackets = numTotalPackets;
    dataPacket.chunkNumber = currChunkNum;
    dataPacket.packetNumber = currPacketNum;
    memcpy(dataPacket.filename, filename.c_str(), strlen(filename.c_str()) + 1);
    memcpy(dataPacket.data, buffer, DATA_LEN); // TODO: +1?
    memcpy(outgoingDataPacket, &dataPacket, sizeof(dataPacket));

    sock -> write(outgoingDataPacket, MAX_PACKET_LEN);
    free(buffer);
}


// void sendChunkCheckRequest(C150DgmSocket *sock, char filename[], 
//                            int currChunkNum) {
//     assert(sock != NULL);
//     assert(filename != NULL);

//     ChunkCheckRequestPacket requestPacket;
//     char incomingResponsePacket[MAX_PACKET_LEN];

//     // request for chunk check
//     requestPacket.chunkNumber = currChunkNum;
//     memcpy(requestPacket.filename, filename, strlen(filename) + 1);
//     memcpy(outgoingRequestPacket, &requestPacket, sizeof(requestPacket));
    
//     sendAndRetry(sock, (char *) filename.c_str(), outgoingRequestPacket, incomingResponsePacket, CC_REQUEST_PACKET_TYPE, CC_RESPONSE_PACKET_TYPE);
// }

void resendFailedPackets(C150DgmSocket *sock, char filename[], char incomingResponsePacket[]) {
    ChunkCheckResponsePacket *responsePacket = reinterpret_cast<ChunkCheckResponsePacket *>(incomingResponsePacket);

    // assuming that chunkCheck is formed correctly
    vector<int> failedPackets;
    for (int i = 0; i < responsePacket->numPacketsInChunk; i++) {
        if (responsePacket->numPacketsInChunk[i] == false) {
            failedPackets.push_back(i);
        }
    }

    // TODO: optimization - read from memory instead of disk

    // resend all failed packets
    if (failedPackets.size() > 0) {
        for (int v : failedPackets) {
            sendDataPacket(sock, filename, dirName, filenastiness, numTotalPackets, currChunkNum, v);
        }
    }

    // if responsePacket->bytemap not all 1 -> recalculate corresponding packets and resend
    // for all i in failedPackets, sendDataPacket()
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

