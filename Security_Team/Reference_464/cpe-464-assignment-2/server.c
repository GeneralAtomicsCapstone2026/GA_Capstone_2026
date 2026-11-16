/******************************************************************************
 * myServer.c
 *
 * Writen by Prof. Smith, updated Jan 2023
 * Use at your own risk.
 *
 *****************************************************************************/

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "handleTable.h"
#include "networks.h"
#include "pdu.h"
#include "pollLib.h"
#include "safeUtil.h"

#define MAXBUF 1500
#define DEBUG_FLAG 1

void serverLoop(int mainServerSocket);
void addNewClient(int mainServerSocket);
void processClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
void porcessFlag1(int clientSocket, uint8_t *pdu, int pduLen);
void processFlag4(int clientSocket, uint8_t *pdu, int pduLen);
void processFlag5(int clientSocket, uint8_t *pdu, int pduLen);
void processFlag8(int clientSocket);
void processFlag10(int clientSocket);
void processFlag6(int clientSocket, uint8_t *pdu, int pduLen);


int main(int argc, char *argv[]) {
  int mainServerSocket = 0;

  mainServerSocket = tcpServerSetup(checkArgs(argc, argv));
  initHandleTable();
  serverLoop(mainServerSocket);
  close(mainServerSocket);
  return 0;
}


void serverLoop(int mainServerSocket) {
  //int readySocket = 0;

  setupPollSet();
  addToPollSet(mainServerSocket);

  while (1) {
    // wait forever for a socket to be ready
    int readySocket = pollCall(-1);

    if (readySocket == mainServerSocket) {
      // server socket ready - new client wants to connect
      addNewClient(mainServerSocket);
    } else {
      // client socket ready - data came in
      processClient(readySocket);
    }
  }
}

void addNewClient(int mainServerSocket) {
  // accept the new client connection
  int clientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);

  // add the new client to the poll set
  addToPollSet(clientSocket);
}

void processClient(int clientSocket) {
  uint8_t pdu[MAXBUF];
  uint8_t flag = 0;
  int pduLen = 0;

  pduLen = recvPDU(clientSocket, pdu, MAXBUF);

  if (pduLen == 0) {
    // client disconnected
    printf("socket %d: client disconnected\n", clientSocket);
    removeHandleBySocket(clientSocket);
    removeFromPollSet(clientSocket);
    close(clientSocket);

    return;
  }

  flag = pdu[0];

  if (flag == 1) {
    porcessFlag1(clientSocket, pdu, pduLen);
  }else if (flag == 4){
    processFlag4(clientSocket, pdu, pduLen);
  } else if(flag == 5) {
    processFlag5(clientSocket, pdu, pduLen);
  }else if (flag == 6) {
    processFlag6(clientSocket, pdu, pduLen);
  } else if (flag == 8) {
    processFlag8(clientSocket);
  } else if (flag == 10) {
    processFlag10(clientSocket);
  } else {
    printf("socket %d: unknown flag %d\n", clientSocket, flag);
  }
}

void porcessFlag1(int clientSocket, uint8_t *pdu, int pduLen) {
  uint8_t handleLen = 0;
  char handle[101];
  uint8_t flagBuf[1];

  // pdu[0] - flag 1, [1] - handle len, [2] - handle chars
  handleLen = pdu[1];
  memcpy(handle, pdu + 2, handleLen);
  handle[handleLen] = '\0';

  // add handle -1 if dup
  if (addHandle(handle, clientSocket) == -1) {
    // send error response
    printf("Error: socket %d duplicate handle %s rejecting \n", clientSocket,handle);
    flagBuf[0] = 3;
    sendPDU(clientSocket, flagBuf, 1);
    removeFromPollSet(clientSocket);
    close(clientSocket);
  } else {
    // valid handle - send flag 2
    printf("socket %d: registered handle %s\n", clientSocket, handle);
    flagBuf[0] = 2;
    sendPDU(clientSocket, flagBuf, 1);
  }
}

void processFlag4(int clientSocket, uint8_t *pdu, int pduLen) {
    int total = 0;
    int i = 0;
    int destSocket = 0;
    char handleName[101];

    total = getNumHandles();

    // send message to all handles except src handle
    for (i = 0; i < total; i++) {
        if (getHandleAtIndex(i, handleName) == 0) {
            destSocket = lookupHandle(handleName);
            if (destSocket != clientSocket) {
                sendPDU(destSocket, pdu, pduLen);
            }
        }
    }
}

