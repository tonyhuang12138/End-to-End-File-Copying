#ifndef SHA1_H
#define SHA1_H

#include <string>

using namespace std;

#define HASH_CODE_LENGTH 20


// --------------------------------------------------------------------------
//
//                                sha1
//
//  Given a filename, compute and return the corresponding file's checksum
//     
// --------------------------------------------------------------------------
unsigned char *sha1(string filename, string dirName, int nastiness);

#endif
