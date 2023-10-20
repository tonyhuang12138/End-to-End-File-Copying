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
#include "sha1.h"

using namespace std;
using namespace C150NETWORK;


// --------------------------------------------------------------------------
//
//                                sha1
//
//  Given a filename, compute and return the corresponding file's checksum
//     
// --------------------------------------------------------------------------
unsigned char *sha1(string filename, string dirName, int nastiness) {
	size_t readlen = SIZE_MAX;
	unsigned char *obuf = (unsigned char *) malloc(HASH_CODE_LENGTH + 1);
	unsigned char *buffer = bufferFile(dirName.c_str(), filename, nastiness, &readlen);
	if (buffer == NULL or readlen == SIZE_MAX) {
		fprintf(stderr,"Error reading file %s into buffer.\n", filename.c_str());
    	return NULL;
	}

	SHA1(buffer, readlen, obuf);
	
	free(buffer);

	return obuf;
}
