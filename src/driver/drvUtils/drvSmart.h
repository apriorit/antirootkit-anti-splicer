#ifndef DRV_SMART_H
#define DRV_SMART_H

#include "drvExtensions.h"

namespace drv
{

class CDeviceOwner
{
    _DEVICE_OBJECT * m_pDevice;
public:
    CDeviceOwner(_DEVICE_OBJECT * pDevice=0)
        :   m_pDevice(pDevice)
    {
    }
    CDeviceOwner(CDeviceOwner & owner)
        :   m_pDevice(0)
    {
        reset(owner.release());
    }
    CDeviceOwner & operator = (CDeviceOwner & owner)
    {
        reset(owner.release());
        return *this;
    }
    ~CDeviceOwner()
    {
        reset(0);
    }
    _DEVICE_OBJECT * get()
    {
        return m_pDevice;
    }
    _DEVICE_OBJECT * operator ->()
    {
        return m_pDevice;
    }
    _DEVICE_OBJECT * release()
    {
        _DEVICE_OBJECT * pOldDevice = m_pDevice;
        m_pDevice = 0;
        return pOldDevice;
    }
    void reset(_DEVICE_OBJECT * pDevice)
    {
        _DEVICE_OBJECT * pOldDevice = m_pDevice;
        m_pDevice = pDevice;
        if (pOldDevice)
        {
            CommonDeviceExtension * pDevExt = (CommonDeviceExtension*)pOldDevice->DeviceExtension;
            pDevExt->m_cleanProc(pDevExt);
            IoDeleteDevice(pOldDevice);
        }
    }
    _DEVICE_OBJECT ** GetPtr2()
    {
        return &m_pDevice;
    }
};


} // drv
#endif