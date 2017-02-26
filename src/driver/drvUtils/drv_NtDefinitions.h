#ifndef DRV_NT_DEFINITIONS_H
#define DRV_NT_DEFINITIONS_H

//---------------------------------------------------------------------------------------
// versions

typedef struct _OSVERSIONINFOEXW_CMN {
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR  szCSDVersion[ 128 ];     // Maintenance string for PSS usage
    USHORT wServicePackMajor;
    USHORT wServicePackMinor;
    USHORT wSuiteMask;
    UCHAR wProductType;
    UCHAR wReserved;
} OSVERSIONINFOEXW_CMN, 
*POSVERSIONINFOEXW_CMN, 
*LPOSVERSIONINFOEXW_CMN, 
RTL_OSVERSIONINFOEXW_CMN, 
*PRTL_OSVERSIONINFOEXW_CMN;


typedef NTSTATUS
(*RtlGetVersionProc)(
    OUT  POSVERSIONINFOEXW_CMN lpVersionInformation
    );

//---------------------------------------------------------------------------------------
// Hooking supply
//---------------------------------------------------------------------------------------
#define SYSCALL_INDEX(_Function) *(PULONG)((PUCHAR)_Function+1)

typedef NTSTATUS (NTAPI *NTPROC) ();
typedef NTPROC *PNTPROC;
typedef struct _SYSTEM_SERVICE_TABLE
{
    /*000*/ PNTPROC ServiceTable;           // array of entry points
    /*004*/ ULONG *  CounterTable;           // array of usage counters
    /*008*/ ULONG   ServiceLimit;           // number of table entries
    /*00C*/ UCHAR *  ArgumentTable;          // array of byte counts
    /*010*/ 
}SYSTEM_SERVICE_TABLE, * PSYSTEM_SERVICE_TABLE,    **PPSYSTEM_SERVICE_TABLE;

typedef struct _SERVICE_DESCRIPTOR_TABLE
{
    /*000*/ SYSTEM_SERVICE_TABLE ntoskrnl;  // ntoskrnl.exe (native api)
    /*010*/ SYSTEM_SERVICE_TABLE win32k;    // win32k.sys   (gdi/user)
    /*020*/ SYSTEM_SERVICE_TABLE Table3;    // not used
    /*030*/ SYSTEM_SERVICE_TABLE Table4;    // not used
    /*040*/ 
}SERVICE_DESCRIPTOR_TABLE, * PSERVICE_DESCRIPTOR_TABLE,    **PPSERVICE_DESCRIPTOR_TABLE;

//---------------------------------------------------------------------------------------
// TOKENS
//---------------------------------------------------------------------------------------
NTSTATUS
ZwOpenProcessToken(
                   HANDLE Token,
                   ULONG TokenRights,
                   PVOID pointHandle
                   );

NTSTATUS 
ZwOpenThreadToken(IN HANDLE ThreadHandle, 
                  IN ACCESS_MASK DesiredAccess, 
                  IN BOOLEAN OpenAsSelf, 
                  OUT PHANDLE TokenHandle);

//---------------------------------------------------------------------------------------
// QUERIES
//---------------------------------------------------------------------------------------
typedef NTSTATUS (*ZwDelayExecutionProc)(IN BOOLEAN Alertable, IN PLARGE_INTEGER Interval);
//
// Process Device Map information
//  NtQueryInformationProcess using ProcessDeviceMap
//  NtSetInformationProcess using ProcessDeviceMap
//


typedef struct _SYSTEM_MODULE
{
    ULONG dReserved01;
    ULONG d04;
    PVOID pAddress;
    ULONG dSize;                // bytes
    ULONG dFlags;
    USHORT  wId;                  // zero based
    USHORT  wRank;                // 0 if not assigned
    USHORT  w18;
    USHORT  wNameOffset;
    CHAR  abName [MAXIMUM_FILENAME_LENGTH];
} SYSTEM_MODULE, * PSYSTEM_MODULE, **PPSYSTEM_MODULE;

#define SYSTEM_MODULE_ \
sizeof (SYSTEM_MODULE)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

typedef struct _SYSTEM_MODULE_INFORMATION
{
    ULONG         dCount;
    SYSTEM_MODULE aSM [1];
}
SYSTEM_MODULE_INFORMATION,
* PSYSTEM_MODULE_INFORMATION,
**PPSYSTEM_MODULE_INFORMATION;

#define SYSTEM_MODULE_INFORMATION_ \
sizeof (SYSTEM_MODULE_INFORMATION)

