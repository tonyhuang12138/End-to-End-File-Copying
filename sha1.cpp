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


#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <openssl/sha.h>
#include <iostream>
#include <string.h>

using namespace std;
void sha1();

int main(int argc, char *argv[])
{
	sha1();
	return 0;
}



void sha1() {
	unsigned char obuf[20];
	unsigned char buffer[10000];

	string filename = "Makefile";
	FILE* infile = fopen(filename.c_str(), "rb");


	// // traverse infile and return its size
	// fseek(infile, 0, SEEK_END);

	// // if (ftell(infile) < 0) {
	// // 	cerr << "Error occurred when traversing file to find file size." << endl;
	// // 	// delete t;
	// // 	// delete buffer;
	// // 	// return;
	// // } 
	
	// size_t fileSize = ftell(infile);

	// cout << "File size is " << fileSize << endl;
	// // fseek(infile, 0, SEEK_SET);
	// rewind(infile);

	// read infile into char buffer
	size_t bytesRead = fread(buffer, 1, 2527, infile);
	buffer[2528] = '\0';

	cout << "Bytes read is " << bytesRead << endl;

	// if (bytesRead != fileSize) {
	// 	cerr << "There is an error reading the supplied infile!!!!!" << endl;
	// 	// fclose(infile);
	// 	// delete t;
	// 	// delete buffer;
	// 	return;
	// }

	cerr << "HIIIII!!!!" << endl;
	fclose(infile);

	// for (size_t i = 0; i < bytesRead; ++i) {
	// 	printf("%c", buffer[i]);
	// }


	printf ("SHA1 (\"%s\") = ", filename.c_str());
	SHA1(buffer, sizeof(buffer), obuf);


	for (int i = 0; i < 20; i++)
	{
		printf ("%02x", (unsigned int) obuf[i]);
	}
	printf ("\n");
}
