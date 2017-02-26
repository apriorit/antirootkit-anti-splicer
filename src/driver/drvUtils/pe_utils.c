#include "pe_utils.h"
#include "ntddk_module.h"

typedef struct _drv_ResolveParams
{
    void * pModuleAddress;
    PVOID pAddress;
    IMAGE_SECTION_HEADER * pSection;
}drv_ResolveParams;
NTSTATUS __stdcall Drv_ResolveSectionAddressFnc(PIMAGE_SECTION_HEADER pSection, 
                                             void * pContext,
                                             int * bContinueSearch)
{
    drv_ResolveParams * pResolveParams = pContext;
    char * pStart = (char *)pResolveParams->pModuleAddress + pSection->VirtualAddress;
    char * pEnd = pStart + pSection->Misc.VirtualSize;

    *bContinueSearch = 1;

    // does pResolveParams->pAddress relate to this section?
    if (pResolveParams->pAddress >= pStart && pResolveParams->pAddress <= pEnd)
    {
        *bContinueSearch = 0;
        pResolveParams->pSection = pSection;
    }
    return STATUS_SUCCESS;
}


NTSTATUS drv_EnumSections(char * pModule, 
                                 drv_SectionHandlerType pSectionHandler, 
                                 void * pContext)
{
    NTSTATUS status = 0;
    PIMAGE_NT_HEADERS pPE = (PIMAGE_NT_HEADERS)(pModule+((PIMAGE_DOS_HEADER)pModule)->e_lfanew);
    short NumberOfSections = pPE->FileHeader.NumberOfSections;
    long SectionAlign=pPE->OptionalHeader.SectionAlignment;
   
    PIMAGE_SECTION_HEADER Section = (PIMAGE_SECTION_HEADER)((char*)&(pPE->FileHeader)+ 
                                                            pPE->FileHeader.SizeOfOptionalHeader+
                                                            sizeof(IMAGE_FILE_HEADER));

    {
        int i = 0, iContinueSearch = 0;
        for (i=0; i<NumberOfSections; ++i)
        {
            status = pSectionHandler(Section++, pContext, &iContinueSearch);
            if (!NT_SUCCESS(status))
                return status;

            if (!iContinueSearch)
                break;
        }
    }
    return status;
}


NTSTATUS drv_FindFirstSection(char * pModule,
                              DrvFindSectionInfo * pSectionInfo)

{
    NTSTATUS status = 0;
    PIMAGE_NT_HEADERS pPE = (PIMAGE_NT_HEADERS)(pModule+((PIMAGE_DOS_HEADER)pModule)->e_lfanew);
    short NumberOfSections = pPE->FileHeader.NumberOfSections;
    long SectionAlign=pPE->OptionalHeader.SectionAlignment;

    PIMAGE_SECTION_HEADER pSection = (PIMAGE_SECTION_HEADER)((char*)&(pPE->FileHeader)+ 
                                                            pPE->FileHeader.SizeOfOptionalHeader+
                                                            sizeof(IMAGE_FILE_HEADER));
    pSectionInfo->m_currentNumber = 0;
    pSectionInfo->m_currentSection = pSection;
    pSectionInfo->m_numberOfSections = NumberOfSections;
    pSectionInfo->m_pModuleAddress = pModule;

    pSectionInfo->m_sectionStart = (char *)pModule + pSection->VirtualAddress;
    pSectionInfo->m_sectionEnd = pSectionInfo->m_sectionStart + pSection->Misc.VirtualSize;
    return status;
}

NTSTATUS drv_FindNextSection(DrvFindSectionInfo * pSectionInfo,
                             int * pbDone)
{
    PIMAGE_SECTION_HEADER pSection = 0;
    *pbDone = 0;
    ++pSectionInfo->m_currentNumber;
    if (pSectionInfo->m_currentNumber >= pSectionInfo->m_numberOfSections)
    {
        pSectionInfo->m_currentSection = 0;
        *pbDone = 1;
        return STATUS_SUCCESS;
    }
    ++pSectionInfo->m_currentSection;
    pSection = pSectionInfo->m_currentSection;

    pSectionInfo->m_sectionStart = (char *)pSectionInfo->m_pModuleAddress + pSection->VirtualAddress;
    pSectionInfo->m_sectionEnd = pSectionInfo->m_sectionStart + pSection->Misc.VirtualSize;
    return STATUS_SUCCESS;
}

