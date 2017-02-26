#ifndef DRV_UNHOOKER_DEVICE_H
#define DRV_UNHOOKER_DEVICE_H

#include "drvUtils.h"

namespace drv
{

NTSTATUS CreateUnhookerDevice(IN PDRIVER_OBJECT   pDriverObject, 
                              OUT drv::CDeviceOwner & deviceObject);

}
#endif