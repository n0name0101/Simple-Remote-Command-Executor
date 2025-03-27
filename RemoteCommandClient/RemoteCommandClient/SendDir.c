#include "SendDir.h"
#include "EstablishTCPConnection.h"
#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib") 

char* concat(const char* s1, const char* s2) {
    // Hitung panjang masing-masing string
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    size_t total = len1 + len2 + 1;  // +1 untuk karakter null

    // Alokasi memori untuk string hasil
    char* result = malloc(total);
    if (result == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }

    // Salin s1 ke result dengan ukuran buffer total
    if (strcpy_s(result, total, s1) != 0) {
        fprintf(stderr, "Error in strcpy_s.\n");
        free(result);
        return NULL;
    }

    // Gabungkan s2 ke result dengan ukuran buffer total
    if (strcat_s(result, total, s2) != 0) {
        fprintf(stderr, "Error in strcat_s.\n");
        free(result);
        return NULL;
    }

    return result;
}

// Function that returns a pointer to the substring in fullPath
// starting at the first occurrence of folderName.
// If folderName is not found, it returns NULL.
const char* get_sub_path(const char* fullPath, const char* folderName) {
    // Use strstr to search for folderName within fullPath.
    const char* start = strstr(fullPath, folderName);
    return start;  // returns NULL if folderName is not found.
}

void get_error_message(char* err_buf, size_t buf_size) {
    strerror_s(err_buf, buf_size, errno);
}

void sendDirRecur(SOCKET socket1, SOCKET socket2, SOCKET socket3, const char* serverip, const char* port, const char* base_path, const char* curr_path) {
    const char* relative;
    if (strcmp(base_path, curr_path) == 0)
        relative = ".";
    else {
        relative = curr_path + strlen(base_path);
        if (relative[0] == '\\' || relative[0] == '/')
            relative++;
    }

    // Buat pola pencarian, misalnya "curr_path\*"
    char searchPath[PATH_MAX];
    if (snprintf(searchPath, sizeof(searchPath), "%s/*", curr_path) >= PATH_MAX) {
        printf("Error (Path too long) : %s \n", curr_path);
        return;
    }
    struct _finddata_t findData;
    intptr_t handle = _findfirst(searchPath, &findData);
    if (handle == -1) {
        char err_buf[256];
        get_error_message(err_buf, sizeof(err_buf));
        fprintf(stderr, "Error opening folder %s: %s\n", curr_path, err_buf);
        return;
    }
    do {
        if (strcmp(findData.name, ".") == 0 || strcmp(findData.name, "..") == 0)
            continue;
        char full_path[PATH_MAX];
        if (snprintf(full_path, sizeof(full_path), "%s/%s", curr_path, findData.name) >= PATH_MAX) {
            printf("Error (Path too long) : %s/%s \n", curr_path, findData.name);
            return;
        }
        const char* rel_entry = full_path + strlen(base_path);
        if (rel_entry[0] == '\\' || rel_entry[0] == '/')
            rel_entry++;

        struct _stat st;
        if (_stat(full_path, &st) < 0) {
            char err_buf[256];
            get_error_message(err_buf, sizeof(err_buf));
            fprintf(stderr, "Error in _stat for %s: %s\n", full_path, err_buf);
            continue;
        }
        if (st.st_mode & _S_IFDIR) {
            printf("DIRECTORY : %s \n", full_path);
            char* sendBuffer = concat("MKDIR ", get_sub_path(full_path, PathFindFileNameA(base_path)));
            if (sendBuffer != NULL && sendDataWithChecksum(socket3, sendBuffer, strlen(sendBuffer) + 1) == 0) {
                unsigned char* receivedData = NULL;
                if (receiveDataWithChecksum(socket3, &receivedData, TRUE) >= 0 && strcmp(receivedData, "MKDIR OK") == 0)
                    sendDirRecur(socket1, socket2, socket3, serverip, port, base_path, full_path);
                if (receivedData) free(receivedData); receivedData = NULL;
            }
            if (sendBuffer) free(sendBuffer); sendBuffer = NULL;
        }
        else if (st.st_mode & _S_IFREG) {
            printf("FILE : %s\n", full_path);
            if (sendDataWithChecksum(socket3, "FILE", 5) == 0) {
                int sendFileResult = sendFile(socket3, full_path, get_sub_path(full_path, PathFindFileNameA(base_path)));
                if (sendFileResult == -2) {
                    char message[265] = "Error: ";  // Inisialisasi dengan prefix "Error: "
                    // Tambahkan pesan error ke buffer, dimulai setelah "Error: "
                    strerror_s(message + strlen(message), sizeof(message) - strlen(message), errno);
                    strcat_s(message, sizeof(message), "\n");
                    sendDataWithChecksum(socket2, (unsigned char*)message, strlen(message) + 1);
                }
                else if (sendFileResult == -1)
                    sendDataWithChecksum(socket2, "Download Failed ... \n", 23);
                // Reset Socket3
                CLOSE_SOCKET(socket3);
                socket3 = establishTCPConnect(serverip, port, 2);
            }
        }
    } while (_findnext(handle, &findData) == 0);
    _findclose(handle);
}

