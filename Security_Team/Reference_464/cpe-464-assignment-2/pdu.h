#ifndef PDU_H
#define PDU_H

#include <stdint.h>

// send app PDU 2 byte header n payload
int sendPDU(int socketNumber, uint8_t *dataBuffer, int lengthOfData);

// receive app PDU - strip header n return payload bytes
int recvPDU(int socketNumber, uint8_t *dataBuffer, int bufferSize);

#endif