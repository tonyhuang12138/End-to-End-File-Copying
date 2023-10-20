// 
//            sha1tstFIXED
//
//     Author: Noah Mendelsohn
//
//     Test programming showing use of computation of 
//     sha1 hash function.
//
//     NOTE: problems were discovered using the incremental
//     version of the computation with SHA1_Update. This 
//     version, which computes the entire checksum at once,
//     seems to be reliable (if less flexible).
//
//     Note: this must be linked with the g++ -lssl directive. 
//

#include "c150nastyfile.h"        // for c150nastyfile & framework
#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <openssl/sha.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "nastyfileio.h"
#include "packettypes.h"

using namespace std;
using namespace C150NETWORK;

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
    printf("Packet type is %s\n", packetTypeStringMatch(packetType).c_str());
    
    return packetType;
}

// --------------------------------------------------------------------------
//
//                           packetTypeStringMatch
//
//  Given a packet type integer, return the corresponding string
//     
// --------------------------------------------------------------------------
string packetTypeStringMatch(int packetType) {
    // match packet type for output message
    switch (packetType) {
        case BEGIN_REQUEST_PACKET_TYPE:
            return "begin request";

        case BEGIN_RESPONSE_PACKET_TYPE:
            return "begin response";

        case DATA_PACKET_TYPE:
            return "data";

        case CC_REQUEST_PACKET_TYPE:
            return "chunk check request";

        case CC_RESPONSE_PACKET_TYPE:
            return "chunk check response";
        
        case CS_REQUEST_PACKET_TYPE:
            return "checksum request";

        case CS_RESPONSE_PACKET_TYPE:
            return "checksum response";

        case CS_COMPARISON_PACKET_TYPE:
            return "checksum comparison";

        case FINISH_PACKET_TYPE:
            return "finish";

        default:
            fprintf(stderr, "Invalid packet type provided: %d\n", packetType);
            return "invalid type";
    }
}


unsigned char *findMostFrequentSHA(string filename, string dirName, 
                                   int filenastiness) {
    unsigned char *shaBuffer = (unsigned char *) malloc(HASH_CODE_LENGTH);
    unsigned char *checksum;
    unordered_map<string, int> frequencyCount;
    int currentMax = 0;
    string mode;

    // sample sha MAX_SAMPLES times
    for (int i = 0; i < MAX_SHA_SAMPLES; i++) {
        checksum = sha1(filename, dirName, filenastiness);

        if (checksum == NULL) continue;

        string key(checksum, checksum + HASH_CODE_LENGTH);
        frequencyCount[key]++;

        free(checksum);
    }

    // find mode (see references)
    for (auto it = frequencyCount.cbegin(); it != frequencyCount.cend(); ++it) {
        if (it ->second > currentMax) {
            mode = it->first;
            currentMax = it->second;
        }
    }

    printf ("SHA1 MODE (\"%s\") = ", filename.c_str());

    for (int i = 0; i < 20; i++)
    {
      printf ("%02x", (unsigned int) mode[i]);
    }
    printf ("appears %d times\n", currentMax);

    memcpy(shaBuffer, mode.data(), HASH_CODE_LENGTH);

    return shaBuffer;
}

// ------------------------------------------------------
//
//                   getFilename
//
//  Given an incoming packet, extract and return the 
//  filename
//     
// ------------------------------------------------------
void getFilename(char incomingPacket[], char filename[]) {
    strcpy(filename, incomingPacket + PACKET_TYPE_LEN);
    // printf("Getting filename: %s\n", filename);
}


// ------------------------------------------------------
//
//                   getFileSize
//
//  Given name and directory for a file, return its size
//     
// ------------------------------------------------------
size_t getFileSize(string filename, string dirName) {
    struct stat statbuf;
    string sourceName = makeFileName(dirName, filename);

    // reead whole input file and find its size
    if (lstat(sourceName.c_str(), &statbuf) != 0) {
      fprintf(stderr,"copyFile: Error stating supplied source file %s\n", sourceName.c_str());
     exit(20);
    }

    return statbuf.st_size;
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
//                   isFile
//
//  Make sure the supplied file is not a directory or
//  other non-regular file.
//     
// ------------------------------------------------------
bool isFile(string fname) {
  const char *filename = fname.c_str();
  struct stat statbuf;  
  if (lstat(filename, &statbuf) != 0) {
    fprintf(stderr,"isFile: Error stating supplied source file %s\n", filename);
    return false;
  }

  if (!S_ISREG(statbuf.st_mode)) {
    fprintf(stderr,"isFile: %s exists but is not a regular file\n", filename);
    return false;
  }
  return true;
}

// ------------------------------------------------------
//
//                   makeFileName
//
// Put together a directory and a file name, making
// sure there's a / in between
//
// ------------------------------------------------------

string makeFileName(string dir, string name) {
  stringstream ss;

  ss << dir;
  // make sure dir name ends in /
  if (dir.substr(dir.length()-1,1) != "/")
    ss << '/';
  ss << name;     // append file name to dir
  return ss.str();  // return dir/name
  
}

// ------------------------------------------------------
//
//                   bufferFile
//
// Given a file path, load its entirety to a buffer and
// return it
//
// ------------------------------------------------------

unsigned char *bufferFile(string sourceDir, string fileName, int nastiness, 
                          size_t *filesize) {
  //
  //  Misc variables, mostly for return codes
  //
  void *fopenretval;
  size_t len;
  string errorString;
  unsigned char *buffer;
  struct stat statbuf;  
  size_t sourceSize;

  //
  // Put together directory and filenames SRC/file TARGET/file
  //
  string sourceName = makeFileName(sourceDir, fileName);

  //
  // make sure the file we're copying is not a directory
  // 
  printf("Copying %s to buffer\n", fileName.c_str());
  if (!isFile(sourceName)) {
    cerr << "Input file " << sourceName << " is a directory or other non-regular file. Skipping" << endl;
    return NULL;
  }

  try {
    // read whole input file 
    if (lstat(sourceName.c_str(), &statbuf) != 0) {
      fprintf(stderr,"copyFile: Error stating supplied source file %s\n", sourceName.c_str());
      exit(20);
    }

    // make an input buffer large enough for the whole file
    sourceSize = statbuf.st_size;
    buffer = (unsigned char *)malloc(sourceSize);

    NASTYFILE inputFile(nastiness);

    // do an fopen on the input file
    fopenretval = inputFile.fopen(sourceName.c_str(), "rb");  
                                          // wraps Unix fopen
                                          // Note rb gives "read, binary"
                                          // which avoids line end munging

    if (fopenretval == NULL) {
      cerr << "Error opening input file " << sourceName << 
        " errno=" << strerror(errno) << endl;
      exit(12);
    }

    // read the whole file
    len = inputFile.fread(buffer, 1, sourceSize);
    if (len != sourceSize) {
      cerr << "Expected size " << sourceSize << " but read size " << len << endl;
      cerr << "Error reading file " << sourceName << 
        "  errno=" << strerror(errno) << endl;
      exit(16);
    }

    if (inputFile.fclose() != 0 ) {
      cerr << "Error closing input file " << sourceName << 
        " errno=" << strerror(errno) << endl;
      exit(16);
    }

  } catch (C150Exception& e) {
      cerr << "nastyfiletest:bufferFile(): Caught C150Exception: " << 
        e.formattedExplanation() << endl;
  }
  *filesize = len;

  return buffer;
}
