/*
    This is a C++ run-time library for Windows kernel-mode drivers.
    Copyright (C) 2003 Bo Brantùn.
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include "drvCpp.h"

typedef void (__cdecl *func_t)();

//
// Data segment with pointers to C++ initializers.
//
#pragma data_seg(".CRT$XCA")
func_t xc_a[] = { 0 };
#pragma data_seg(".CRT$XCZ")
func_t xc_z[] = { 0 };
#pragma data_seg()

#if _MSC_FULL_VER >= 140050214 || defined (_M_IA64) || defined (_M_AMD64)
#pragma comment(linker, "/merge:.CRT=.rdata")
#else
#pragma comment(linker, "/merge:.CRT=.data")
#endif

#if defined (_M_IA64) || defined (_M_AMD64)
#pragma section(".CRT$XCA",read)
#pragma section(".CRT$XCZ",read)
#endif

//
// Simple class to keep track of functions registred to be called when
// unloading the driver. Since we only use this internaly from the load
// and unload functions it doesn't need to be thread safe.
//
class AtExitCall {
public:
    AtExitCall(func_t f) : m_func(f), m_next(m_exit_list) { m_exit_list = this; }
    ~AtExitCall() { m_func(); m_exit_list = m_next; }
    static void run() { while (m_exit_list) delete m_exit_list; }
private:
    func_t              m_func;
    AtExitCall*         m_next;
    static AtExitCall*  m_exit_list;
};

AtExitCall* AtExitCall::m_exit_list = 0;

//
// Calls functions the compiler has registred to call constructors
// for global and static objects.
//
NTSTATUS __cdecl libcpp_init()
{
    NTSTATUS status;
    for (func_t* f = xc_a; f < xc_z; f++) if (*f) (*f)();

    return STATUS_SUCCESS;
}

//
// Calls functions the compiler has registred to call destructors
// for global and static objects.
//
void __cdecl libcpp_exit()
{
    AtExitCall::run();
}

namespace std
{

struct nothrow_t
{
};

}
extern "C" {

//
// The run-time support for RTTI uses malloc and free so we include them here.
//

void * __cdecl malloc(size_t size)
{
    return size ? ExAllocatePoolWithTag(NonPagedPool, size, '++CL') : 0;
}

void __cdecl free(void *p)
{
    if (p) { ExFreePool(p); }
}

//
// Registers a function to be called when unloading the driver. If memory
// couldn't be allocated the function is called immediately since it never
// will be called otherwise.
//
int __cdecl atexit(func_t f)
{
    return (new AtExitCall(f) == 0) ? (*f)(), 1 : 0;
}


void *__cdecl _encoded_null() {return 0;}
void __cdecl _lock() {}
void __cdecl _unlock() {}

} // extern "C"




void* __cdecl operator new (size_t size)
{
    PVOID ptr = ExAllocatePoolWithTag(NonPagedPool, (ULONG)size, '++CL');
    if (ptr == NULL)
    {
        return 0;
    }
    return ptr;
}

void __cdecl operator delete (void *ptr)
{
    if (ptr != NULL)
    {
        ExFreePool(ptr);
    }
}

void* __cdecl operator new[] (size_t size)
{
    PVOID ptr = ExAllocatePoolWithTag(NonPagedPool, (ULONG)size, '++CL');
    if (ptr == NULL)
    {
        return 0;
    }
    return ptr;
}

void __cdecl operator delete[] (void *ptr)
{
    if (ptr != NULL)
    {
        ExFreePool(ptr);
    }
}

void* __cdecl operator new (size_t size, POOL_TYPE pool)
{
    PVOID ptr = ExAllocatePoolWithTag(pool, (ULONG)size, '++CL');
    if (ptr == NULL)
    {
        return 0;
    }
    return ptr;
}

void __cdecl operator delete(void *ptr, POOL_TYPE pool)
{
    if (ptr != NULL)
    {
        ExFreePool(ptr);
    }
}

void* __cdecl operator new[] (size_t size, POOL_TYPE pool)
{
    PVOID ptr = ExAllocatePoolWithTag(pool, (ULONG)size, '++CL');
    if (ptr == NULL)
    {
        return 0;
    }
    return ptr;
}

void* __cdecl operator new (size_t size, const std::nothrow_t&)
{
    return ExAllocatePoolWithTag(NonPagedPool, (ULONG)size, '++CL');
}

void* __cdecl operator new[] (size_t size, const std::nothrow_t&)
{
    return ExAllocatePoolWithTag(NonPagedPool, (ULONG)size, '++CL');
}
void* __cdecl operator new (size_t size, void *ptr)
{
    return ptr;
}

void* __cdecl operator new[] (size_t size, void *ptr)
{
    return ptr;
}

void __cdecl operator delete (void *ptr, void *)
{
}

void __cdecl operator delete[] (void *ptr, void *)
{
}


extern "C" {
static ULONG NTAPI probe(CONST PVOID Buffer, ULONG Length, LOCK_OPERATION Operation)
{
    PMDL    Mdl;
    ULONG   IsBadPtr;

    Mdl = IoAllocateMdl(Buffer, Length, FALSE, FALSE, 0);

    __try
    {
        MmProbeAndLockPages(Mdl, KernelMode, Operation);
        MmUnlockPages(Mdl);
        IsBadPtr = FALSE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        IsBadPtr = TRUE;
    }

    IoFreeMdl(Mdl);

    return IsBadPtr;
}

ULONG NTAPI IsBadReadPtr(CONST PVOID Buffer, ULONG Length)
{
    return probe(Buffer, Length, IoReadAccess);
}

ULONG NTAPI IsBadWritePtr(PVOID Buffer, ULONG Length)
{
    return probe(Buffer, Length, IoWriteAccess);
}

ULONG NTAPI IsBadCodePtr(CONST PVOID Buffer)
{
    return probe(Buffer, 1, IoReadAccess);
}
}

LONG
__CxxUnhandledExceptionFilter(EXCEPTION_POINTERS *) {
    ASSERT(FALSE);
    return EXCEPTION_CONTINUE_SEARCH;
}
