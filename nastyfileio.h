#ifndef NASTYFILE_IO_H
#define NASTYFILE_IO_H

#define MAX_SHA_SAMPLES 101

int getPacketType(char incomingPacket[]);
string packetTypeStringMatch(int packetType);
unsigned char *findMostFrequentSHA(string filename, string dirName, 
                                   int filenastiness);
void getFilename(char incomingPacket[], char filename[]);
size_t getFileSize(string filename, string dirName);
bool isDirectory(char *dirname);
unsigned char *bufferFile(string sourceDir, string fileName, int nastiness, 
                          size_t *filesize);
string makeFileName(string dir, string name);
bool isFile(string fname);

#endif