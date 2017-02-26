#include "drvVirginity2.h"

extern "C"
{
NTSYSAPI 
PIMAGE_NT_HEADERS
NTAPI
RtlImageNtHeader(IN PVOID ModuleAddress ); 

NTSYSAPI
PVOID
NTAPI
RtlImageDirectoryEntryToData (
    PVOID,
    BOOLEAN,
    ULONG,
    ULONG *);
}

static 
ULONG ConvertVAToRaw(PIMAGE_NT_HEADERS pNtHeaders,
                    ULONG virtualAddr) 
{
    PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)((char*)&(pNtHeaders->FileHeader)+ 
                                                            pNtHeaders->FileHeader.SizeOfOptionalHeader+
                                                            sizeof(IMAGE_FILE_HEADER));
    for(int i=0;i < pNtHeaders->FileHeader.NumberOfSections;i++, ++pSectionHeader) 
    {
        if ((virtualAddr >= pSectionHeader->VirtualAddress) &&
            (virtualAddr <  pSectionHeader->VirtualAddress + pSectionHeader->Misc.VirtualSize))
        {
            if (!pSectionHeader->SizeOfRawData)
                return 0;

            ULONG va = pSectionHeader->VirtualAddress;
            ULONG raw = pSectionHeader->PointerToRawData;
            ULONG res =  virtualAddr - va + raw;
            return res;
        }
    }
    return 0;
}

static
PIMAGE_BASE_RELOCATION  ProcessRelocationEntry(PIMAGE_NT_HEADERS pNtHeaders,
                                               void * pMappedImage,
                                               ULONG virtualAddress,
                                               ULONG subEntriesCount,
                                               PUSHORT pSubEntry,
                                               LONG diff
                                               )
{
    for(int i = 0; i < subEntriesCount; ++i, ++pSubEntry)
    {
       USHORT offset = *pSubEntry & (USHORT)0xfff;
       ULONG rawTarget = (ULONG)ConvertVAToRaw(pNtHeaders, virtualAddress + offset);

       if (!rawTarget)
       {
           continue;
       }

       // calculate the target inside our mapped image
       PUCHAR pTarget = (PUCHAR)pMappedImage + rawTarget;
       LONG tempVal = 0;


       // done it
       switch ((*pSubEntry) >> 12) 
       {
            case IMAGE_REL_BASED_HIGHLOW :
                *(LONG UNALIGNED *)pTarget += diff;
                break;

            case IMAGE_REL_BASED_HIGH :
                tempVal = *(PUSHORT)pTarget << 16;
                tempVal += diff;
                *(PUSHORT)pTarget = (USHORT)(tempVal >> 16);
                break;

            case IMAGE_REL_BASED_ABSOLUTE :
                break;

            default :
                return NULL;
        }

    }
    return (PIMAGE_BASE_RELOCATION)pSubEntry;
}


static 
NTSTATUS FixRelocs(void * pMappedImage, 
                   void * pLoadedNtAddress)
{
    PIMAGE_NT_HEADERS pNtHeaders = RtlImageNtHeader( pMappedImage );
    ULONG oldBase = pNtHeaders->OptionalHeader.ImageBase;


    // scan for relocation section
    ULONG bytesCount = 0;
    PIMAGE_BASE_RELOCATION pRelocationEntry = 
        (PIMAGE_BASE_RELOCATION)RtlImageDirectoryEntryToData((char*)pMappedImage, 
                                                             FALSE, 
                                                             IMAGE_DIRECTORY_ENTRY_BASERELOC, 
                                                             &bytesCount);

    if (!pRelocationEntry) 
    {
        // no relocations there
        return STATUS_NOT_FOUND;
    }


    // calculate the difference
    ULONG diff = (LONG)pLoadedNtAddress - (LONG)oldBase;

    while ((int)bytesCount > 0) 
    {
        // process next entry
        bytesCount -= pRelocationEntry->SizeOfBlock;

        // parse offsets
        PUSHORT pFirstSubEntry =  (PUSHORT)((ULONG)pRelocationEntry + sizeof(IMAGE_BASE_RELOCATION));
        int iSubEntriesCount  = (pRelocationEntry->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION))/sizeof(USHORT);
        
        pRelocationEntry = ProcessRelocationEntry(pNtHeaders,
                                                  pMappedImage,
                                                  pRelocationEntry->VirtualAddress,
                                                  iSubEntriesCount,
                                                  pFirstSubEntry,
                                                  diff);
        if (!pRelocationEntry)
        {
            return STATUS_UNSUCCESSFUL;
        }
    }
    return STATUS_SUCCESS;
}

