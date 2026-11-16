/******************************************************************************
 * myClient.c
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

#include "networks.h"
#include "pdu.h"
#include "pollLib.h"
#include "safeUtil.h"

#define MAXBUF 1500
#define DEBUG_FLAG 1

void clientControl(int socketNum);
void processStdin(int socketNum);
void processMsgFromServer(int socketNum);
void sendMsgPacket(int socketNum, char *line);
void sendExitPacket(int socketNum);
void displayMessage(uint8_t *pdu, int pduLen);
void displayError(uint8_t *pdu, int pduLen);
void checkArgs(int argc, char *argv[]);
void sendFlag1(int socketNum);
void processFlag11(int socketNum, uint8_t *pdu, int pduLen);
void processFlag12(uint8_t *pdu, int pduLen);
void sendListRequest(int socketNum);
void sendMulticastPacket(int socketNum, char *line);
void sendBroadcastPacket(int socketNum, char *line);
void displayBroadcast(uint8_t *pdu, int pduLen);

int readFromStdin(uint8_t *buffer);

// client handle
static char handle[101];

int main(int argc, char *argv[]) {
  int socketNum = 0;
  checkArgs(argc, argv);

  strncpy(handle, argv[1], 100);
  handle[100] = '\0';

  // arg 1 - handle, arg 2 - server name, arg 3 - server port
  socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);

  // register handle with server - send flag 1 packet
  sendFlag1(socketNum);
  clientControl(socketNum);

  close(socketNum);
  return 0;
}

void sendFlag1(int socketNum) {
  uint8_t pdu[MAXBUF];
  int pduLen = 0;
  uint8_t handleLen = strlen(handle);
  uint8_t response[MAXBUF];
  int recvLen = 0;
  uint8_t flag = 0;

  // flag 1 packet - flag 1, handle len, handle chars
  pdu[0] = 1;
  pdu[1] = handleLen;
  memcpy(pdu + 2, handle, handleLen);
  pduLen = handleLen + 2;

  sendPDU(socketNum, pdu, pduLen);

  // block n wait for flag from server 2-ok or flag 3-error
  recvLen = recvPDU(socketNum, response, MAXBUF);
  if (recvLen == 0) {
    printf("Server terminated during registration\n");
    exit(-1);
  }

  flag = response[0];

  if (flag == 3) {
    printf("Handle already in use: %s\n", handle);
    exit(-1);
  } else if (flag != 2) {
    printf("unexpected flag during registration: %d\n", flag);
    exit(-1);
  }
  // flag 2 ok - go thru main loop
}

void clientControl(int socketNum) {
  int readySocket = 0;

  setupPollSet();
  addToPollSet(STDIN_FILENO);
  addToPollSet(socketNum);

  printf("$: ");
  fflush(stdout);

  while (1) {
    readySocket = pollCall(-1);

    if (readySocket == STDIN_FILENO) {
      // user typed command
      processStdin(socketNum);
    } else if (readySocket == socketNum) {
      // msg from server
      processMsgFromServer(socketNum);
    }
  }
}

void processStdin(int socketNum) {
  char line[MAXBUF];

  // read what user typed on stdin
  if (fgets(line, MAXBUF, stdin) == NULL) {
    // EOF - quit command
    exit(0);
  }

  // remove newline
  int len = strlen(line);
  if (len > 0 && line[len - 1] == '\n') {
    line[len - 1] = '\0';
  }

  if (line[0] == '%' && (line[1] == 'M' || line[1] == 'm')) {
    sendMsgPacket(socketNum, line);
  } else if (line[0] == '%' && (line[1] == 'E' || line[1] == 'e')) {
    sendExitPacket(socketNum);
    return; // in case exitpacket doesnt work
  } else if (line[0] == '%' && (line[1] == 'L' || line[1] == 'l')) {
    sendListRequest(socketNum);
  } else if (line[0] == '%' && (line[1] == 'C' || line[1] == 'c')) {
    sendMulticastPacket(socketNum, line);
  } else if (line[0] == '%' && (line[1] == 'B' || line[1] == 'b')) {
    sendBroadcastPacket(socketNum, line);
  } else {
    printf("Invalid command %s\n", line);
  }

  printf("$: ");
  fflush(stdout);
}

void processFlag11(int socketNum, uint8_t *pdu, int pduLen) {
    uint32_t numHandles = 0;

    // parse handle count after flag
    memcpy(&numHandles, pdu + 1, 4);
    numHandles = ntohl(numHandles);

    printf("number of clients: %d\n", numHandles);
}

void processFlag12(uint8_t *pdu, int pduLen) {
    uint8_t handleLen = 0;
    char handleName[101];

    // parse handle len and name
    handleLen = pdu[1];
    memcpy(handleName, pdu + 2, handleLen);
    handleName[handleLen] = '\0';

    printf("  %s\n", handleName);
}

void sendMsgPacket(int socketNum, char *line) {
  char destHandle[101];
  uint8_t pdu[MAXBUF];
  int pduOffset = 0;
  uint8_t srcLeng = strlen(handle);
  uint8_t dstLen = 0;
  char *ptr = line + 2; // skip %M or %m
  char *textStart = NULL;
  int textLen = 0;
  int textOffset = 0;
  int chunkSize = 0;

  //skip spaces between %m and dest handle
  while (*ptr == ' ') {
    ptr++;
  }

  //read dest handle
  int i = 0;
  while (*ptr != ' ' && *ptr != '\0') {
    destHandle[i++] = *ptr++;
  }
  destHandle[i] = '\0';
  dstLen = i;

  if(dstLen == 0) {
    printf("usage: %%M destination handle [text]\n");
    return;
  }

  //skip spaces between dest handle and text
  while (*ptr == ' ') {
    ptr++;
  }

  //ptr points to text
  textStart = ptr;
  textLen = strlen(textStart);

  //send 1 packet even if empty
  textOffset = 0;
  do{
    chunkSize = textLen - textOffset;
    if (chunkSize > 199) chunkSize = 199; // max text that fits in 1 packet - header and handle {

    pduOffset = 0;
    pdu[pduOffset++] = 5; // flag
    pdu[pduOffset++] = srcLeng; // src handle len
    memcpy(pdu + pduOffset, handle, srcLeng); // src handle
    pduOffset += srcLeng;
    pdu[pduOffset++] = 1; // num dest handles - always 1 for %M
    pdu[pduOffset++] = dstLen; // dest handle len
    memcpy(pdu + pduOffset, destHandle, dstLen);   // dst handle chars - was missing
    pduOffset += dstLen;
    memcpy(pdu + pduOffset, textStart + textOffset, chunkSize); // text
    pduOffset += chunkSize;
    pdu[pduOffset++] = '\0'; // null terminate text for safety

    sendPDU(socketNum, pdu, pduOffset);
    textOffset += chunkSize;
    if(textLen == 0) break; // no text, send 1 packet with header and handle
  }
  while(textOffset < textLen);
}

void sendExitPacket(int socketNum) {
  uint8_t pdu[1];
  uint8_t response[MAXBUF];
  int recvLen = 0;

  //send flag 8 - exit
  pdu[0] = 8; 
  sendPDU(socketNum, pdu, 1);

  //wait for flag 9 - confirm
  recvLen = recvPDU(socketNum, response, MAXBUF);
  if (recvLen == 0 || response[0] == 9) {
    exit(0);
  }
}

void sendListRequest(int socketNum) {
    uint8_t pdu[1];

    // flag 10 - ask server for handle list
    pdu[0] = 10;
    sendPDU(socketNum, pdu, 1);
}

void processMsgFromServer(int socketNum) {
  uint8_t pdu[MAXBUF];
  uint8_t flag = 0;
  int pduLen = 0;

  pduLen = recvPDU(socketNum, pdu, MAXBUF);

  if (pduLen == 0) {
    printf("Server Terminated\n");
    exit(0);
  }

  flag = pdu[0];

  if(flag == 4) {
    displayBroadcast(pdu, pduLen);
  } else if (flag == 5) {
    displayMessage(pdu, pduLen);
  } else if (flag == 6) {
    displayMessage(pdu, pduLen);
  } else if (flag == 7) {
    displayError(pdu, pduLen);
    } else if (flag == 11) {
    processFlag11(socketNum, pdu, pduLen);
  } else if (flag == 12) {
    processFlag12(pdu, pduLen);
  } else if (flag == 13) {
    // list done - reprint prompt
    printf("$: ");
    fflush(stdout);
    return; // skip prompt - reprint at the bottom
  } else {
    printf("unknown flag %d from server \n", flag);
  }

  // no $: reprints for flags 11 and 12 - flag 13 handles final prompt
  // stopts duplicate $: when using %L
  if (flag != 11 && flag != 12) {
      printf("$: ");
      fflush(stdout);
  }
}

void displayMessage(uint8_t *pdu, int pduLen) {
  uint8_t srcLen = 0;
  uint8_t dstLen = 0;
  uint8_t numDest = 0;
  int offset = 1; // skip flag
  char srcHandle[101];
  char *text = NULL;

  //parse scr handle
  srcLen = pdu[offset++];
  memcpy(srcHandle, pdu + offset, srcLen);
  srcHandle[srcLen] = '\0';
  offset += srcLen;

  //skip dest handles - not needed for display
  numDest = pdu[offset++];
  int i = 0;
  for (i = 0; i < numDest; i++) {
    dstLen = pdu[offset++];
    offset += dstLen;
  }

  text = (char *)(pdu + offset);
  printf("\n%s: %s\n", srcHandle, text);
}

void displayError(uint8_t *pdu, int pduLen) {
  uint8_t handleLen = 0;
  int offset = 1; // skip flag
  char handle[101];

  handleLen = pdu[offset++];
  memcpy(handle, pdu + offset, handleLen);
  handle[handleLen] = '\0';

  printf("Client with handle %s does not exist\n", handle);
}

void sendMulticastPacket(int socketNum, char *line) {
  uint8_t pdu[MAXBUF];
  uint8_t srcLen = strlen(handle);
  uint8_t destLens[9];
  char *ptr = line + 2; // skip %C or %c
  char *textStart = NULL;
  char destHandles[9][101];
  int textLen = 0;
  int textOffset = 0;
  int chunkSize = 0;
  //int numsDestOffset = 0;
  int numDest = 0;
  int pduOffset = 0;

  //skip spaces between %C and dest handle
  while (*ptr == ' ') {
    ptr++;
  }

  //read dests
  numDest = atoi(ptr);

  //skip past num
  while (*ptr != ' ' && *ptr != '\0'){
    ptr++;
  }
  
  if(numDest < 2 || numDest > 9) {
    printf("usage: %%C num-hanldes (2-9) handel1 handle2 ... [text]\n");
    return;
  }

  //read dest handles
  int i = 0;
  for(i = 0; i < numDest; i++) {
    //skip spaces between dest handles
    while (*ptr == ' ') {
      ptr++;
    }
    //read handle
    int j = 0;
    while (*ptr != ' ' && *ptr != '\0' && j < 100) {
      destHandles[i][j++] = *ptr++;
    }
    destHandles[i][j] = '\0';
    destLens[i] = j;

    if(destLens[i] == 0) {
      printf("not enuogh destination hanldes provided");
      return;
    }
  }

  //skip spaces to get to text
  while (*ptr == ' ') {
    ptr++;
  }
  textStart = ptr;
  textLen = strlen(textStart);

  //send 1 packet even if empty
  textOffset = 0;
  do{
    chunkSize = textLen - textOffset;
    if (chunkSize > 199) chunkSize = 199; // max text for 1 packet - header and handles

    pduOffset = 0;
    pdu[pduOffset++] = 6; // flag
    pdu[pduOffset++] = srcLen; // src handle len
    memcpy(pdu + pduOffset, handle, srcLen); // src handle
    pduOffset += srcLen;
    pdu[pduOffset++] = numDest; // num dest handles

    //write each dest handle
    for(i = 0; i < numDest; i++) {
      pdu[pduOffset++] = destLens[i]; // dest handle len
      memcpy(pdu + pduOffset, destHandles[i], destLens[i]);   // dst handle chars
      pduOffset += destLens[i];
    }

    memcpy(pdu + pduOffset, textStart + textOffset, chunkSize); // text
    pduOffset += chunkSize;
    pdu[pduOffset++] = '\0'; // null terminate text for safety

    sendPDU(socketNum, pdu, pduOffset);

    // update offset for next chunk
    textOffset += chunkSize;
    if(textLen == 0) break; // no text, send 1 packet with header and handles
  }
  while(textOffset < textLen);
}

void sendBroadcastPacket(int socketNum, char *line) {
    uint8_t pdu[MAXBUF];
    uint8_t srcLen = strlen(handle);
    char *ptr = line + 2; // skip %B or %b
    char *textStart = NULL;
    int textLen = 0;
    int textOffset = 0;
    int chunkSize = 0;
    int pduOffset = 0;

    // skip spaces between %B and text
    while (*ptr == ' ') ptr++;

    textStart = ptr;
    textLen = strlen(textStart);

    // send 1 packet even if empty
    textOffset = 0;
    do {
        chunkSize = textLen - textOffset;
        if (chunkSize > 199) chunkSize = 199;

        pduOffset = 0;
        pdu[pduOffset++] = 4;    // flag 
        pdu[pduOffset++] = srcLen;      // src handle len
        memcpy(pdu + pduOffset, handle, srcLen);  // src handle
        pduOffset += srcLen;  
        memcpy(pdu + pduOffset, textStart + textOffset, chunkSize); // text
        pduOffset += chunkSize;
        pdu[pduOffset++] = '\0';  /// null terminate text for safety

        sendPDU(socketNum, pdu, pduOffset);

        // update offset for next chunk
        textOffset += chunkSize;
        if (textLen == 0) break;
    } while (textOffset < textLen);
}

void displayBroadcast(uint8_t *pdu, int pduLen) {
    uint8_t srcLen = 0;
    int offset = 1; // skip flag
    char srcHandle[101];
    char *text = NULL;

    // parse src handle
    srcLen = pdu[offset++];
    memcpy(srcHandle, pdu + offset, srcLen);  // src handle
    srcHandle[srcLen] = '\0';
    offset += srcLen;

    // no dest handles in broadcast - skip straight to text
    text = (char *)(pdu + offset);
    printf("\n%s: %s\n", srcHandle, text);
}

int readFromStdin(uint8_t *buffer) {
  char aChar = 0;
  int inputLen = 0;

  // Important you don't input more characters than you have space
  buffer[0] = '\0';
  while (inputLen < (MAXBUF - 1) && aChar != '\n') {
    aChar = getchar();
    if (aChar != '\n') {
      buffer[inputLen] = aChar;
      inputLen++;
    }
  }

  // Null terminate the string
  buffer[inputLen] = '\0';
  inputLen++;

  return inputLen;
}

void checkArgs(int argc, char *argv[]) {
  // check command line args
  if (argc != 4) {
    printf("usage: cclient handle server-name server-port\n");
    exit(1);
  }
  if (strlen(argv[1]) > 100) {
    printf("Invalid handle, handle longer than 100 characters: %s\n", argv[1]);
    exit(1);
  }
}