void sendDir(SOCKET socket1, SOCKET socket2, SOCKET socket3, const char* serverip, const char* port, const char* base_path, const char* curr_path) {
    char* sendBuffer = concat("MKDIR ", PathFindFileNameA(base_path));
    if (sendBuffer != NULL && sendDataWithChecksum(socket3, sendBuffer, strlen(sendBuffer) + 1) == 0) {
        unsigned char* receivedData = NULL;
        if (receiveDataWithChecksum(socket3, &receivedData, TRUE) >= 0 && strcmp(receivedData, "MKDIR OK") == 0)
            sendDirRecur(socket1, socket2, socket3, serverip, port, base_path, curr_path);
        if (receivedData) free(receivedData); receivedData = NULL;
    }
    sendDataWithChecksum(socket3, "END 89324527", 13);
    CLOSE_SOCKET(socket3);
    if (sendBuffer) free(sendBuffer); sendBuffer = NULL;
}

void receiveDir(SOCKET socket1, SOCKET socket2, SOCKET socket3, const char* serverip, const char* port, bool btimeout) {
    unsigned char* receivedData = NULL;
    while (receiveDataWithChecksum(socket3, &receivedData, btimeout) >= 0) {
        if (strncmp(receivedData, "MKDIR ", 6) == 0) {
            printf("%s   ", receivedData);
            if (_mkdir(receivedData + 6) == 0 || errno == EEXIST) {
                printf("Success\n");
                sendDataWithChecksum(socket3, "MKDIR OK", 9);
            }
            else {
                printf("Failed\n");
                sendDataWithChecksum(socket3, "MKDIR NO", 9);
            }
        }
        else if (strcmp(receivedData, "FILE") == 0) {
            char* recvBuffer = NULL;
            int receiveFileResult = receiveFile(socket3, TRUE);
            if (receiveFileResult == 0)
                printf("Download Success\n");
            else if (receiveFileResult == -2) {
                char message[265] = "Error: ";  // Inisialisasi dengan prefix "Error: "
                // Tambahkan pesan error ke buffer, dimulai setelah "Error: "
                strerror_s(message + strlen(message), sizeof(message) - strlen(message), errno);
                strcat_s(message, sizeof(message), "\n");
                printf("%s", message);
                receiveDataWithChecksum(socket2, &recvBuffer, TRUE);
                printf("%s", recvBuffer);
            }
            else {
                receiveDataWithChecksum(socket2, &recvBuffer, TRUE);
                printf("%s", recvBuffer);
            }
            if (recvBuffer) free(recvBuffer); recvBuffer = NULL;

            // Reset Socket3
            CLOSE_SOCKET(socket3);
            socket3 = establishTCPListening(port, 2);
        }
        else if (strcmp(receivedData, "END 89324527") == 0) {
            printf("Completed \n");
            break;
        }
        else
            printf("%s \n", receivedData);

        FREE_AND_NULL(receivedData);
    }
    FREE_AND_NULL(receivedData);
    CLOSE_SOCKET(socket3);
    return;
}