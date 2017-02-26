#ifndef DRV_EXTENSIONS_H
#define DRV_EXTENSIONS_H

extern "C"
{
#include "ntddk.h"
}

namespace drv
{


inline NTSTATUS CompleteIrp(IRP * pIrp, NTSTATUS status = STATUS_SUCCESS, ULONG outSize = 0)
{
    pIrp->IoStatus.Status = status;
    pIrp->IoStatus.Information = outSize;
    IoCompleteRequest( pIrp, IO_NO_INCREMENT );
    return status;
}


struct CommonDeviceExtension;
typedef NTSTATUS (*DeviceDispatchProc)(
                                        IN  PDEVICE_OBJECT  DeviceObject,
						                IN  PIRP            Irp,
                                        CommonDeviceExtension * pExtension
                                      );
typedef void (*CleanProc)(CommonDeviceExtension * pExtension);

struct CommonDeviceExtension
{
    DeviceDispatchProc m_deviceDispatchFnc;
    CleanProc m_cleanProc;
};

template <class ArgType>
struct TypeComparer
{
    typedef void * Result_type;
};
template <class ArgType>
struct TypeComparer<NTSTATUS (*)(
                                        IN  PDEVICE_OBJECT  DeviceObject,
						                IN  PIRP            Irp,
                                        ArgType * pExtension
                                      )>
{
    typedef ArgType * Result_type;
};
    
struct TwoChars{ char  buf[2]; };
static char TestFunction(...);
static TwoChars TestFunction(CommonDeviceExtension * pExtension);


template <class DeviceDispatchType, class CleanupFncType>
inline typename TypeComparer<DeviceDispatchType>::Result_type 
InitCommonDeviceExtension(IN  PDEVICE_OBJECT  pDeviceObject, 
                          IN  DeviceDispatchType pDeviceDispatchFnc,
                          IN  CleanupFncType pCleanupFnc )
{
    typedef TypeComparer<DeviceDispatchType>::Result_type ExtensionPtr_type;
    volatile char invalidTypeSpecified[sizeof(TestFunction((ExtensionPtr_type)0)) - 1];

    CommonDeviceExtension * pDevExt = (CommonDeviceExtension*)pDeviceObject->DeviceExtension;
    pDevExt->m_deviceDispatchFnc = (DeviceDispatchProc)pDeviceDispatchFnc;
    pDevExt->m_cleanProc = (CleanProc)pCleanupFnc;
    return (ExtensionPtr_type)pDevExt;
}

inline
NTSTATUS RootDispatch(
                        IN  PDEVICE_OBJECT  pDeviceObject,
						IN  PIRP            pIrp
                      )
{
    drv::CommonDeviceExtension * pDevExt = (drv::CommonDeviceExtension*)pDeviceObject->DeviceExtension;
    return pDevExt->m_deviceDispatchFnc(pDeviceObject, pIrp, pDevExt);
}

} // drv


#endif