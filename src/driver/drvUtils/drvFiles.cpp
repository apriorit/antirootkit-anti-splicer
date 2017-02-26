#include "drvFiles.h"

extern "C"
{
NTSTATUS
MmCreateSection (
    OUT PVOID *SectionObject,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN PLARGE_INTEGER MaximumSize,
    IN ULONG SectionPageProtection,
    IN ULONG AllocationAttributes,
    IN HANDLE FileHandle OPTIONAL,
    IN PFILE_OBJECT File OPTIONAL
    );


NTSTATUS
MmMapViewOfSection(
    IN PVOID SectionToMap,
    IN PEPROCESS Process,
    IN OUT PVOID *CapturedBase,
    IN ULONG_PTR ZeroBits,
    IN SIZE_T CommitSize,
    IN OUT PLARGE_INTEGER SectionOffset,
    IN OUT PSIZE_T CapturedViewSize,
    IN SECTION_INHERIT InheritDisposition,
    IN ULONG AllocationType,
    IN ULONG Protect
    );


NTSTATUS
MmUnmapViewOfSection(
    IN PEPROCESS Process,
    IN PVOID BaseAddress
     );

VOID 
NTAPI
ObMakeTemporaryObject(                                          
    IN PVOID Object                                             
    );                                                          

NTSTATUS
NTAPI
NtQueryInformationFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    OUT PVOID FileInformation,
    IN ULONG Length,
    IN FILE_INFORMATION_CLASS FileInformationClass
    );

}


NTSTATUS drv_MapAllFileEx(HANDLE hFile OPTIONAL, 
                           drv_MappedFile * pMappedFile, 
                           LARGE_INTEGER * pFileSize,
                           ULONG Protect)
{
    NTSTATUS status = STATUS_SUCCESS;
    PVOID section = 0;
    PCHAR pData=0;
    LARGE_INTEGER offset;

    offset.QuadPart = 0;

    // check zero results
    if (!pFileSize->QuadPart)
        goto calc_exit;

    status = MmCreateSection (&section,
                              SECTION_MAP_READ,
                              0, // OBJECT ATTRIBUTES
                              pFileSize, // MAXIMUM SIZE
                              Protect,
                              0x8000000,
                              hFile,
                              0
                              );


    if (status!= STATUS_SUCCESS)
         goto calc_exit;
    
 
    status = MmMapViewOfSection(section,
                                PsGetCurrentProcess(),
                                (PVOID*)&pData,
                                0,
                                0,
                                &offset,
                                &pFileSize->LowPart,
                                ViewUnmap,
                                0,
                                Protect);

    if (status!= STATUS_SUCCESS)
        goto calc_exit;



calc_exit:

    if (NT_SUCCESS(status))
    {
        pMappedFile->fileSize.QuadPart = pFileSize->QuadPart;
        pMappedFile->pData = pData;
        pMappedFile->section = section;
    }
    else
    {
        if (pData)
            MmUnmapViewOfSection(PsGetCurrentProcess(),
                                pData);

        if (section)
        {
            ObMakeTemporaryObject(section);
            ObDereferenceObject(section);
        }
    }
    return status;
}


NTSTATUS drv_GetSizeOfFile(HANDLE hFile, PLARGE_INTEGER pFileSize)
{
	IO_STATUS_BLOCK IoBlock;
	FILE_STANDARD_INFORMATION FileInfo;
    NTSTATUS status = 0;

	pFileSize->QuadPart=0;
	status = NtQueryInformationFile(hFile,&IoBlock,&FileInfo,sizeof(FileInfo),FileStandardInformation);
    if (status == STATUS_SUCCESS)
    {
		pFileSize->QuadPart = FileInfo.EndOfFile.QuadPart;
    }
    return status;
}


NTSTATUS drv_MapAllFile(HANDLE hFile, drv_MappedFile * pMappedFile)
{
    LARGE_INTEGER  fileSize;
    NTSTATUS status;
    status = drv_GetSizeOfFile(hFile, &fileSize);
    if (!NT_SUCCESS(status))
        return status;
    return drv_MapAllFileEx(hFile, pMappedFile, &fileSize, PAGE_WRITECOPY);
}

void drv_UnMapFile(drv_MappedFile * pMappedFile)
{
    if (pMappedFile->pData)
    {
        MmUnmapViewOfSection(PsGetCurrentProcess(),
                                pMappedFile->pData);
    }

    if (pMappedFile->section)
    {
        ObMakeTemporaryObject(pMappedFile->section);
        ObDereferenceObject(pMappedFile->section);
    }
    memset(pMappedFile, 0, sizeof(drv_MappedFile));
}
