#include "drvVirginity.h" 

extern "C"
{
#include "pe_utils.h"
#include "ntddk_module.h"

extern PSERVICE_DESCRIPTOR_TABLE KeServiceDescriptorTable;

}




ULONG DisableKernelDefence(KIRQL  * OldIrql)
{
    ULONG OldCr0=0;

    __asm
    {
        cli
        mov     eax, cr0
        mov     OldCr0, eax
        and     eax, 0xFFFEFFFF
        mov     cr0, eax
    }
    return OldCr0;
}

VOID EnableKernelDefence(ULONG OldCr0, KIRQL OldIrql)
{
    __asm
    {
        mov  eax, OldCr0
        mov     cr0, eax
        sti
    }
}


void Drv_HookSST(PVOID * ppPlaceToHook, PVOID pNewHook)
{
    PVOID result;
    KIRQL OldIrqL;
    ULONG OldCr0;

    OldCr0 = DisableKernelDefence(&OldIrqL);
    *ppPlaceToHook = pNewHook;
    EnableKernelDefence(OldCr0, OldIrqL);
}

void Drv_HookMemCpy(void * dest, 
                    const void * src, 
                    size_t count)
{
    PVOID result;
    KIRQL OldIrqL;
    ULONG OldCr0;

    OldCr0 = DisableKernelDefence(&OldIrqL);
    
    memcpy(dest, src, count);

    EnableKernelDefence(OldCr0, OldIrqL);
}

void ** Drv_GetNtosSSTEntry(int index)
{
    return (void ** )(KeServiceDescriptorTable->ntoskrnl.ServiceTable + index);
}

ULONG Drv_GetSizeOfNtosSST()
{
    return KeServiceDescriptorTable->ntoskrnl.ServiceLimit;
}

NTSTATUS GetAllModulesInfo(PULONG * pRes)
{
    ULONG i;
    NTSTATUS status = STATUS_NOT_FOUND;

    unsigned long Base = 0;

    status = NtQuerySystemInformation( SystemModuleInformation, &i, 0, &i ); // system module info
    if (status != STATUS_INFO_LENGTH_MISMATCH)
        return status;

    *pRes = (PULONG)ExAllocatePool(NonPagedPool, sizeof(ULONG)*i);

    if (!*pRes)
        return STATUS_NO_MEMORY;

    status = NtQuerySystemInformation( SystemModuleInformation, *pRes, i * sizeof(ULONG), 0);
    if (status != STATUS_SUCCESS)
    {
        ExFreePool(*pRes);
        return STATUS_UNSUCCESSFUL;
    }
    return STATUS_SUCCESS;    
}

BOOLEAN iskernelName(CHAR* name)
{
    if (!_strnicmp(name, "nt", 2) || !_strnicmp(name, "wrkx", 4))
        return TRUE;
    return FALSE;
} 

NTSTATUS GetNtoskrnlInfo(SYSTEM_MODULE * pInfo)
{
    ULONG i;
    NTSTATUS status = STATUS_SUCCESS;
    PULONG pBuf = 0;
    PSYSTEM_MODULE_INFORMATION pModInfo;
    status = GetAllModulesInfo(&pBuf);
    if(status != STATUS_SUCCESS)
        return status;
 
    pModInfo = (PSYSTEM_MODULE_INFORMATION)pBuf;
    status = STATUS_NOT_FOUND;

    for (i=0; (i<*pBuf) && i<2; i++)
    {
        if (iskernelName((CHAR*)pModInfo->aSM[i].abName + pModInfo->aSM[i].wNameOffset))
        {
            *pInfo=pModInfo->aSM[i];
            status = STATUS_SUCCESS;
            break;
        }
    }
    ExFreePool(pBuf);
    return status;
}


NTSTATUS drv_MapFile(drv_MappedFile * pMmapped,
                         UNICODE_STRING * pNtosName,
                         HANDLE * phFile)
{
    NTSTATUS status = 0;
    HANDLE hFile = 0;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;

    InitializeObjectAttributes(&oa,pNtosName,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);

    status = IoCreateFile(&hFile, 
                            GENERIC_READ, 
                            &oa, 
                            &iosb, 
                            0, 
                            0, 
                            FILE_SHARE_WRITE|FILE_SHARE_READ, 
                            FILE_OPEN, 
                            FILE_NON_DIRECTORY_FILE, 
                            0, 
                            0,
                            CreateFileTypeNone,
                            0,
                            IO_NO_PARAMETER_CHECKING);
    if (!NT_SUCCESS(status))
        goto clean;

    // map file
    status = drv_MapAllFile(hFile, pMmapped);
    if (!NT_SUCCESS(status))
        goto clean;

    // commit
    *phFile = hFile;

    hFile = 0;
clean:
    if (hFile)
        NtClose(hFile);
    return status;
}

