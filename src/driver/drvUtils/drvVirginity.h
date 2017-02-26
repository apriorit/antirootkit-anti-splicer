#ifndef DRV_VIRGINITY_H
#define DRV_VIRGINITY_H

#include "drvFiles.h"
#include "drvCpp.h"
extern "C"
{
#include "drv_NtDefinitions.h"
}

typedef struct _Drv_VirginityContext
{
    drv_MappedFile m_mapped;
    HANDLE m_hFile;
    UCHAR  m_SectionName[IMAGE_SIZEOF_SHORT_NAME+1];
    ULONG  m_sstOffsetInSection;
    char * m_mappedSST;
    ULONG m_imageBase;
    char * m_pSectionStart;
    char * m_pMappedSectionStart;
    char * m_pLoadedNtAddress;
}Drv_VirginityContext;

NTSTATUS Drv_VirginityInit(Drv_VirginityContext * pContext);
void Drv_VirginityFree(Drv_VirginityContext * pContext);
void Drv_GetRealSSTValue(Drv_VirginityContext * pContext, long index, void ** pValue);

ULONG Drv_GetSizeOfNtosSST();
void ** Drv_GetNtosSSTEntry(int index);

ULONG DisableKernelDefence(KIRQL  * OldIrql);
VOID EnableKernelDefence(ULONG OldCr0, KIRQL OldIrql);

void Drv_HookSST(PVOID * ppPlaceToHook, PVOID pNewHook);
void Drv_HookMemCpy(void * dest, 
                    const void * src, 
                    size_t count);

class CAutoVirginity
{
    Drv_VirginityContext * m_pContext;
    CAutoVirginity(const CAutoVirginity&);
    CAutoVirginity&operator = (const CAutoVirginity&);
public:
    CAutoVirginity()
        : m_pContext(0)
    {
    }
    NTSTATUS Init(Drv_VirginityContext * pContext)
    {
        NT_CHECK(Drv_VirginityInit(pContext));
        m_pContext = pContext;
        return NT_OK;
    }
    ~CAutoVirginity()
    {
        if (m_pContext)
            Drv_VirginityFree(m_pContext);
    }
};


typedef struct _Drv_ResolverContext
{
    PSYSTEM_MODULE_INFORMATION m_pModInfo;
}
Drv_ResolverContext;

NTSTATUS Drv_ResolverContextInit(Drv_ResolverContext * pContext);
void Drv_ResolverContextFree(Drv_ResolverContext * pContext);
SYSTEM_MODULE * Drv_ResolverLookupModule(Drv_ResolverContext * pContext, const void * pPointer);
SYSTEM_MODULE * Drv_ResolverLookupModule2(Drv_ResolverContext * pContext, const char * pName);

class Drv_Resolver
{
    Drv_ResolverContext m_context;
    int m_bInited;
    Drv_Resolver(const Drv_Resolver & );
    Drv_Resolver&operator =(const Drv_Resolver & );
public:
    Drv_Resolver();
    ~Drv_Resolver();
    NTSTATUS Init();
    SYSTEM_MODULE * LookupModule(const void * pPointer);
    SYSTEM_MODULE * LookupModule(const char * pName);
};

NTSTATUS GetNtoskrnlInfo(SYSTEM_MODULE * pInfo);
NTSTATUS drv_MapFile(drv_MappedFile * pMmapped,
                         UNICODE_STRING * pNtosName,
                         HANDLE * phFile);

#endif 