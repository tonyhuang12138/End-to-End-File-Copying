#include <fstream>
#include <sstream>
#include <stdio.h>
#include <openssl/sha.h>
#include "sha1.h"
#include <string>

using namespace std;

int main(int argc, char *argv[])
{
    string filename = argv[1];
    string dirName = argv[2];

    sha1(filename, dirName);

    return 0;
}