static
NTSTATUS FindOrdinal(SYSTEM_MODULE * pModule,
                     PIMAGE_THUNK_DATA pThunk,
                     PIMAGE_EXPORT_DIRECTORY pExport,
                     USHORT * pOrdinal,
                     ULONG sizeOfExportTable)
{
        
    PULONG pNameTable = (PULONG)((ULONG)pModule->pAddress +
                                 (ULONG)pExport->AddressOfNames);

    PUSHORT pOrdinalTable = (PUSHORT)((ULONG)pModule->pAddress +
                                     (ULONG)pExport->AddressOfNameOrdinals);

    PIMAGE_IMPORT_BY_NAME pAddressOfData = (PIMAGE_IMPORT_BY_NAME)pThunk->u1.AddressOfData;
    USHORT hint = pAddressOfData->Hint;

    if ((hint < pExport->NumberOfNames) &&
        (strcmp((char*)pAddressOfData->Name, (PCHAR)pModule->pAddress + pNameTable[hint]) == 0))
    {
        // ordinal found
        *pOrdinal = pOrdinalTable[hint];
        return STATUS_SUCCESS;
    } 

    // resolving by name
    
    return Drv_GetProcAddrEx(pModule->pAddress, 
                              (char*)pAddressOfData->Name, 
                              pModule->dSize,
                              pExport,
                              pOrdinalTable,
                              sizeOfExportTable,
                              pOrdinal);
   
}

static
NTSTATUS LinkThunk(SYSTEM_MODULE * pModule,
                    PIMAGE_THUNK_DATA pThunk,
                    PIMAGE_EXPORT_DIRECTORY pExport,
                    ULONG sizeOfExportTable,
                    void * pMappedImage,
                    PIMAGE_NT_HEADERS pNtHeaders, 
                    void * pLoadedNtAddress)
{
    USHORT ordinal = 0;
    if (IMAGE_SNAP_BY_ORDINAL(pThunk->u1.Ordinal))
    {
        ordinal = (ULONG)(IMAGE_ORDINAL(pThunk->u1.Ordinal) - pExport->Base);
    }
    else
    {
        // import by name
        ULONG oldAddressOfDataRaw = ConvertVAToRaw(pNtHeaders, pThunk->u1.AddressOfData);
        pThunk->u1.AddressOfData = (ULONG)pMappedImage + oldAddressOfDataRaw;

        NTSTATUS status = FindOrdinal(pModule, 
                                      pThunk, 
                                      pExport, 
                                      &ordinal, 
                                      sizeOfExportTable);
        if (!NT_SUCCESS(status))
            return status;

    }

    if (ordinal >= pExport->NumberOfFunctions) 
        return STATUS_UNSUCCESSFUL;


    PULONG pAddressOfFunctions = (PULONG)((char *)pModule->pAddress + pExport->AddressOfFunctions);
    PCHAR pTargetFunction = (PCHAR)pModule->pAddress + pAddressOfFunctions[ordinal];

    pThunk->u1.Function = (ULONG)pTargetFunction;
    return STATUS_SUCCESS;
}

static 
NTSTATUS FixImports(Drv_Resolver * pResolver,
                    void * pMappedImage, 
                    void * pLoadedNtAddress)
{
    PIMAGE_NT_HEADERS pNtHeaders = RtlImageNtHeader( pMappedImage );
    ULONG oldBase = pNtHeaders->OptionalHeader.ImageBase;


    // scan for import section
    ULONG bytesCount = 0;
    PIMAGE_IMPORT_DESCRIPTOR pImportEntry = 
        (PIMAGE_IMPORT_DESCRIPTOR)RtlImageDirectoryEntryToData((char*)pMappedImage, 
                                                             FALSE, 
                                                             IMAGE_DIRECTORY_ENTRY_IMPORT, 
                                                             &bytesCount);

    if (!pImportEntry) 
    {
        // no imports there
        return STATUS_NOT_FOUND;
    }

    // process all import entries
    for (;pImportEntry->Name && pImportEntry->FirstThunk; ++pImportEntry) 
    {
        PCHAR pDllName = (PCHAR)pMappedImage + (ULONG)ConvertVAToRaw(pNtHeaders, pImportEntry->Name);
        PCHAR pFirstThunk = (PCHAR)pMappedImage + (ULONG)ConvertVAToRaw(pNtHeaders, pImportEntry->FirstThunk);

        SYSTEM_MODULE * pModule = pResolver->LookupModule(pDllName);

        if (!pModule)
        {
            continue;
        }

        // get module exports
        ULONG sizeOfExportTable = 0;
        PIMAGE_EXPORT_DIRECTORY pExport = (PIMAGE_EXPORT_DIRECTORY)RtlImageDirectoryEntryToData(pModule->pAddress,
                                                                 TRUE,
                                                                 IMAGE_DIRECTORY_ENTRY_EXPORT,
                                                                 &sizeOfExportTable);


        // process all thunks
        PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)pFirstThunk;
        for(; pThunk->u1.AddressOfData; ++pThunk)
        {
            NTSTATUS status = LinkThunk(pModule,
                               pThunk,
                               pExport,
                               sizeOfExportTable,
                               pMappedImage,
                               pNtHeaders, 
                               pLoadedNtAddress);
            NT_CHECK(status);
        }
    }
    return STATUS_SUCCESS;
}

