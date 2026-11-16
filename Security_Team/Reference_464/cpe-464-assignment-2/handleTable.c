// handleTable.c
// manages table of connected clients
// map handles to socket

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "handleTable.h"

#define MAX_HANDLE_LEN 101
#define INITIAL_TABLE_SIZE 10

// entry in table - handle name n its socket
typedef struct {
  char handle[MAX_HANDLE_LEN];
  int socketNum;
  int inUse; // 1 - slot has entry, 0 - empty
} HandleEntry;

//  the table
static HandleEntry *table = NULL;
static int tableSize = 0;
static int numHandles = 0;

void initHandleTable() {
  // initial table
  tableSize = INITIAL_TABLE_SIZE;
  table = calloc(tableSize, sizeof(HandleEntry));
  if (table == NULL) {
    printf("error: could not allocate handle table\n");
    exit(-1);
  }
  numHandles = 0;
}

int addHandle(char *handle, int socketNum) {
  int i = 0;

  // handle check - no duplicates
  if (lookupHandle(handle) != -1) {
    return -1;
  }

  // resize table if full
  if (numHandles >= tableSize) {
    tableSize *= 2;
    table = realloc(table, tableSize * sizeof(HandleEntry));
    if (table == NULL) {
      printf("error: could not grow handle table\n");
      exit(-1);
    }
    // zero new entries
    for (i = numHandles; i < tableSize; i++) {
      table[i].inUse = 0;
      table[i].socketNum = 0;
      table[i].handle[0] = '\0';
    }
  }

  // find first open slot for new handle
  for (i = 0; i < tableSize; i++) {
    if (table[i].inUse == 0) {
      strncpy(table[i].handle, handle, MAX_HANDLE_LEN - 1);
      table[i].handle[MAX_HANDLE_LEN - 1] = '\0';
      table[i].socketNum = socketNum;
      table[i].inUse = 1;
      numHandles++;
      return 0;
    }
  }

  // table didnt resize properly
  return -1;
}

int lookupHandle(char *handle) {
  int i = 0;

  // scan for handle match
  for (i = 0; i < tableSize; i++) {
    if (table[i].inUse == 1) {
      if (strncmp(table[i].handle, handle, MAX_HANDLE_LEN) == 0) {
        return table[i].socketNum;
      }
    }
  }

  // not found
  return -1;
}

int lookupSocket(int socketNum, char *outHandle) {
  int i = 0;

  // scan for socket num match
  for (i = 0; i < tableSize; i++) {
    if (table[i].inUse == 1 && table[i].socketNum == socketNum) {
      strncpy(outHandle, table[i].handle, MAX_HANDLE_LEN);
      return 0;
    }
  }

  // not found
  return -1;
}

void removeHandleBySocket(int socketNum) {
  int i = 0;

  // find entry with socket - mark unused
  for (i = 0; i < tableSize; i++) {
    if (table[i].inUse == 1 && table[i].socketNum == socketNum) {
      table[i].inUse = 0;
      table[i].socketNum = 0;
      table[i].handle[0] = '\0';
      numHandles--;
      return;
    }
  }
}

int getNumHandles() { return numHandles; }

int getHandleAtIndex(int index, char *outHandle) {
  int i = 0;
  int count = 0;

  // find nth active entry
  for (i = 0; i < tableSize; i++) {
    if (table[i].inUse == 1) {
      if (count == index) {
        strncpy(outHandle, table[i].handle, MAX_HANDLE_LEN);
        return 0;
      }
      count++;
    }
  }

  // index out of range
  return -1;
}