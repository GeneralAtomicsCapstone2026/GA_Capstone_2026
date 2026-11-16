// pdu.c
// send n recv PDUs
// PDU format: [2-byte total length in network order] [payload]

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdu.h"
#include "safeUtil.h"

int sendPDU(int socketNumber, uint8_t *dataBuffer, int lengthOfData) {
  // pdu size - header n payload
  int pduLen = lengthOfData + 2;
  // buffer - hold full pdu
  uint8_t pdu[pduLen];
  // convert length to network byte order
  uint16_t networkLen = htons(pduLen);

  // copy 2 byte length to front of pdu
  memcpy(pdu, &networkLen, 2);
  // copy payload to pdu after header
  memcpy(pdu + 2, dataBuffer, lengthOfData);

  // send full pdu in one send call
  safeSend(socketNumber, pdu, pduLen, 0);
  // return data bytes num sent - excludes header
  return lengthOfData;
}

int recvPDU(int socketNumber, uint8_t *dataBuffer, int bufferSize) {
  uint16_t networkLen = 0;
  int payloadLen = 0;
  int recvBytes = 0;

  // first recv - get 2 byte length header
  recvBytes = safeRecv(socketNumber, (uint8_t *)&networkLen, 2, MSG_WAITALL);

  // other side closed connection
  if (recvBytes == 0) {
    return 0;
  }

  // convert length from network byte order to host byte order
  // -2 since length has header
  payloadLen = ntohs(networkLen) - 2;

  // make sure payload fits in buffer
  if (payloadLen > bufferSize) {
    printf("Error: PDU too big for buffer\n");
    exit(-1);
  }

  // second recv - get payload data
  recvBytes = safeRecv(socketNumber, dataBuffer, payloadLen, MSG_WAITALL);

  // return payload bytes num received
  return recvBytes;
}