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

    unsigned char checksum[20];
    sha1(filename, dirName, 0, checksum);
    printf("Printing calculated checksum: ");
    for (int i = 0; i < 20; i++)
    {
        printf ("%02x", (unsigned int) checksum[i]);
    }
    printf ("\n");

    return 0;
}