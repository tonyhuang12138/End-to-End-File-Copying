#ifndef NASTYFILE_IO_H
#define NASTYFILE_IO_H

unsigned char *copyFile(string sourceDir, string fileName, int nastiness, 
                        int *filesize);
string makeFileName(string dir, string name);
bool isFile(string fname);

#endif