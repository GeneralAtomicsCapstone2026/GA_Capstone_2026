// handleTable.h
// maps handle names to socket numbers

#ifndef HANDLE_TABLE_H
#define HANDLE_TABLE_H
void initHandleTable();

// add handle n socket
// -1 if handle exists
int addHandle(char *handle, int socketNum);

// -1 if not found
int lookupHandle(char *handle);

// -1 if not found
int lookupSocket(int socketNum, char *outHandle);

// remove entry by socket number
void removeHandleBySocket(int socketNum);

// num handles
int getNumHandles();

// handle name @ index
int getHandleAtIndex(int index, char *outHandle);

#endif