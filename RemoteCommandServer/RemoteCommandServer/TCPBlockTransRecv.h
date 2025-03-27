#ifndef TCPBLOCKTRANSRECV_H
#define TCPBLOCKTRANSRECV_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <stdbool.h>

// Definisi konstanta dan parameter
#define BLOCK_SIZE       (1024*2)
#define ACK              "ACK"
#define ACK_SIZE         3
#define MAX_RETRIES      5
#define MAGIC            0xABCD1234           // Nilai konstan untuk header

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;       // Nilai magic untuk sinkronisasi
    uint32_t block_seq;   // Nomor urut blok
    uint32_t block_size;  // Ukuran data pada blok
    uint32_t checksum;    // Checksum untuk data blok
} BlockHeader;
#pragma pack(pop)

// Deklarasi fungsi-fungsi
unsigned int calculateChecksum(const unsigned char* buffer, size_t data_size);
int sendDataWithChecksum(SOCKET socket, const unsigned char* buffer, size_t data_size);
int recvAll(SOCKET sock, char* buffer, int length, bool btimeout);
int receiveHeader(SOCKET sock, BlockHeader* header, bool btimeout);
int receiveDataWithChecksum(SOCKET socket, unsigned char** outputBuffer, bool btimeout);

#endif // TCPBLOCKTRANSRECV_H