#define SYSCALL_INDEX(_Function) *(PULONG)((PUCHAR)_Function+1)

typedef unsigned __int64 QWORD;

typedef struct _SYSTEM_THREAD
{
    QWORD        qKernelTime;       // 100 nsec units
    QWORD        qUserTime;         // 100 nsec units
    QWORD        qCreateTime;       // relative to 01-01-1601
    ULONG        d18;
    PVOID        pStartAddress;
    CLIENT_ID    Cid;               // process/thread ids
    ULONG        dPriority;
    ULONG        dBasePriority;
    ULONG        dContextSwitches;
    ULONG        dThreadState;      // 2=running, 5=waiting
    KWAIT_REASON WaitReason;
    ULONG        dReserved01;
}
SYSTEM_THREAD,
* PSYSTEM_THREAD,
**PPSYSTEM_THREAD;

#define SYSTEM_THREAD_ \
sizeof (SYSTEM_THREAD)

typedef struct _SYSTEM_THREAD_INFORMATION {
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER CreateTime;
    ULONG WaitTime;
    PVOID StartAddress;
    CLIENT_ID ClientId;
    KPRIORITY Priority;
    LONG BasePriority;
    ULONG ContextSwitches;
    ULONG ThreadState;
    ULONG WaitReason;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

//
// Process Virtual Memory read\write 
//

typedef
NTSTATUS 
(*ZwWriteVirtualMemoryProc)(
                            HANDLE ProcessHandle, 
                            PVOID BaseAddress, 
                            PVOID Buffer, 
                            ULONG BufferSize, 
                            PULONG NumberOfBytesWritten
                            );

typedef
NTSTATUS 
(*ZwReadVirtualMemoryProc)(
                           HANDLE ProcessHandle, 
                           PVOID BaseAddress,
                           PVOID Buffer,
                           ULONG BufferSize, 
                           PULONG NumberOfBytesRead
                           );

//
// Process Virtual Memory Counters
//  NtQueryInformationProcess using ProcessVmCounters
//



typedef PVOID           POBJECT;
#define MAXPATHLEN     1024
#define MAXNTPATH 260

#define DRIVE_UNKNOWN     0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_REMOVABLE   2
#define DRIVE_FIXED       3
#define DRIVE_REMOTE      4
#define DRIVE_CDROM       5
#define DRIVE_RAMDISK     6


typedef struct _OBJECT_DIRECTORY_INFORMATION {
    UNICODE_STRING ObjectName;
    UNICODE_STRING ObjectType;
    WCHAR          StringData[1];
} OBJECT_DIRECTORY_INFORMATION, *POBJECT_DIRECTORY_INFORMATION;

typedef USHORT   SECURITY_DESCRIPTOR_CONTROL, *PSECURITY_DESCRIPTOR_CONTROL;


typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER Reserved1[2];
    ULONG Reserved2;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION, *PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;
#define MAX_PROCESSOR   32

typedef struct _SYSTEM_BASIC_INFORMATION {
    CHAR Reserved1[24];
    PVOID Reserved2[4];
    CHAR NumberOfProcessors;
} SYSTEM_BASIC_INFORMATION;


typedef enum _SECTION_INFORMATION_CLASS
{
    SectionBasicInformation,
    SectionImageInformation
}SECTION_INFORMATION_CLASS;

typedef struct _SECTION_BASIC_INFORMATION
{
    PVOID BaseAddress;
    LARGE_INTEGER MaximumSize;
    ULONG AllocationAttributes;
} SECTION_BASIC_INFORMATION, *PSECTION_BASIC_INFORMATION;

typedef struct _SECTION_IMAGE_INFORMATION 
{
    ULONG     EntryPoint;
    ULONG     StackZeroBits;
    ULONG_PTR StackReserve;
    ULONG_PTR StackCommit;
    ULONG     Subsystem;
    USHORT    MinorSubsystemVersion;
    USHORT    MajorSubsystemVersion;
    ULONG     Unknown2;
    ULONG     Characteristics;
    USHORT    ImageNumber;
    BOOLEAN   Executable;
    UCHAR     Unknown3;
    ULONG     Unknown4[3];
} SECTION_IMAGE_INFORMATION, *PSECTION_IMAGE_INFORMATION;

#define SIZE_OF_TOKEN_INFORMATION                   \
sizeof( TOKEN_USER )                            \
+ sizeof( SID )                                 \
+ sizeof( ULONG ) * SID_MAX_SUB_AUTHORITIES



#define SEC_COMMIT        0x8000000

typedef enum _OBJECT_INFORMATION_CLASS
{
    ObjectBasicInformation,
    ObjectNameInformation,
    ObjectTypeInformation,
    ObjectAllTypesInformation,
    ObjectHandleFlagInformation
} OBJECT_INFORMATION_CLASS;

typedef struct _OBJECT_BASIC_INFORMATION 
{
    ULONG                   Attributes;
    ACCESS_MASK             GrantedAccess;
    ULONG                   HandleCount;
    ULONG                   PointerCount;
    ULONG                   PagedPoolCharge;
    ULONG                   NonPagedPoolCharge;
    LARGE_INTEGER   CreationTime;
    ULONG     NameInfoSize;
    ULONG     TypeInfoSize;
    ULONG     SecurityDescriptorSize;
    CHAR                    Unknown2[8];
} OBJECT_BASIC_INFORMATION, *POBJECT_BASIC_INFORMATION;

//---------------------------------------------------------------------------------------
// COMMON PROTOTYPES
//---------------------------------------------------------------------------------------


NTKERNELAPI
NTSTATUS
ObOpenObjectByName(
                   IN POBJECT_ATTRIBUTES ObjectAttributes,
                   IN POBJECT_TYPE ObjectType,
                   IN KPROCESSOR_MODE AccessMode,
                   IN OUT PACCESS_STATE PassedAccessState OPTIONAL,
                   IN ACCESS_MASK DesiredAccess OPTIONAL,
                   IN OUT PVOID ParseContext OPTIONAL,
                   OUT PHANDLE Handle
                   );

NTSYSAPI 
NTSTATUS
NTAPI
ZwCreateSection (
                 OUT PHANDLE SectionHandle,
                 IN ACCESS_MASK DesiredAccess,
                 IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
                 IN PLARGE_INTEGER MaximumSize OPTIONAL,
                 IN ULONG SectionPageProtection,
                 IN ULONG AllocationAttributes,
                 IN HANDLE FileHandle OPTIONAL
                 );

NTSYSAPI
NTSTATUS
NTAPI
ZwUnmapViewOfSection(
                     IN HANDLE ProcessHandle,
                     IN PVOID BaseAddress
                     );

NTSYSAPI
NTSTATUS
NTAPI
ZwAllocateVirtualMemory(
                        IN HANDLE ProcessHandle,
                        IN OUT PVOID *BaseAddress,
                        IN ULONG ZeroBits,
                        IN OUT PSIZE_T RegionSize,
                        IN ULONG AllocationType,
                        IN ULONG Protect
                        );

NTSYSAPI
NTSTATUS
NTAPI
ZwReadVirtualMemory (
                            IN HANDLE ProcessHandle,
                            IN PVOID BaseAddress,
                            OUT PVOID Buffer,
                            IN ULONG BufferSize,
                            OUT PULONG NumberOfBytesRead 
                            );

NTSYSAPI
NTSTATUS
NTAPI
ZwFreeVirtualMemory(
                    IN HANDLE ProcessHandle,
                    IN OUT PVOID *BaseAddress,
                    IN OUT PSIZE_T RegionSize,
                    IN ULONG FreeType
                    );
NTSYSAPI 
NTSTATUS
NTAPI
ZwQueryDirectoryFile(
                     IN HANDLE FileHandle,
                     IN HANDLE Event OPTIONAL,
                     IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                     IN PVOID ApcContext OPTIONAL,
                     OUT PIO_STATUS_BLOCK IoStatusBlock,
                     OUT PVOID FileInformation,
                     IN ULONG Length,
                     IN FILE_INFORMATION_CLASS FileInformationClass,
                     IN BOOLEAN ReturnSingleEntry,
                     IN PUNICODE_STRING FileName OPTIONAL,
                     IN BOOLEAN RestartScan
                     );

NTSYSAPI 
NTSTATUS
NTAPI
ZwQueryInformationProcess(
                          IN HANDLE ProcessHandle,
                          IN PROCESSINFOCLASS ProcessInformationClass,
                          OUT PVOID ProcessInformation,
                          IN ULONG ProcessInformationLength,
                          OUT PULONG ReturnLength OPTIONAL
                          );
NTSTATUS
ObReferenceObjectByName(
                        IN PUNICODE_STRING ObjectName,
                        IN ULONG Attributes,
                        IN PACCESS_STATE PassedAccessState OPTIONAL,
                        IN ACCESS_MASK DesiredAccess OPTIONAL,
                        IN POBJECT_TYPE ObjectType,
                        IN KPROCESSOR_MODE AccessMode,
                        IN OUT PVOID ParseContext OPTIONAL,
                        OUT PVOID *Object
                        );
NTSYSAPI
NTSTATUS
NTAPI
ZwOpenDirectoryObject(
                      OUT PHANDLE DirectoryHandle,
                      IN ACCESS_MASK DesiredAccess,
                      IN POBJECT_ATTRIBUTES ObjectAttributes
                      );
NTSYSAPI
NTSTATUS
NTAPI
ZwQueryDirectoryObject (
                        IN HANDLE       DirectoryHandle,
                        OUT PVOID       Buffer,
                        IN ULONG        Length,
                        IN BOOLEAN      ReturnSingleEntry,
                        IN BOOLEAN      RestartScan,
                        IN OUT PULONG   Context,
                        OUT PULONG      ReturnLength OPTIONAL
                        );

NTSYSAPI
NTSTATUS
NTAPI
ZwOpenSymbolicLinkObject(
                         OUT PHANDLE LinkHandle,
                         IN ACCESS_MASK DesiredAccess,
                         IN POBJECT_ATTRIBUTES ObjectAttributes
                         );

NTSYSAPI
NTSTATUS
NTAPI
ZwQuerySymbolicLinkObject(
                          IN HANDLE LinkHandle,
                          IN OUT PUNICODE_STRING LinkTarget,
                          OUT PULONG ReturnedLength OPTIONAL
                          );

NTSYSAPI
NTSTATUS
NTAPI
ZwQueryVolumeInformationFile(
                             IN HANDLE FileHandle,
                             OUT PIO_STATUS_BLOCK IoStatusBlock,
                             OUT PVOID FsInformation,
                             IN ULONG Length,
                             IN FS_INFORMATION_CLASS FsInformationClass
                             );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtOpenFile(
           OUT PHANDLE FileHandle,
           IN ACCESS_MASK DesiredAccess,
           IN POBJECT_ATTRIBUTES ObjectAttributes,
           OUT PIO_STATUS_BLOCK IoStatusBlock,
           IN ULONG ShareAccess,
           IN ULONG OpenOptions
           );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtClose(
        IN HANDLE Handle
        );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwOpenProcess (
                    OUT PHANDLE ProcessHandle,
                    IN ACCESS_MASK DesiredAccess,
                    IN POBJECT_ATTRIBUTES ObjectAttributes,
                    IN PCLIENT_ID ClientId OPTIONAL
                    );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwQuerySection(
                    IN HANDLE SectionHandle,
                    IN SECTION_INFORMATION_CLASS SectionInformationClass,
                    OUT PVOID SectionInformation,
                    IN ULONG SectionInformationLength,
                    OUT PULONG ReturnLength OPTIONAL
                    );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwFsControlFile(
                     IN HANDLE FileHandle,
                     IN HANDLE Event OPTIONAL,
                     IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                     IN PVOID ApcContext OPTIONAL,
                     OUT PIO_STATUS_BLOCK IoStatusBlock,
                     IN ULONG FsControlCode,
                     IN PVOID InputBuffer OPTIONAL,
                     IN ULONG InputBufferLength,
                     OUT PVOID OutputBuffer OPTIONAL,
                     IN ULONG OutputBufferLength
                     );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwQueryObject (
                    IN HANDLE Handle,
                    IN OBJECT_INFORMATION_CLASS ObjectInformationClass,
                    OUT PVOID ObjectInformation,
                    IN ULONG ObjectInformationLength,
                    OUT PULONG ReturnLength OPTIONAL
                    );

NTKERNELAPI 
NTSTATUS 
ObQueryNameString(PVOID Object, 
                  POBJECT_NAME_INFORMATION ObjectNameInfo, 
                  ULONG Length, 
                  PULONG ReturnLength);


NTSTATUS 
ZwOpenEvent(
            OUT PHANDLE  EventHandle,
            IN ACCESS_MASK  DesiredAccess,
            IN POBJECT_ATTRIBUTES  ObjectAttributes
            );

NTSTATUS
ZwNotifyChangeKey(
  IN HANDLE  KeyHandle,
  IN HANDLE  Event,
  IN PIO_APC_ROUTINE  ApcRoutine,
  IN PVOID  ApcContext,
  OUT PIO_STATUS_BLOCK  IoStatusBlock,
  IN ULONG  CompletionFilter,
  IN BOOLEAN  WatchTree,
  OUT PVOID  Buffer,
  IN ULONG  BufferSize,
  IN BOOLEAN  Asynchronous
  );




#define IMAGE_SIZEOF_SHORT_NAME              8

#endif
