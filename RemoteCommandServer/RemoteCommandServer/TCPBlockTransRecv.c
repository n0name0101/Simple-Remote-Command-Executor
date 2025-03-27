#include "TCPBlockTransRecv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Fungsi untuk menghitung checksum (sama dengan pada pengirim)
unsigned int calculateChecksum(const unsigned char* buffer, size_t data_size) {
    unsigned int checksum = 0;
    for (size_t i = 0; i < data_size; i++) {
        checksum += buffer[i];
    }
    return checksum;
}

// Fungsi untuk mengirim data per blok dengan header dan pengecekan ACK
int sendDataWithChecksum(SOCKET socket, const unsigned char* buffer, size_t data_size) {
    size_t offset = 0;
    int result;
    int retries;
    char ackBuffer[ACK_SIZE + 1];
    uint32_t block_seq = 0;

    // Set Time Out
    int timeout = 5000; // Timeout dalam milidetik
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    while (offset < data_size) {
        // Tentukan ukuran blok saat ini
        size_t current_block_size = ((data_size - offset) < BLOCK_SIZE) ? (data_size - offset) : BLOCK_SIZE;
        unsigned int checksum = calculateChecksum(buffer + offset, current_block_size);

        // Persiapkan header
        BlockHeader header;
        header.magic = htonl(MAGIC);
        header.block_seq = htonl(block_seq);
        header.block_size = htonl((uint32_t)current_block_size);
        header.checksum = htonl(checksum);

        retries = 0;
        while (retries < MAX_RETRIES) {
            //printf("Tekan apa saja untuk kirim Header Block...\n");
            //getchar();

            // Kirim header
            result = send(socket, (const char*)&header, sizeof(BlockHeader), 0);
            if (result == SOCKET_ERROR) {
                printf("Gagal mengirim header. Error: %d\n", WSAGetLastError());
                // Set Time Out
                int timeout = 0; // Timeout dalam milidetik
                setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
                return -1;
            }
            // Kirim data blok
            result = send(socket, (const char*)(buffer + offset), (int)current_block_size, 0);
            if (result == SOCKET_ERROR) {
                printf("Gagal mengirim data blok. Error: %d\n", WSAGetLastError());
                // Set Time Out
                int timeout = 0; // Timeout dalam milidetik
                setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
                return -1;
            }
            // Tunggu ACK dari penerima
            result = recv(socket, ackBuffer, ACK_SIZE, 0);
            if (result == SOCKET_ERROR) {
                if (WSAGetLastError() == WSAETIMEDOUT) {
                    printf("(TimeOut) Gagal menerima ACK. Error: %d, mencoba kirim ulang...\n", WSAGetLastError());
                    retries++;
                    continue;
                }
                else {
                    printf("recv() gagal dengan error: %d\n", WSAGetLastError());
                    // Set Time Out
                    int timeout = 0; // Timeout dalam milidetik
                    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
                    return -1;
                }
            }
            ackBuffer[result] = '\0';
            if (strcmp(ackBuffer, ACK) == 0) {
                // Blok terkirim dengan benar
                break;
            }
            else {
                // ACK tidak sesuai, ulang pengiriman blok
                printf("ACK tidak diterima untuk blok %u, mencoba kirim ulang...\n", block_seq);
                retries++;
            }
        }
        if (retries == MAX_RETRIES) {
            printf("Terjadi error pada blok %u. Transfer dibatalkan.\n", block_seq);
            // Set Time Out
            int timeout = 0; // Timeout dalam milidetik
            setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
            return -1;
        }
        offset += current_block_size;
        block_seq++;
    }

    //Kirim Header Sebagai Tanda Ending
    BlockHeader header;
    header.magic = htonl(MAGIC);
    header.block_seq = htonl(0);
    header.block_size = htonl(0);
    header.checksum = htonl(0);
    // Kirim header
    result = send(socket, (const char*)&header, sizeof(BlockHeader), 0);
    if (result == SOCKET_ERROR) {
        printf("Gagal mengirim header. Error: %d\n", WSAGetLastError());
        // Set Time Out
        int timeout = 0; // Timeout dalam milidetik
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        return -1;
    }

    timeout = 0;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    return 0;
}

