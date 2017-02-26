#ifndef DRV_FILES_H
#define DRV_FILES_H

extern "C"
{
#include "ntddk.h"
}

typedef struct _drv_MappedFile
{
    PVOID section;
    PCHAR pData;
    LARGE_INTEGER fileSize;
}drv_MappedFile;

NTSTATUS drv_MapAllFile(HANDLE hFile, drv_MappedFile * pMappedFile);
void drv_UnMapFile(drv_MappedFile * pMappedFile);

#endif