#ifndef NASTYFILE_IO_H
#define NASTYFILE_IO_H

char *copyFile(string sourceDir, string fileName, int nastiness);
string makeFileName(string dir, string name);
bool isFile(string fname);

#endif