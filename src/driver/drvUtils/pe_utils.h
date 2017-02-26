#ifndef PE_UTILS_H
#define PE_UTILS_H

#include <ntddk.h>
#include <ntimage.h>
#include "drv_ntdefinitions.h"

typedef struct _DrvFindSectionInfo
{
    char * m_pModuleAddress;
    int m_currentNumber;
    int m_numberOfSections;
    PIMAGE_SECTION_HEADER m_currentSection;
    char * m_sectionStart;
    char * m_sectionEnd;
}DrvFindSectionInfo;


NTSTATUS drv_FindFirstSection(char * pModule,
                              DrvFindSectionInfo * pSectionInfo);

NTSTATUS drv_FindNextSection(DrvFindSectionInfo * pSectionInfo,
                             int * pbDone);



typedef NTSTATUS (__stdcall *drv_SectionHandlerType)(PIMAGE_SECTION_HEADER pSection, 
                                                 void * pContext,
                                                 int * bContinueSearch); // out


NTSTATUS drv_EnumSections(char * pModule, 
                                drv_SectionHandlerType pSectionHandler, 
                                void * pContext);
NTSTATUS Drv_ResolveSectionAddress(PVOID pModuleAddress, PVOID pAddress, IMAGE_SECTION_HEADER ** ppSection);

NTSTATUS Drv_FindSection(PVOID pModuleAddress, 
                          UCHAR Name[IMAGE_SIZEOF_SHORT_NAME],
                          IMAGE_SECTION_HEADER ** ppSection);

NTSTATUS
Drv_GetProcAddrEx(PVOID DllBase, 
                   PCHAR FunctionName, 
                   ULONG lMaxSize,
                   PIMAGE_EXPORT_DIRECTORY ExportDirectory,
                   PUSHORT NameOrdinalTableBase,
                   ULONG ExportSize,
                   USHORT * OrdinalNumber);
#endif