static NTSTATUS ResolveSST(Drv_VirginityContext * pContext, SYSTEM_MODULE * pNtOsInfo)
{
    PIMAGE_SECTION_HEADER pSection = 0;
    PIMAGE_SECTION_HEADER pMappedSection = 0;
    NTSTATUS status = 0;
    PNTPROC pStartSST = KeServiceDescriptorTable->ntoskrnl.ServiceTable;
    char * pSectionStart = 0;
    char * pMappedSectionStart = 0;

    pContext->m_pLoadedNtAddress = (char*)pNtOsInfo->pAddress;
    status = Drv_ResolveSectionAddress(pNtOsInfo->pAddress, pStartSST, &pSection);
    if (!NT_SUCCESS(status))
        goto clean;
    
    memcpy(pContext->m_SectionName, pSection->Name, IMAGE_SIZEOF_SHORT_NAME);
    
    pSectionStart = (char *)pNtOsInfo->pAddress + pSection->VirtualAddress;
    pContext->m_sstOffsetInSection = (char*)pStartSST - pSectionStart;

    status = Drv_FindSection(pContext->m_mapped.pData, pSection->Name, &pMappedSection);
    if (!NT_SUCCESS(status))
        goto clean;

    pMappedSectionStart = (char *)pContext->m_mapped.pData + pMappedSection->PointerToRawData;
    pContext->m_mappedSST = pMappedSectionStart+pContext->m_sstOffsetInSection;

    {
        PIMAGE_DOS_HEADER dosHeader  =  (PIMAGE_DOS_HEADER)pContext->m_mapped.pData;
        PIMAGE_NT_HEADERS pNTHeader = (PIMAGE_NT_HEADERS)((char*)dosHeader + dosHeader->e_lfanew);
        pContext->m_imageBase = pNTHeader->OptionalHeader.ImageBase;
    }

    pContext->m_pSectionStart = pSectionStart;
    pContext->m_pMappedSectionStart = pMappedSectionStart;
clean:
    return status;
}

NTSTATUS Drv_VirginityInit(Drv_VirginityContext * pContext)
{
    NTSTATUS status = 0;
    SYSTEM_MODULE * pNtOsInfo = 0;
    ANSI_STRING kernelAnsiName;
    UNICODE_STRING kernelUnicodeName;
    wchar_t * pNtosFileName = 0;
    UNICODE_STRING ntosName;
    
    kernelUnicodeName.Buffer = 0;

    memset(pContext, 0, sizeof(Drv_VirginityContext));
    pNtOsInfo = (SYSTEM_MODULE*)ExAllocatePool(PagedPool, sizeof(SYSTEM_MODULE));
    if (!pNtOsInfo)
    {
        status = STATUS_NO_MEMORY;
        goto clean;
    }

    memset(pNtOsInfo, 0, sizeof(*pNtOsInfo));
    status = GetNtoskrnlInfo(pNtOsInfo);
    if (!NT_SUCCESS(status))
        goto clean;

    pNtosFileName = (wchar_t*)ExAllocatePool(PagedPool, 1024);
    if (!pNtosFileName)
    {
        status = STATUS_NO_MEMORY;
        goto clean;
    }
    ntosName.Buffer = pNtosFileName;
    ntosName.Length = 0;
    ntosName.MaximumLength = 1024;

    status = RtlAppendUnicodeToString(&ntosName, L"\\SystemRoot\\system32\\");
    if (!NT_SUCCESS(status))
        goto clean;

    RtlInitAnsiString(&kernelAnsiName, (char*)pNtOsInfo->abName + pNtOsInfo->wNameOffset);
        
    status = RtlAnsiStringToUnicodeString(&kernelUnicodeName, &kernelAnsiName, TRUE);
    if (!NT_SUCCESS(status))
        goto clean;

    status = RtlAppendUnicodeStringToString(&ntosName, &kernelUnicodeName);
    if (!NT_SUCCESS(status))
        goto clean;
    
    // now in ntosName - fullPath
    // open file
    status = drv_MapFile(&pContext->m_mapped, &ntosName, &pContext->m_hFile);
    if (!NT_SUCCESS(status))
        goto clean;

    // parse file - find sst address
    status = ResolveSST(pContext, pNtOsInfo);
    if (!NT_SUCCESS(status))
        goto clean;
clean:
    if (!NT_SUCCESS(status))
    {
        // free
        Drv_VirginityFree(pContext);
    }
    if (pNtOsInfo)
        ExFreePool(pNtOsInfo);
    if (pNtosFileName)
        ExFreePool(pNtosFileName);
    if (kernelUnicodeName.Buffer)
        ExFreePool(kernelUnicodeName.Buffer);
    return status;

}
void Drv_VirginityFree(Drv_VirginityContext * pContext)
{
    drv_UnMapFile(&pContext->m_mapped);

    if (pContext->m_hFile)
    {
        NtClose(pContext->m_hFile);
    }
}