// Fungsi untuk mengirim data per blok dengan header dan pengecekan ACK
int receiveDataWithChecksum(SOCKET socket, unsigned char** outputBuffer, bool btimeout) {
    int iReceivedBLockSeq = 0;
    int receivedBytes = 0;  // Menyimpan jumlah total data yang diterima
    BlockHeader header = { 1 , 1 , 1 };
    while (header.block_size != 0) {
        // Terima header dengan mekanisme re-synchronization
        if (receiveHeader(socket, &header, btimeout) < 0) {
            printf("Gagal menerima header atau koneksi ditutup.\n");
            return -1;
        }

        // Alokasikan buffer sesuai ukuran blok yang diberikan dalam header
        uint32_t blockSize = header.block_size;
        unsigned char* block_buffer = (unsigned char*)malloc(blockSize);
        if (!block_buffer) {
            printf("Gagal mengalokasikan memori untuk blok.\n");
            return -1;
        }

        // Terima data blok
        if (recvAll(socket, (char*)block_buffer, blockSize, btimeout) < 0) {
            printf("Gagal menerima data blok.\n");
            free(block_buffer);
            return -1;
        }

        // Hitung checksum data yang diterima
        unsigned int computed_checksum = calculateChecksum(block_buffer, blockSize);
        if (computed_checksum != header.checksum) {
            printf("Error: Checksum blok %u tidak sesuai. Transfer dihentikan.\n", header.block_seq);
            // Kirim NACK sebagai notifikasi error (opsional)
            send(socket, "NACK", 4, 0);
            free(block_buffer);
            return -1;
        }
        else {
            // Proses data yang diterima (misalnya, tampilkan informasi blok)
            if (header.block_seq == iReceivedBLockSeq) {
                // Tambahkan data ke buffer utama
                unsigned char* temp = (unsigned char*)realloc(*outputBuffer, receivedBytes + blockSize);
                if (!temp) {
                    printf("Gagal memperbesar buffer.\n");
                    free(block_buffer);
                    free(*outputBuffer);
                    return -1;
                }
                *outputBuffer = temp;
                memcpy(*outputBuffer + receivedBytes, block_buffer, blockSize);
                receivedBytes += blockSize;
                //printf("Blok %u diterima (%u byte)\n", header.block_seq, blockSize, blockSize/*, block_buffer*/);
                iReceivedBLockSeq++;
                //printf("Tekan apa saja untuk kirim ACK...\n");
                //getchar();
                // Checksum valid, kirim ACK ke pengirim
                send(socket, ACK, ACK_SIZE, 0);
            }
        }
        free(block_buffer);
    }
    return receivedBytes;
}

// Fungsi untuk menerima sejumlah byte yang diharapkan
// Mengembalikan jumlah byte yang berhasil diterima atau -1 jika gagal.
int recvAll(SOCKET sock, char* buffer, int length, bool btimeout) {
    int total = 0;
    int bytesReceived;
    int retries = 0;
    // Set Time Out
    int timeout = btimeout ? 5000 : 0; // Timeout dalam milidetik
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    while (total < length && retries < MAX_RETRIES) {
        bytesReceived = recv(sock, buffer + total, length - total, 0);
        if (bytesReceived == 0) {
            // Set Time Out
            timeout = 0; // Timeout dalam milidetik
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
            return -1;
        }
        else if (bytesReceived == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                printf("(TimeOut) Gagal menerima data. Error: %d, mencoba terima ulang...\n", WSAGetLastError());
                retries++;
                continue;
            }
            else {
                printf("recv() gagal dengan error: %d\n", WSAGetLastError());
                // Set Time Out
                timeout = 0; // Timeout dalam milidetik
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
                return -1;
            }
        }
        total += bytesReceived;
        retries = 0;
    }
    // Set Time Out
    timeout = 0; // Timeout dalam milidetik
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    return total;
}

// Fungsi untuk melakukan re-synchronization agar selalu membaca header yang valid.
// Jika data yang diterima tidak diawali header (berdasarkan MAGIC), lakukan sliding window untuk menemukan header.
int receiveHeader(SOCKET sock, BlockHeader* header, bool btimeout) {
    unsigned char buffer[sizeof(BlockHeader)];

    // Ambil data untuk header
    if (recvAll(sock, (char*)buffer, sizeof(BlockHeader), btimeout) <= 0)
        return -1;

    // Periksa magic field
    uint32_t magic = ntohl(*(uint32_t*)buffer);
    while (magic != MAGIC) {
        // Jika tidak sama, geser buffer 1 byte ke kiri dan ambil 1 byte lagi
        memmove(buffer, buffer + 1, sizeof(buffer) - 1);
        if (recv(sock, (char*)&buffer[sizeof(buffer) - 1], 1, 0) <= 0)
            return -1;
        magic = ntohl(*(uint32_t*)buffer);
    }

    // Setelah ditemukan header yang valid, salin ke struct header
    header->magic = ntohl(*(uint32_t*)&buffer[0]);
    header->block_seq = ntohl(*(uint32_t*)&buffer[4]);
    header->block_size = ntohl(*(uint32_t*)&buffer[8]);
    header->checksum = ntohl(*(uint32_t*)&buffer[12]);
    return 0;
}