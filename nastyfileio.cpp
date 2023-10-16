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
        case DATA_PACKET_TYPE:
            return "data";

        case CHUNK_CHECK_PACKET_TYPE:
            return "chunk check";
        
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
    printf("Filename is %s\n", filename);
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
//                   copyFile
//
// Copy a single file from sourcdir to target dir
//
// ------------------------------------------------------

unsigned char *copyFile(string sourceDir, string fileName, int nastiness, 
                        int *filesize) {
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

  //   try {

    //
    // Read whole input file 
    //
    if (lstat(sourceName.c_str(), &statbuf) != 0) {
      fprintf(stderr,"copyFile: Error stating supplied source file %s\n", sourceName.c_str());
      exit(20);
    }

    //
    // Make an input buffer large enough for
    // the whole file
    //
    sourceSize = statbuf.st_size;
    buffer = (unsigned char *)malloc(sourceSize);

    //
    // Define the wrapped file descriptors
    //
    // All the operations on outputFile are the same
    // ones you get documented by doing "man 3 fread", etc.
    // except that the file descriptor arguments must
    // be left off.
    //
    // Note: the NASTYFILE type is meant to be similar
    //       to the Unix FILE type
    //
    NASTYFILE inputFile(nastiness);      // See c150nastyfile.h for interface
    // NASTYFILE outputFile(nastiness);     // NASTYFILE is supposed to
                                          // remind you of FILE
                                          //  It's defined as: 
                                          // typedef C150NastyFile NASTYFILE

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


    // 
    // Read the whole file
    //
    len = inputFile.fread(buffer, 1, sourceSize);
    if (len != sourceSize) {
      cerr << "Error reading file " << sourceName << 
        "  errno=" << strerror(errno) << endl;
      exit(16);
    }

    if (inputFile.fclose() != 0 ) {
      cerr << "Error closing input file " << sourceName << 
        " errno=" << strerror(errno) << endl;
      exit(16);
    }

    //
    // Handle any errors thrown by the file framekwork
    //
//   }   catch (C150Exception& e) {
//        cerr << "nastyfiletest:copyfile(): Caught C150Exception: " << 
// 	       e.formattedExplanation() << endl;
    
//   }
    *filesize = len;

    return buffer;
}
