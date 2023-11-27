#ifndef PROCESSLINKEDLIST_H
#define PROCESSLINKEDLIST_H

#include <ntddk.h>

typedef struct NewProc {
    HANDLE pid;
    LIST_ENTRY listEntry;
} NewProc, * PNewProc;

VOID InitializeProcessList();
NTSTATUS AddProcess(HANDLE pid);
BOOLEAN IsNewProcess(HANDLE pid);
NTSTATUS RemoveProcess(HANDLE pid);

#endif