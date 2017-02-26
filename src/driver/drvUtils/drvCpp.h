#ifndef DRV_CPP_H
#define DRV_CPP_H

extern "C"
{
#include "ntddk.h"
}

void* __cdecl operator new (size_t size, void *ptr);
void* __cdecl operator new[] (size_t size, void *ptr);
void __cdecl operator delete (void *ptr, void *);
void __cdecl operator delete[] (void *ptr, void *);

NTSTATUS __cdecl libcpp_init();
void __cdecl libcpp_exit();

#define NT_CHECK(x) {if (!(NT_SUCCESS((x)))) return x;}
#define NT_CHECK_ALLOC(p) {if (!p) return STATUS_NO_MEMORY;}
#define NT_OK STATUS_SUCCESS

namespace drv
{

class CLibCpp
{
    bool m_inited;
public:
    CLibCpp()
        : m_inited(false)
    {
    }
    NTSTATUS Init()
    {
        return libcpp_init();
    }
    void release()
    {
        m_inited = false;
    }
    ~CLibCpp()
    {
        if (m_inited)
        {
            libcpp_exit();
        }
    }
};

} // drv
#endif 