const char * g_pageSectionName = "PAGE";
const char * g_textSectionName = ".text";

NTSTATUS Drv_InitVirginityContext2(Drv_VirginityContext2 * pContext)
{
    NTSTATUS status = pContext->m_resolver.Init();
    if (!NT_SUCCESS(status))
        return status;

    // fill sections to scan
    RtlInitAnsiString( pContext->m_sectionsToCheck + 0, g_pageSectionName);
    RtlInitAnsiString( pContext->m_sectionsToCheck + 1, g_textSectionName);

    status = Drv_VirginityInit(&pContext->m_parent);
    if (!NT_SUCCESS(status))
        return status;

    status = FixRelocs(pContext->m_parent.m_mapped.pData, 
                       pContext->m_parent.m_pLoadedNtAddress);

    if (!NT_SUCCESS(status))
    {
        Drv_FreeVirginityContext2(pContext);
        return status;
    }

    status = FixImports(&pContext->m_resolver,
                        pContext->m_parent.m_mapped.pData, 
                        pContext->m_parent.m_pLoadedNtAddress);

    if (!NT_SUCCESS(status))
    {
        Drv_FreeVirginityContext2(pContext);
    }
    return status;
}
void Drv_FreeVirginityContext2(Drv_VirginityContext2 * pContext)
{
    Drv_VirginityFree(&pContext->m_parent);
}


const char * Drv_GetMappedSectionStart(const Drv_VirginityContext2 * pContext)
{
    char * pMappedSectionStart = 
            (char *)pContext->m_parent.m_mapped.pData +  pContext->m_pMappedSection->PointerToRawData;

    return pMappedSectionStart;
}

void Drv_GetRealSSTValue2(Drv_VirginityContext2 * pContext, long index, void ** ppValue)
{
    char * pSST = pContext->m_parent.m_pMappedSectionStart + pContext->m_parent.m_sstOffsetInSection;
    ULONG * pValue = ((ULONG *) pSST)+index;
    *ppValue = (void*)*pValue;
}

static int GetStartingEqualBytesCount(int a, int b)
{
    int iCount = 0;

    if (a == b)
    {
        KeBugCheck(1);
    }

    if ((a&0x000000FF) == (b&0x000000FF))
    {
        ++iCount;
        if ((a&0x0000FF00) == (b&0x0000FF00))
        {
            ++iCount;
            if ((a&0x00FF0000) == (b&0x00FF0000))
            {
                ++iCount;
            }
        }
    }
    return iCount;
}
static int GetTailingEqualBytesCount(int a, int b)
{
    int iCount = 0;

    if (a == b)
    {
        KeBugCheck(1);
    }

    if ((a&0xFF000000) == (b&0xFF000000))
    {
        ++iCount;
        if ((a&0x00FF0000) == (b&0x00FF0000))
        {
            ++iCount;
            if ((a&0x0000FF00) == (b&0x0000FF00))
            {
                ++iCount;
            }
        }
    }
    return iCount;
}


