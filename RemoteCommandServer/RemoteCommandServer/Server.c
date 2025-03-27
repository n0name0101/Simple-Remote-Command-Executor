#include "EstablishTCPConnection.h"
#include "TCPBlockTransRecv.h"
#include "SendFile.h"
#include "SendDir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <process.h>
#include <stdbool.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_COMMAND 2048
#define SERVER_PORT "8080"

#define FREE_AND_NULL(ptr) do { if ((ptr) != NULL) { free(ptr); (ptr) = NULL; } } while(0)

char* sendCommandAndGetResult(SOCKET socket1, SOCKET socket2, char* command) {
    char* returnValue = NULL;
    if (sendDataWithChecksum(socket1, command, strlen(command) + 1) == 0 && receiveDataWithChecksum(socket2, &returnValue, TRUE) >= 0)
        return returnValue;
    return NULL;
}

void flushSocketBuffer(SOCKET sock) {
    u_long mode = 1;  // 1 = non-blocking mode
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        printf("Gagal mengatur socket ke mode non-blocking, error: %d\n", WSAGetLastError());
        return;
    }

    char tmpBuf[1024];
    int bytesRead;

    // Baca semua data yang tersisa pada buffer kernel
    while ((bytesRead = recv(sock, tmpBuf, sizeof(tmpBuf), 0)) > 0) {
        // Data dibaca dan diabaikan
    }

    if (bytesRead == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {  // WSAEWOULDBLOCK berarti tidak ada data lagi
            printf("Error saat membaca data socket, error: %d\n", err);
        }
    }

    // Kembalikan socket ke mode blocking (0 = blocking mode)
    mode = 0;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        printf("Gagal mengembalikan socket ke mode blocking, error: %d\n", WSAGetLastError());
    }
}

int main() {
    system("chcp 65001");
    char command[MAX_COMMAND];
    WSADATA wsaData;
    int iResult;
    SOCKET listen_socket = INVALID_SOCKET, socket1 = INVALID_SOCKET, socket2 = INVALID_SOCKET, socket3 = INVALID_SOCKET;
    struct addrinfo* result = NULL, hints;

    // Inisialisasi Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup gagal: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;    // Untuk bind

    // Resolusi alamat untuk server
    iResult = getaddrinfo(NULL, SERVER_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo gagal: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Buat socket untuk mendengarkan koneksi
    listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listen_socket == INVALID_SOCKET) {
        printf("Error pada socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Bind socket
    iResult = bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("Bind gagal dengan error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);

    // Listen pada socket
    iResult = listen(listen_socket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("Listen gagal dengan error: %d\n", WSAGetLastError());
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    printf("Waiting for connection...\n");
    socket1 = accept(listen_socket, NULL, NULL);
    if (socket1 == INVALID_SOCKET) {
        printf("Accept gagal: %d\n", WSAGetLastError());
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    socket2 = accept(listen_socket, NULL, NULL);
    if (socket2 == INVALID_SOCKET) {
        printf("Accept gagal: %d\n", WSAGetLastError());
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    // Tutup socket listening karena tidak lagi diperlukan
    closesocket(listen_socket);
    MessageBeep(-1);

    char* commandResult = NULL;
    while (1) {
        commandResult = sendCommandAndGetResult(socket1, socket2, "getcurrentpath");
        printf("%s>", commandResult);

        free(commandResult);
        commandResult = NULL;


        fgets(command, sizeof(command), stdin);
        // Remove newline character from input
        command[strcspn(command, "\n")] = 0;
        if (strlen(command) > 0) {
            if (strcmp(command, "exit") == 0) {
                break;
            }
            else if (strncmp(command, "download ", 9) == 0) {
                if (sendDataWithChecksum(socket1, command, strlen(command) + 1) == 0 &&
                    (socket3 = establishTCPListening(SERVER_PORT, 2)) != INVALID_SOCKET) {
                    char* fileType = NULL;
                    receiveDataWithChecksum(socket3, &fileType, TRUE);
                    if (strncmp(fileType, "FILE ", 4) == 0) {
                        int receiveFileResult = receiveFile(socket3, TRUE);
                        if (receiveFileResult == 0)
                            printf("Download Success\n");
                        else if (receiveFileResult == -2) {
                            char message[265] = "Error: ";  // Inisialisasi dengan prefix "Error: "
                            // Tambahkan pesan error ke buffer, dimulai setelah "Error: "
                            strerror_s(message + strlen(message), sizeof(message) - strlen(message), errno);
                            strcat_s(message, sizeof(message), "\n");
                            printf("%s", message);
                            receiveDataWithChecksum(socket2, &commandResult, TRUE);
                            printf("%s", commandResult);
                        }
                        else {
                            receiveDataWithChecksum(socket2, &commandResult, TRUE);
                            printf("%s", commandResult);
                        }
                    }
                    else if (strncmp(fileType, "DIRECTORY ", 9) == 0)
                        receiveDir(socket1, socket2, socket3, "127.0.0.1", SERVER_PORT, TRUE);
                    else
                        printf("%s", fileType);
                    FREE_AND_NULL(fileType);
                }
                else {
                    Sleep(2000);
                    printf("Download failed .... \n");
                }
            }
            else {
                commandResult = sendCommandAndGetResult(socket1, socket2, command);
                printf("%s", commandResult);
            }
        }
        FREE_AND_NULL(commandResult);
    }

    // Shutdown koneksi dan cleanup
    CLOSE_SOCKET(socket1);
    CLOSE_SOCKET(socket2);
    WSACleanup();
    return 0;
}
