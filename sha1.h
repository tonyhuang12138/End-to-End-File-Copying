#ifndef SHA1_H
#define SHA1_H

#include <string>

using namespace std;

#define HASH_CODE_LENGTH 20

void sha1(string filename, string dirName, int nastiness, unsigned char* obuf);

#endif
