#include "SendFile.h"

// Function to clear the current console line.
void clearCurrentLine() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD cursorPos;
    DWORD written;

    if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        cursorPos.X = 0;
        cursorPos.Y = csbi.dwCursorPosition.Y;
        // Fill the entire line with spaces.
        FillConsoleOutputCharacter(hConsole, ' ', csbi.dwSize.X, cursorPos, &written);
        // Reset the cursor to the beginning of the line.
        SetConsoleCursorPosition(hConsole, cursorPos);
    }
}

// Function to print the download progress with a progress bar and colored output.
void printDownloadProgress(double speedKBps, unsigned long long downloadedBytes, unsigned long long totalBytes) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);

    // Clear the current line first.
    clearCurrentLine();

    double progress = (double)downloadedBytes / totalBytes;
    int barWidth = 20;
    int pos = (int)(barWidth * progress);

    // Print the progress bar.
    printf("[");
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) {
            // Completed portion: green color.
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            printf("=");
        }
        else if (i == pos) {
            // Current position marker: yellow.
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            printf(">");
        }
        else {
            // Incomplete portion: default gray.
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            printf(" ");
        }
    }
    // Reset color and close the progress bar.
    SetConsoleTextAttribute(hConsole, csbi.wAttributes);
    printf("] ");

    // Print percentage, speed, and downloaded sizes in cyan.
    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf("%.2f%% [%.2f MB/%.2f MB], Speed: %.2f KB/s",
        progress * 100,
        downloadedBytes / 1048576.0,
        totalBytes / 1048576.0,
        speedKBps);
    // Reset to original color.
    SetConsoleTextAttribute(hConsole, csbi.wAttributes);

    fflush(stdout);
}

int sendFile(SOCKET socket, const char* filename, const char* savedfilename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("Gagal membuka file %s\n", filename);
        CLOSE_SOCKET(socket);
        return -2;
    }

    //Send saved filename first
    if (sendDataWithChecksum(socket, savedfilename, strlen(savedfilename) + 1) < 0) {
        printf("Gagal mengirim saved filename.\n");
        fclose(fp);
        CLOSE_SOCKET(socket);
        return -1;
    }

    // Move file pointer to the end to get the file size.
    if (_fseeki64(fp, 0, SEEK_END) != 0) {
        perror("Error seeking to end of file");
        fclose(fp);
        return -1;
    }
    // Get the file size as a 64-bit integer.
    __int64 filesize = _ftelli64(fp);
    if (filesize < 0) {
        perror("Error determining file size");
        fclose(fp);
        return -1;
    }
    // Move the file pointer back to the beginning.
    _fseeki64(fp, 0, SEEK_SET);

    char* sendBuffer = malloc(strlen(filename) + 1 + sizeof(filesize));
    memcpy(sendBuffer, filename, strlen(filename) + 1);
    memcpy(sendBuffer + strlen(filename) + 1, (char*)(&filesize), sizeof(filesize));

    // Sending File Size
    if (sendDataWithChecksum(socket, sendBuffer, strlen(filename) + 1 + sizeof(filesize)) < 0) {
        printf("Gagal mengirim data file.\n");
        fclose(fp);
        CLOSE_SOCKET(socket);
        return -1;
    }
    if (sendBuffer) free(sendBuffer); sendBuffer = NULL;

    // Sediakan buffer lokal sebesar 8KB
    unsigned char buffer[1024 * 32];
    size_t bytesRead;

    // Baca file secara berkala dan kirim tiap blok menggunakan sendDataWithChecksum
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (sendDataWithChecksum(socket, buffer, bytesRead) < 0) {
            printf("Gagal mengirim data file.\n");
            fclose(fp);
            CLOSE_SOCKET(socket);
            return -1;
        }
    }
    fclose(fp);

    // Setelah file habis, kirim magic number sebagai tanda akhir transfer file
    uint32_t magicNumber = FILE_MAGIC;
    if (sendDataWithChecksum(socket, (unsigned char*)&magicNumber, sizeof(magicNumber)) < 0) {
        printf("Gagal mengirim magic number.\n");
        CLOSE_SOCKET(socket);
        return -1;
    }
    CLOSE_SOCKET(socket);
    return 0;
}

