#ifndef FSSAPI_H
#define FSSAPI_H

#include <stdbool.h>

int openFile( const char* pathname, int flags, /* */const char* trashdir);

int closeFile( const char* pathname);

int writeFile( const char* pathname, const char* trashdir );

int appendToFile( const char* pathname, void* buf, size_t size, const char* trashdir );

int readFile( const char* pathname, void** buf, size_t* size /*,const char* readdir*/);

int readNFiles( int n, const char* readdir);

int removeFile(const char* pathname);

int lockFile(const char* pathname);

int unlockFile(const char* pathname);

int openConnection(const char* sockname, int msec, const struct timespec abstime);

int closeConnection(const char* sockname);

int ezOpen(const char* sockname);

#endif