NTSTATUS Drv_ResolveSectionAddress(PVOID pModuleAddress, PVOID pAddress, IMAGE_SECTION_HEADER ** ppSection)
{
    drv_ResolveParams resolveParams;
    NTSTATUS status = 0;
    resolveParams.pModuleAddress = pModuleAddress;
    resolveParams.pSection = 0;
    resolveParams.pAddress = pAddress;

    status = drv_EnumSections(pModuleAddress, Drv_ResolveSectionAddressFnc, &resolveParams);
    if (!NT_SUCCESS(status))
        return status;

    if (!resolveParams.pSection)
        return STATUS_NOT_FOUND;
    *ppSection = resolveParams.pSection;
    return STATUS_SUCCESS;
}


// find section in file
NTSTATUS Drv_FindSection(PVOID pModuleAddress, 
                          UCHAR Name[IMAGE_SIZEOF_SHORT_NAME],
                          IMAGE_SECTION_HEADER ** ppSection)
{
    PIMAGE_DOS_HEADER dosHeader  =  (PIMAGE_DOS_HEADER)pModuleAddress;
    PIMAGE_NT_HEADERS pNTHeader = (PIMAGE_NT_HEADERS)((char*)dosHeader + dosHeader->e_lfanew);
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(pNTHeader);

    unsigned i;

    for ( i=0; i < pNTHeader->FileHeader.NumberOfSections; i++, section++ ) 
    {
        int k, found = 1;
        for(k=0;k<IMAGE_SIZEOF_SHORT_NAME;++k)
        {
            if (Name[k] != section->Name[k])
            {
                found = 0;
                break;
            }
        }
        if (found)
        {
            *ppSection = section;
            return STATUS_SUCCESS;
        }
    }
    return STATUS_NOT_FOUND;
}


NTSTATUS
Drv_GetProcAddrEx(PVOID DllBase, 
                   PCHAR FunctionName, 
                   ULONG lMaxSize,
                   PIMAGE_EXPORT_DIRECTORY ExportDirectory,
                   PUSHORT NameOrdinalTableBase,
                   ULONG ExportSize,
                   USHORT * OrdinalNumber)
{
    PULONG NameTableBase = 0;
    ULONG Low = 0;
    ULONG Middle = 0;
    ULONG High = 0;
    LONG Result = 0;
    
    
    
    if (((char*)ExportDirectory<(char*)DllBase || (char*)ExportDirectory > (char*)DllBase+lMaxSize))
    {
        return STATUS_INVALID_PARAMETER;
    }

    NameTableBase =  (PULONG)((PCHAR)DllBase + (ULONG)ExportDirectory->AddressOfNames);
    if (((char*)NameTableBase<(char*)DllBase || (char*)NameTableBase > (char*)DllBase+lMaxSize))
    {
        return STATUS_INVALID_PARAMETER;
    }

    NameOrdinalTableBase = (PUSHORT)((PCHAR)DllBase + (ULONG)ExportDirectory->AddressOfNameOrdinals);
    if (((char*)NameOrdinalTableBase<(char*)DllBase || 
        (char*)NameOrdinalTableBase > (char*)DllBase+lMaxSize))
    {
        return STATUS_INVALID_PARAMETER;
    }

    Low = 0;
    High = ExportDirectory->NumberOfNames - 1;

    // test High
    if (((char*)(NameTableBase+High)<(char*)DllBase || 
        (char*)(NameTableBase+High)> (char*)DllBase+lMaxSize))
    {
        return STATUS_INVALID_PARAMETER;
    }

    while (High >= Low && (LONG)High >= 0) 
    {
        Middle = (Low + High) >> 1;

        // test NameOrdinalTableBase[Middle]
        if (NameTableBase[Middle] > lMaxSize)
        {
            return STATUS_INVALID_PARAMETER;
        }

        Result = strcmp(FunctionName,
            (PCHAR)((PCHAR)DllBase + NameTableBase[Middle]));
        if (Result < 0) 
        {
            High = Middle - 1;
        } 
        else if (Result > 0) 
        {
            Low = Middle + 1;
        } 
        else 
        {
            break;
        }
    }


    if ((LONG)High >= (LONG)Low) 
    {
        // test NameOrdinalTableBase[Middle]
        if (((char*)(NameOrdinalTableBase+Middle)<(char*)DllBase || 
            (char*)(NameOrdinalTableBase+Middle)> (char*)DllBase+lMaxSize))
            return 0;

        *OrdinalNumber = NameOrdinalTableBase[Middle];
        return STATUS_SUCCESS;
    }
    return STATUS_NOT_FOUND;
}
