#ifndef NASTYFILE_IO_H
#define NASTYFILE_IO_H

int getPacketType(char incomingPacket[]);
string packetTypeStringMatch(int packetType);
void getFilename(char incomingPacket[], char filename[]);
bool isDirectory(char *dirname);
unsigned char *copyFile(string sourceDir, string fileName, int nastiness, 
                        int *filesize);
string makeFileName(string dir, string name);
bool isFile(string fname);

#endif