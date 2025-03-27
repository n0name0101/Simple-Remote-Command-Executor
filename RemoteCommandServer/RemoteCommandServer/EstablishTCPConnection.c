#include "EstablishTCPConnection.h"

SOCKET establishTCPListening(const char* port, int waitingTime) {
    struct addrinfo hints, * result = NULL;
    SOCKET listen_socket = INVALID_SOCKET, client_socket = INVALID_SOCKET;
    int iResult;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;      // For bind

    iResult = getaddrinfo(NULL, port, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
        return INVALID_SOCKET;
    }

    listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listen_socket == INVALID_SOCKET) {
        printf("socket() failed: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        return INVALID_SOCKET;
    }

    iResult = bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(listen_socket);
        return INVALID_SOCKET;
    }
    freeaddrinfo(result);

    iResult = listen(listen_socket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed: %d\n", WSAGetLastError());
        closesocket(listen_socket);
        return INVALID_SOCKET;
    }

    printf("Waiting for connection (timeout in %d seconds)...\n", waitingTime);

    // Use select() to wait for connection with a timeout
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(listen_socket, &readfds);

    struct timeval tv;
    tv.tv_sec = waitingTime;
    tv.tv_usec = 0;

    int sel = select(0, &readfds, NULL, NULL, &tv);
    if (sel > 0 && FD_ISSET(listen_socket, &readfds)) {
        client_socket = accept(listen_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET)
            printf("accept failed: %d\n", WSAGetLastError());
        else
            printf("Connected : Start Downloading ... \n");
    }
    else if (sel == 0)
        printf("Timeout waiting for connection.\n");
    else
        printf("select error: %d\n", WSAGetLastError());

    // Once a connection is accepted (or not), close the listening socket
    closesocket(listen_socket);
    return client_socket;
}

SOCKET establishTCPConnect(const char* serverAddress, const char* port, int retry) {
    struct addrinfo hints, * result = NULL;
    SOCKET connect_socket = INVALID_SOCKET;
    int iResult;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo(serverAddress, port, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
        return INVALID_SOCKET;
    }

    int attempts = 0;
    while (attempts < retry) {
        connect_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (connect_socket == INVALID_SOCKET) {
            printf("socket() failed: %ld\n", WSAGetLastError());
            freeaddrinfo(result);
            return INVALID_SOCKET;
        }

        iResult = connect(connect_socket, result->ai_addr, (int)result->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            printf("connect failed: %d (attempt %d/%d)\n", WSAGetLastError(), attempts + 1, retry);
            closesocket(connect_socket);
            connect_socket = INVALID_SOCKET;
            attempts++;
            Sleep(1000);  // wait 1 second before retrying
            continue;
        }
        else
            break;  // successful connection
    }
    freeaddrinfo(result);

    if (connect_socket == INVALID_SOCKET)
        printf("Unable to connect to server after %d attempts.\n", retry);

    return connect_socket;
}