static 
NTSTATUS FindModificationInSection(Drv_VirginityContext2 * pContext, 
                                   void ** ppStart,
                                   int * pSize)
{
    ULONG isDiscardable = pContext->m_currentSectionInfo.m_currentSection->Characteristics & IMAGE_SCN_MEM_DISCARDABLE;
    ULONG isExecutable = pContext->m_currentSectionInfo.m_currentSection->Characteristics & IMAGE_SCN_MEM_EXECUTE;
    ULONG isWritable = pContext->m_currentSectionInfo.m_currentSection->Characteristics & IMAGE_SCN_MEM_WRITE;
    
    pContext->m_startOfModification = 0;

    if (isDiscardable || (!isExecutable) || isWritable)
    {
        STRING sectionName;
        sectionName.Buffer = (PCHAR)pContext->m_currentSectionInfo.m_currentSection->Name;
        sectionName.Length = sizeof(pContext->m_currentSectionInfo.m_currentSection->Name);
        sectionName.MaximumLength = sectionName.Length;

        bool bNeedToCheck = false;
        for(int i = 0; i < DRV_SECTIONS_COUNT; ++i)
        {
            if (RtlCompareString(&sectionName, pContext->m_sectionsToCheck + i, TRUE) == 0)
            {
                bNeedToCheck = true;
                break;
            }
        }
        if (!bNeedToCheck)
            return STATUS_SUCCESS;
    }

    // normal executable non-writable section, scan it
    const char * pMappedSectionStart = Drv_GetMappedSectionStart( pContext );

    // ready to compare them 
    int sizeInInts = (pContext->m_currentSectionInfo.m_sectionEnd - pContext->m_currentSectionInfo.m_sectionStart)/4;

    int bInModification = 0;
    int * pMappedSectionStartInt = (int*)pMappedSectionStart;
    int * pOriginalSectionStartInt = (int*)pContext->m_currentSectionInfo.m_sectionStart;
    int i = pContext->m_endOfModification;
    for(;
        i < sizeInInts;
        ++i)
    {
        if (pOriginalSectionStartInt[i] != pMappedSectionStartInt[i])
        {
            if (!bInModification)
            {
                // make it more accurate
                int iRealStartOfModification =  i*4;

                iRealStartOfModification += GetStartingEqualBytesCount(pOriginalSectionStartInt[i],
                                                                      pMappedSectionStartInt[i]);
        
                pContext->m_startOfModification = iRealStartOfModification;
                bInModification = 1;
            }
            continue;
        }
        else
        {
            // we got 4 equal bytes
            if (bInModification)
            {
                break;
            }
        }
    }
    
    if (bInModification)
    {
        pContext->m_endOfModification = i*4;

        pContext->m_endOfModification -= GetTailingEqualBytesCount(pOriginalSectionStartInt[i-1],
                                                                   pMappedSectionStartInt[i-1]);

        *pSize = pContext->m_endOfModification - pContext->m_startOfModification;
        *ppStart =  (int *)pOriginalSectionStartInt + i;
    }
    return STATUS_SUCCESS;
}

static 
NTSTATUS FindModificationImpl(Drv_VirginityContext2 * pContext, 
                                void ** ppStart,
                                int * pSize)
{
    *ppStart = 0;
    *pSize = 0;

    NTSTATUS status = 0;
    while(1)
    {
        status = FindModificationInSection(pContext, 
                                           ppStart,
                                           pSize);
        if (!NT_SUCCESS(status))
            return status;

        // check if smth found
        if (*ppStart)
            return STATUS_SUCCESS;

        // not found, try next section
        pContext->m_endOfModification = 0;

        int bSectionNotFound = 0;
        status = drv_FindNextSection(&pContext->m_currentSectionInfo, &bSectionNotFound);
        if (!NT_SUCCESS(status))
            return status;

        if (bSectionNotFound)
            return STATUS_SUCCESS;

        // find appropriate section in the mapped file
        status = Drv_FindSection(pContext->m_parent.m_mapped.pData, 
                                pContext->m_currentSectionInfo.m_currentSection->Name, 
                                &pContext->m_pMappedSection);
        if (!NT_SUCCESS(status))
            return status;

    }

    return STATUS_SUCCESS;
}

NTSTATUS Drv_GetFirstModification(Drv_VirginityContext2 * pContext, 
                                void ** ppStart,
                                int * pSize)
{
    int done = 0;
    NTSTATUS status = drv_FindFirstSection(pContext->m_parent.m_pLoadedNtAddress, 
                                           &pContext->m_currentSectionInfo);
    if (!NT_SUCCESS(status))
        return status;

    pContext->m_endOfModification = 0;

    // scan for section in mapped file
    status = Drv_FindSection(pContext->m_parent.m_mapped.pData, 
                             pContext->m_currentSectionInfo.m_currentSection->Name, 
                             &pContext->m_pMappedSection);
    if (!NT_SUCCESS(status))
        return status;

    return FindModificationImpl(pContext, ppStart, pSize);
}

NTSTATUS Drv_GetNextModification(Drv_VirginityContext2 * pContext, 
                                void ** ppStart,
                                int * pSize)
{
    return FindModificationImpl(pContext, ppStart, pSize);
}