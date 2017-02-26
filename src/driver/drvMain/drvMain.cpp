#include "drvUtils.h"
#include "drvUnhookerDevice.h"

static 
NTSTATUS RootDispatch(
                        IN  PDEVICE_OBJECT  DeviceObject,
						IN  PIRP            Irp
                      )
{
    return drv::RootDispatch(DeviceObject, Irp);
}

                            
// DriverEntry
extern "C"
NTSTATUS
DriverEntry(IN PDRIVER_OBJECT   pDriverObject,
            IN PUNICODE_STRING  RegistryPath
)
{
    drv::CLibCpp libCpp;


    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) 
    {
        pDriverObject->MajorFunction[i] = RootDispatch;
    }

    drv::CDeviceOwner deviceObject;
    NT_CHECK(drv::CreateUnhookerDevice(pDriverObject, deviceObject));


    pDriverObject->DriverUnload = 0;
    pDriverObject->FastIoDispatch = NULL;

    deviceObject.release();
    libCpp.release();
    return STATUS_SUCCESS;
}



