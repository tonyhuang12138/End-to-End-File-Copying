#ifndef NASTYFILE_IO_H
#define NASTYFILE_IO_H

#define MAX_SHA_SAMPLES 101

// --------------------------------------------------------------------------
//
//                           getPacketType
//
//  Given an incoming packet, extract and return the packet type
//     
// --------------------------------------------------------------------------
int getPacketType(char incomingPacket[]);

// --------------------------------------------------------------------------
//
//                           packetTypeStringMatch
//
//  Given a packet type integer, return the corresponding string
//     
// --------------------------------------------------------------------------
string packetTypeStringMatch(int packetType);

// --------------------------------------------------------------------------
//
//                            findMostFrequentSHA
//  Given the name of a file, sample its SHA1 MAX_SAMPLES times and return 
//  the most frequently appearing sample
//     
// --------------------------------------------------------------------------
unsigned char *findMostFrequentSHA(string filename, string dirName, 
                                   int filenastiness);

// ------------------------------------------------------
//
//                   getFilename
//
//  Given an incoming packet, extract and return the 
//  filename
//     
// ------------------------------------------------------                                   
void getFilename(char incomingPacket[], char filename[]);

// ------------------------------------------------------
//
//                   getFileSize
//
//  Given name and directory for a file, return its size
//     
// ------------------------------------------------------
size_t getFileSize(string filename, string dirName);

// ------------------------------------------------------
//
//                   isDirectory
//
//  Check if the supplied file name is a directory
//  
//  See references up top
//     
// ------------------------------------------------------
bool isDirectory(char *dirname);

// ------------------------------------------------------
//
//                   isFile
//
//  Make sure the supplied file is not a directory or
//  other non-regular file.
//     
// ------------------------------------------------------
unsigned char *bufferFile(string sourceDir, string fileName, int nastiness, 
                          size_t *filesize);

// ------------------------------------------------------
//
//                   makeFileName
//
// Put together a directory and a file name, making
// sure there's a / in between
//
// ------------------------------------------------------
string makeFileName(string dir, string name);

// ------------------------------------------------------
//
//                   bufferFile
//
// Given a file path, load its entirety to a buffer and
// return it
//
// ------------------------------------------------------
bool isFile(string fname);

#endif