#include <ntddk.h>
#include "ProcessLinkedList.h"

DRIVER_INITIALIZE DriverEntry;

DRIVER_UNLOAD RTDetectorUnload;
DRIVER_DISPATCH RTDetectorCreateClose;

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

// IOCTL code definition
#define IOCTL_SEND_DATA CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA)

// Structure for data communication
typedef struct _DATA_TRANSFER {
    HANDLE ProcessId; // You can add more data members as needed
} DATA_TRANSFER, * PDATA_TRANSFER;


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

NTSTATUS SendIoctlToUserMode(PDATA_TRANSFER pData) {
    UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\RTDetector");
    PFILE_OBJECT pFileObject = NULL;
    PDEVICE_OBJECT pDeviceObject = NULL;

    // Open the device object for communication
    if (!NT_SUCCESS(IoGetDeviceObjectPointer(&devName, FILE_READ_DATA, &pFileObject, &pDeviceObject)))
    {
        return STATUS_UNSUCCESSFUL;
    }

    // Send IOCTL request to user-mode program
    IO_STATUS_BLOCK ioStatus = { 0 };
    KEVENT event;
    PIRP irp;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(IOCTL_SEND_DATA,
        pDeviceObject,
        pData,
        sizeof(DATA_TRANSFER),
        NULL,
        0,
        FALSE,
        &event,
        &ioStatus);

    if (irp == NULL)
    {
        ObDereferenceObject(pFileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NTSTATUS status = IoCallDriver(pDeviceObject, irp);

    if (status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }

    // Clean up
    ObDereferenceObject(pFileObject);

    return status;
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

                // Communicate with user-mode program
                PDATA_TRANSFER pData = (PDATA_TRANSFER)ExAllocatePool2(NonPagedPool, sizeof(DATA_TRANSFER), 0);

                if (pData != NULL)
                {
                    RtlZeroMemory(pData, sizeof(DATA_TRANSFER));
                    pData->ProcessId = hostPid;

                    NTSTATUS status = SendIoctlToUserMode(pData);

                    if (!NT_SUCCESS(status))
                    {
                        // Handle error
                        KdPrint(("Failed to communicate with user-mode program! Status: 0x%X\n", status));
                    }

                    ExFreePool(pData);
                }
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
