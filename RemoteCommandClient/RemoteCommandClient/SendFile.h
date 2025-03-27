#ifndef SENDFILE_H
#define SENDFILE_H

#include "TCPBlockTransRecv.h"
#include <stdio.h>

#define FILE_MAGIC 0xBBCD8638   // Nilai konstan untuk ending File Transfer

#define CLOSE_SOCKET(sock) \
    if ((sock) != INVALID_SOCKET) { \
        shutdown((sock), SD_SEND); \
        closesocket((sock)); \
        (sock) = INVALID_SOCKET; \
    }

//Deklarasi fungsi-fungsi
int sendFile(SOCKET socket, const char* filename, const char* savedfilename);
int receiveFile(SOCKET socket, const char* filename, bool btimeout);

#endif // SENDFILE_H
