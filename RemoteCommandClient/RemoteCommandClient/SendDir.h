#ifndef SENDDIR_H
#define SENDDIR_H

#include "TCPBlockTransRecv.h"
#include <stdio.h>
#include <io.h>         // _findfirst, _findnext, _findclose
#include <sys/stat.h>   // _stat
#include <direct.h>     // _mkdir
#include <windows.h>    // Untuk strcpy_s, strerror_s


#define PATH_MAX 128

#define CLOSE_SOCKET(sock) \
    if ((sock) != INVALID_SOCKET) { \
        shutdown((sock), SD_SEND); \
        closesocket((sock)); \
        (sock) = INVALID_SOCKET; \
    }

#define FREE_AND_NULL(ptr) do { if ((ptr) != NULL) { free(ptr); (ptr) = NULL; } } while(0)

//Deklarasi fungsi-fungsi
void sendDir(SOCKET socket, const char* base_path, const char* curr_path);
void receiveDir(SOCKET socket, bool btimeout);

#endif // SENDDIR_H