void Drv_GetRealSSTValue(Drv_VirginityContext * pContext, long index, void ** ppValue)
{
    char * pSST = pContext->m_pMappedSectionStart + pContext->m_sstOffsetInSection;
    ULONG * pValue = ((ULONG *) pSST)+index;
    *ppValue = (void*)(*pValue + (ULONG)pContext->m_pLoadedNtAddress - pContext->m_imageBase);
}



NTSTATUS Drv_ResolverContextInit(Drv_ResolverContext * pContext)
{
    PSYSTEM_MODULE_INFORMATION pModInfo;
    NTSTATUS status = 0;

    pContext->m_pModInfo = 0;

    status = GetAllModulesInfo((PULONG*)&pModInfo);
    if(!NT_SUCCESS(status))
        return status;

    pContext->m_pModInfo = pModInfo;
    return status;
}
void Drv_ResolverContextFree(Drv_ResolverContext * pContext)
{
    if (pContext->m_pModInfo)
        ExFreePool(pContext->m_pModInfo);
}

SYSTEM_MODULE * Drv_ResolverLookupModule(Drv_ResolverContext * pContext, const void * pPointer)
{
    for (int i=0; i < pContext->m_pModInfo->dCount; i++)
    {
        SYSTEM_MODULE * pModule = pContext->m_pModInfo->aSM + i;
        if ((char*)pPointer > (char*)pModule->pAddress && (char*)pPointer < ((char*)pModule->pAddress + pModule->dSize))
        {
            return pModule;
        }
    }
    return 0;
}
SYSTEM_MODULE * Drv_ResolverLookupModule2(Drv_ResolverContext * pContext, const char * pName)
{
    STRING dllName;
    RtlInitAnsiString(&dllName, pName);

    STRING halName;
    halName.Buffer = "hal.dll";
    halName.Length = 7;
    halName.MaximumLength = 7;
    
    for (int i=0; i < pContext->m_pModInfo->dCount; i++)
    {
        SYSTEM_MODULE * pModule = pContext->m_pModInfo->aSM + i;
        int moduleNameSize = (int)strlen(pModule->abName);

        if (moduleNameSize > dllName.Length)
        {
            char * pShortName = pModule->abName + moduleNameSize - dllName.Length;
            STRING moduleName;
            moduleName.Buffer = pShortName;
            moduleName.Length = dllName.Length;
            moduleName.MaximumLength = dllName.MaximumLength;

            if (RtlCompareString(&dllName, &moduleName, TRUE) == 0)
                return pModule;
        }

        // cratch for HAL
        if (i == 1)
        {
            if (RtlCompareString(&dllName, &halName, TRUE) == 0)
            {
                // client wants hal.dll
                // but it has the name like this:
                //     "\SystemRoot\system32\halmacpi.dll"
                // return 1st module 
                return pModule;
            }
        }
    }
    return 0;
}

Drv_Resolver::Drv_Resolver()
    : m_bInited(false)
{
    memset(&m_context, 0, sizeof(m_context));
}
Drv_Resolver::~Drv_Resolver()
{
    if (m_bInited)
    {
        Drv_ResolverContextFree(&m_context);
    }
}
NTSTATUS Drv_Resolver::Init()
{
    NT_CHECK(Drv_ResolverContextInit(&m_context));
    m_bInited = 1;
    return NT_OK;
}
SYSTEM_MODULE * Drv_Resolver::LookupModule(const void * pPointer)
{
    return Drv_ResolverLookupModule(&m_context, pPointer);
}
SYSTEM_MODULE * Drv_Resolver::LookupModule(const char * pName)
{
    return Drv_ResolverLookupModule2(&m_context, pName);
}