void processFlag5(int clientSocket, uint8_t *pdu, int pduLen) {
  uint8_t errPkt[MAXBUF];
  uint8_t srcLen = 0;
  uint8_t numDst = 0;
  uint8_t dstLen = 0;
  
  int offset = 1; // skip flag
  int destSocket = 0;
  int errOffset = 0;
  char dstHandle[101];

  //skip scr handle
  srcLen = pdu[offset++];
  offset += srcLen;

  //num of dests - 1 for %M
  numDst = pdu[offset++];

  int i = 0;
  for ( i = 0; i < numDst; i++) {
    //parse dst handle
    dstLen = pdu[offset++];
    memcpy(dstHandle, pdu + offset, dstLen);
    dstHandle[dstLen] = '\0';
    offset += dstLen;

    destSocket = lookupHandle(dstHandle);

    if (destSocket == -1) {
      //handle not found - send flag 7 error back to src
      errOffset = 0;
      errPkt[errOffset++] = 7; // flag 7 
      errPkt[errOffset++] = dstLen; // dst handle len
      memcpy(errPkt + errOffset, dstHandle, dstLen); // dst handle chars
      errOffset += dstLen;
      sendPDU(clientSocket, errPkt, errOffset);
    } else {
      //send message to dest socket - same pdu, diff dest socket
      sendPDU(destSocket, pdu, pduLen);
    }
  }

}


void processFlag6(int clientSocket, uint8_t *pdu, int pduLen) {
    uint8_t errPkt[MAXBUF];
    uint8_t srcLen = 0;
    uint8_t numDst = 0;
    uint8_t dstLen = 0;
    int offset = 1; // skip flag
    int destSocket = 0;
    int errOffset = 0;
    int i = 0;
    char dstHandle[101];

    // skip src handle
    srcLen = pdu[offset++];
    offset += srcLen;

    //num of dests
    numDst = pdu[offset++];
    // for each dest handle, lookup socket and send or send error if not found
    for (i = 0; i < numDst; i++) {
        // parse dest handles
        dstLen = pdu[offset++];
        memcpy(dstHandle, pdu + offset, dstLen);
        dstHandle[dstLen] = '\0';
        offset += dstLen;

        destSocket = lookupHandle(dstHandle);
        // if no dest handle found, send error back to src handle for that dest handle
        if (destSocket == -1) {
            // flag 7 for handle
            errOffset = 0;
            errPkt[errOffset++] = 7;
            errPkt[errOffset++] = dstLen;
            memcpy(errPkt + errOffset, dstHandle, dstLen);
            errOffset += dstLen;
            sendPDU(clientSocket, errPkt, errOffset);
        } else {
            // send message to dest socket - same pdu, diff dest socket
            sendPDU(destSocket, pdu, pduLen);
        }
    }
}

void processFlag8(int clientSocket) {
  uint8_t flagBuf[1];

  //confirm exit with flag 9
  flagBuf[0] = 9;
  sendPDU(clientSocket, flagBuf, 1);

  //remove client from poll set and close socket
  removeHandleBySocket(clientSocket);
  removeFromPollSet(clientSocket);
  close(clientSocket);
}

void processFlag10(int clientSocket) {
  uint8_t pdu[MAXBUF];
  uint8_t handleLen = 0;
  u_int32_t numHandles = 0;
  int pduOffset = 0;
  int total = 0;
  char handleName[101];

  total = getNumHandles();
  numHandles = htonl(total);

  //flag 11 - send count of handles as 4 bit num in net order
  pduOffset = 0;
  pdu[pduOffset++] = 11;
  memcpy(pdu + pduOffset, &numHandles, 4);
  pduOffset += 4;
  sendPDU(clientSocket, pdu, pduOffset);

  //flag 12 - send each handle as one packet
  int i = 0;
  for (i = 0; i < total; i++) {
    if (getHandleAtIndex(i, handleName) == 0) {
      handleLen = strlen(handleName);
      pduOffset = 0;
      pdu[pduOffset++] = 12;
      pdu[pduOffset++] = handleLen;
      memcpy(pdu + pduOffset, handleName, handleLen);
      pduOffset += handleLen;
      sendPDU(clientSocket, pdu, pduOffset);
    }
  }

  // flag 13 - end of handle list
  pduOffset = 0;
  pdu[pduOffset++] = 13;
  sendPDU(clientSocket, pdu, pduOffset);
}

int checkArgs(int argc, char *argv[]) {
  // check args - returns port number
  int portNumber = 0;

  if (argc > 2) {
    fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
    exit(-1);
  }

  if (argc == 2) {
    portNumber = atoi(argv[1]);
  }

  return portNumber;
}