int receiveFile(SOCKET socket, bool btimeout) {
    // Receive Saved File name
    unsigned char* receivedData = NULL;
    char savefilename[128] = "";
    if (receiveDataWithChecksum(socket, &receivedData, btimeout) < 0) {
        printf("Gagal menerima data file.\n");
        if (receivedData) free(receivedData); receivedData = NULL;
        CLOSE_SOCKET(socket);
        return -1;
    }

    memcpy(savefilename, receivedData, strlen(receivedData) >= sizeof(savefilename) ? sizeof(savefilename) : strlen(receivedData) + 1);
    savefilename[sizeof(savefilename) - 1] = 0;
    FILE* fp = fopen(savefilename, "wb");
    if (!fp) {
        printf("Gagal membuka file untuk ditulis: %s\n", receivedData);
        if (receivedData) free(receivedData); receivedData = NULL;
        CLOSE_SOCKET(socket);
        return -2;
    }

    if (receivedData) free(receivedData); receivedData = NULL;
    if (receiveDataWithChecksum(socket, &receivedData, btimeout) < 0) {
        printf("Gagal menerima data file.\n");
        if (receivedData) free(receivedData); receivedData = NULL;
        fclose(fp);
        CLOSE_SOCKET(socket);
        return -1;
    }

    __int64 filesize = *((__int64*)(receivedData + strlen(receivedData) + 1));
    int totalBytes = 0;
    int intervalBytes = 0;
    double speedKBps = 0.0;
    ULONGLONG printStart = GetTickCount64(); // Waktu awal dalam milidetik
    printf("\x1b[1;32mFile Path \t: %s \nSave to \t: %s \nFileSize \t: %lld bytes\x1b[0m\n", receivedData, savefilename, filesize);
    if (receivedData) free(receivedData); receivedData = NULL;

    while (1) {
        unsigned char* receivedData = NULL;
        // receiveDataWithChecksum() harus dimodifikasi agar mengembalikan jumlah byte yang diterima.
        int dataSize = receiveDataWithChecksum(socket, &receivedData, btimeout);
        if (dataSize < 0) {
            printf("Gagal menerima data file.\n");
            if (receivedData) free(receivedData); receivedData = NULL;
            fclose(fp);
            remove(savefilename);
            CLOSE_SOCKET(socket);
            return -1;
        }

        // Cek apakah data yang diterima adalah magic number sebagai tanda akhir transfer
        if (dataSize == sizeof(uint32_t)) {
            uint32_t receivedMagic;
            memcpy(&receivedMagic, receivedData, sizeof(uint32_t));
            if (receivedMagic == FILE_MAGIC) {
                if (receivedData) free(receivedData); receivedData = NULL;
                break;
            }
        }

        // Tulis data yang diterima ke file
        if (fwrite(receivedData, 1, dataSize, fp) != dataSize) {
            printf("Gagal menulis data ke file.\n");
            if (receivedData) free(receivedData); receivedData = NULL;
            fclose(fp);
            remove(savefilename);
            CLOSE_SOCKET(socket);
            return -2;
        }
        totalBytes += dataSize;
        intervalBytes += dataSize;
        if (receivedData) free(receivedData); receivedData = NULL;

        // Cek apakah sudah 1 detik berlalu menggunakan GetTickCount64()
        ULONGLONG currentTime = GetTickCount64();
        ULONGLONG elapsed_ms = currentTime - printStart;
        if (elapsed_ms >= 1000) {
            speedKBps = (double)intervalBytes / (elapsed_ms / 1000.0) / 1024.0; // KB/s
            printDownloadProgress(speedKBps, totalBytes, filesize);
            // Reset waktu dan byte interval untuk iterasi berikutnya
            printStart = currentTime;
            intervalBytes = 0;
        }
    }
    printDownloadProgress(speedKBps, totalBytes, filesize);
    printf("  ");
    fclose(fp);
    CLOSE_SOCKET(socket);
    return 0;
}