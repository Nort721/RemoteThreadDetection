#include "ProcessLinkedList.h"

LIST_ENTRY processListHead;

VOID InitializeProcessList()
{
    InitializeListHead(&processListHead);
}

NTSTATUS AddProcess(HANDLE pid)
{
    PNewProc newProc = (PNewProc)ExAllocatePool2(NonPagedPool, sizeof(NewProc), 'Proc');
    if (!newProc)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    newProc->pid = pid;

    // Add to the list
    InsertTailList(&processListHead, &newProc->listEntry);

    return STATUS_SUCCESS;
}

BOOLEAN IsNewProcess(HANDLE pid)
{
    PLIST_ENTRY entry;
    for (entry = processListHead.Flink; entry != &processListHead; entry = entry->Flink)
    {
        PNewProc proc = CONTAINING_RECORD(entry, NewProc, listEntry);

        if (proc->pid == pid)
        {
            return TRUE;
        }
    }

    return FALSE;
}


NTSTATUS RemoveProcess(HANDLE pid)
{
    PLIST_ENTRY entry;
    for (entry = processListHead.Flink; entry != &processListHead; entry = entry->Flink)
    {
        PNewProc proc = CONTAINING_RECORD(entry, NewProc, listEntry);

        if (proc->pid == pid)
        {
            // Remove from the list
            RemoveEntryList(&proc->listEntry);

            // Free the allocated memory
            ExFreePoolWithTag(proc, 'Proc');

            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_FOUND; // Element not found
}