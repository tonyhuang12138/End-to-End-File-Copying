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

using namespace std;
using namespace C150NETWORK;


void sha1(string filename, string dirName, int nastiness, unsigned char* obuf) {
	int readlen = -1;
	unsigned char *buffer = copyFile(dirName.c_str(), filename, nastiness, &readlen);
	if (buffer == NULL or readlen == -1) {
		fprintf(stderr,"Error reading file %s into buffer.\n", filename.c_str());
    	return;
	}

	printf("Buffer size is %d\n", readlen);
	printf ("SHA1 (\"%s\") = ", filename.c_str());
	SHA1(buffer, readlen, obuf);

	for (int i = 0; i < 20; i++)
	{
		printf ("%02x", (unsigned int) obuf[i]);
	}
	printf ("\n");
	free(buffer);
}
