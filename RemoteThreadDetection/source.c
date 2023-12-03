#include <ntddk.h>
#include <ntstrsafe.h>
#include "ProcessLinkedList.h"

DRIVER_INITIALIZE DriverEntry;

DRIVER_UNLOAD RTDetectorUnload;
DRIVER_DISPATCH RTDetectorCreateClose;
NTSTATUS OnMessage;

VOID ThreadCreateNotifyRoutine(
    _In_ HANDLE ProcessId,
    _In_ HANDLE ThreadId,
    _In_ BOOLEAN Create
);

VOID ProcessCreateNotifyRoutine(
    _In_ HANDLE ParentId,
    _In_ HANDLE ProcessId,
    _In_ BOOLEAN Create
);

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING registry) {
    UNREFERENCED_PARAMETER(registry);

    NTSTATUS status = STATUS_SUCCESS;

    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\RTDetector");
    UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\RTDetector");
    PDEVICE_OBJECT DeviceObject = NULL;

    status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = IoCreateSymbolicLink(&symName, &deviceName);

    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(DeviceObject);
        return status;
    }

    // Register thread creation callback
    status = PsSetCreateThreadNotifyRoutine(ThreadCreateNotifyRoutine);

    if (!NT_SUCCESS(status)) {
        IoDeleteSymbolicLink(&symName);
        IoDeleteDevice(DeviceObject);
        return status;
    }

    // Register process creation callback
    status = PsSetCreateProcessNotifyRoutine(ProcessCreateNotifyRoutine, FALSE);

    if (!NT_SUCCESS(status)) {
        PsRemoveCreateThreadNotifyRoutine(ThreadCreateNotifyRoutine);
        IoDeleteSymbolicLink(&symName);
        IoDeleteDevice(DeviceObject);
        return status;
    }

    DriverObject->DriverUnload = RTDetectorUnload;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = OnMessage;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = RTDetectorCreateClose;

    InitializeProcessList();

    return STATUS_SUCCESS;
}

NTSTATUS RTDetectorCreateClose(PDEVICE_OBJECT pob, PIRP Irp) {
    UNREFERENCED_PARAMETER(pob);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS OnMessage(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PCHAR welcome = "IOCTLKmUm - Hello from kernel!";
    PVOID pBuf = Irp->AssociatedIrp.SystemBuffer;
    PIO_STACK_LOCATION pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);

    if (pIoStackLocation->Parameters.DeviceIoControl.IoControlCode == 0x800) {
        DbgPrint("IOCTLKmUm - Received: %s\n", pBuf);
        RtlZeroMemory(pBuf, pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength);
        RtlCopyMemory(pBuf, welcome, strlen(welcome));
        Irp->IoStatus.Information = strlen(welcome);
    }
    else {
        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

VOID ThreadCreateNotifyRoutine(
    _In_ HANDLE ProcessId,
    _In_ HANDLE ThreadId,
    _In_ BOOLEAN Create
)
{
    UNREFERENCED_PARAMETER(ThreadId);

    if (Create)
    {
        if (IsNewProcess(ProcessId))
        {
            RemoveProcess(ProcessId);
            return;
        }
        if (HandleToLong(ProcessId) != 4)
        {
            // This code is running in the context of the host of the created thread
            HANDLE hostPid = PsGetCurrentProcessId();
            // Comparing the host id with the creator id
            if (hostPid != ProcessId)
            {
                KdPrint(("Remote thread has been created!\n"));

            }
        }
    }
}

VOID ProcessCreateNotifyRoutine(
    _In_ HANDLE ParentId,
    _In_ HANDLE ProcessId,
    _In_ BOOLEAN Create
)
{
    UNREFERENCED_PARAMETER(ParentId);
    UNREFERENCED_PARAMETER(ProcessId);

    if (Create)
    {
        AddProcess(ProcessId);
    }
}

void RTDetectorUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\RTDetector");

    // Unregister create thread callback
    PsRemoveCreateThreadNotifyRoutine(ThreadCreateNotifyRoutine);
    // Unregister create process callback
    PsSetCreateProcessNotifyRoutine(ProcessCreateNotifyRoutine, TRUE);

    // Delete the symbolic link and device object
    IoDeleteSymbolicLink(&symName);
    if (DriverObject->DeviceObject)
        IoDeleteDevice(DriverObject->DeviceObject);

    KdPrint(("RTDetectorUnload: Unloaded\n"));
}
