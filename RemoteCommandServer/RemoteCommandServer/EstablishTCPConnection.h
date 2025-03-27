#ifndef ESTABLISHTCPCONNECTION_H
#define ESTABLISHTCPCONNECTION_H

#include <winsock2.h>

// Function Declaration
SOCKET establishTCPListening(const char* port, int waitingTime);
SOCKET establishTCPConnect(const char* serverAddress, const char* port, int retry);

#endif // ESTABLISHTCPCONNECTION_H