#include "drvUnhookerDevice.h"
#include "drvCommonPortableDefs.h"
#include "drvSyncQueue.h"
#include "drvVirginity2.h"

namespace drv
{

struct UnhookerDeviceDispatch;
void ThreadWorker( UnhookerDeviceDispatch * pExtension );

// my private extension
struct UnhookerDeviceDispatch:public drv::CommonDeviceExtension
{
    void * m_pThread;
    KEVENT m_waitEvent;
    KEVENT m_stopEvent;
    CSyncQueue * m_pQueue;

    NTSTATUS Init();
    
    template <class Type>
    NTSTATUS AddTaskAndWait(drv::CAutoTask_t<Type> & task);
};


NTSTATUS UnhookerDeviceDispatch::Init()
{
    // zero variables
    KeInitializeEvent( &m_waitEvent, SynchronizationEvent, FALSE );
    KeInitializeEvent( &m_stopEvent, NotificationEvent, FALSE );
    m_pThread = 0;
    m_pQueue = 0;

    // init
    m_pQueue = new CSyncQueue;
    NT_CHECK_ALLOC(m_pQueue);
    NT_CHECK(m_pQueue->Init());

    // start thread
    HANDLE hThread = 0;
    NTSTATUS status =0;
    OBJECT_ATTRIBUTES oa;

    InitializeObjectAttributes(&oa, 0, OBJ_KERNEL_HANDLE, 0, 0);

    status = PsCreateSystemThread(&hThread, 
                                    (ACCESS_MASK) 0L,
                                    &oa,
                                    NULL,
                                    NULL,
                                    (PKSTART_ROUTINE)ThreadWorker,
                                    this
                                    );
    if (!NT_SUCCESS(status))
        return status;

    status = ObReferenceObjectByHandle( hThread, 0, NULL, KernelMode, &m_pThread, NULL);
    if (!NT_SUCCESS(status))
    {
        ZwClose( hThread );
        return status;
    }
    ZwClose( hThread );
    return STATUS_SUCCESS;
}

void UnhookerDeviceCleanupRoutine( UnhookerDeviceDispatch * pExtension )
{
    if (pExtension->m_pThread)
    {
        KeSetEvent( &pExtension->m_stopEvent, 0, FALSE );
        KeWaitForSingleObject(pExtension->m_pThread, Executive, KernelMode, FALSE, NULL);
        ObDereferenceObject(pExtension->m_pThread);
    }
    if (pExtension->m_pQueue)
    {
        delete pExtension->m_pQueue;
    }
}

template <class Type>
NTSTATUS UnhookerDeviceDispatch::AddTaskAndWait(drv::CAutoTask_t<Type> & task)
{
    NT_CHECK(m_pQueue->PushBackSharedTask(task));
    KeSetEvent( &m_waitEvent, 0, FALSE );
    task->Wait();
    return NT_OK;
}

void ThreadWorker( UnhookerDeviceDispatch * pExtension )
{
    PVOID events[2] = { &pExtension->m_waitEvent, &pExtension->m_stopEvent};
    KWAIT_BLOCK waitBlocks[2];

    while (  STATUS_WAIT_0  ==  KeWaitForMultipleObjects
                                (
                                    2, 
                                    events,
                                    WaitAny, 
                                    Executive, 
                                    KernelMode, 
                                    FALSE,
                                    NULL,
                                    waitBlocks
                                    )
          )
    {
        bool bEmpty = false;
        do
        {
            drv::CAutoTask task;
            bEmpty = true;

            NTSTATUS status = pExtension->m_pQueue->PopFirst(task, &bEmpty);
            if (!NT_SUCCESS(status))
            {
                return;
            }

            if (task.get())
                task->Execute();
        }
        while(!bEmpty);
            
    }
}

class CScanTask:public CSharedTask
{
    NTSTATUS m_status;
    PVOID m_pRtlPrefetchMemoryNonTemporal;

    Drv_VirginityContext2 m_virginityContext;

    virtual NTSTATUS DoInit() { return NT_OK; }
    virtual NTSTATUS OnSSTPatch(void ** pCurrentHandler, 
                                void * pRealHandler, 
                                int , 
                                bool * pNeedBreak)
    {
        // restore SST
        Drv_HookSST(pCurrentHandler, pRealHandler);
        *pNeedBreak = false;
        return NT_OK;
    }
    
    virtual NTSTATUS OnModification(const Drv_VirginityContext2 * pContext,
                                    bool * pNeedBreak)
    {
        const char * pMappedSectionStart = Drv_GetMappedSectionStart( pContext );
        char * pMemorySectionStart = (char * )pContext->m_currentSectionInfo.m_sectionStart;

        Drv_HookMemCpy(pContext->m_startOfModification + pMemorySectionStart, 
                       pContext->m_startOfModification + pMappedSectionStart, 
                       pContext->m_endOfModification - pContext->m_startOfModification);

        *pNeedBreak = false;
        return NT_OK;
    }

    virtual NTSTATUS ScanSST()
    {
        for(int i = 0, sstSize = Drv_GetSizeOfNtosSST(); 
            i < sstSize; 
            ++i)
        {
            void ** pCurrentHandler = Drv_GetNtosSSTEntry(i);

            void * pRealHandler = 0;
            Drv_GetRealSSTValue2(&m_virginityContext, i, &pRealHandler);
            if (pRealHandler != *pCurrentHandler)
            {
                bool needBreak = false;
                NT_CHECK(OnSSTPatch(pCurrentHandler, pRealHandler, i, &needBreak));
            }
        }
        return STATUS_SUCCESS;
    }
    
    virtual NTSTATUS ScanAllModule()
    {
        void * pStart = 0;
        int size = 0;
        NT_CHECK(Drv_GetFirstModification(&m_virginityContext, 
                            &pStart,
                            &size));
        while(pStart)
        {
            bool needBreak = false;

            // check RtlPrefetchMemoryNonTemporal
            bool bSkip = false;
            
            {
                char * pMemorySectionStart = (char * )m_virginityContext.m_currentSectionInfo.m_sectionStart;
                if (m_virginityContext.m_startOfModification + pMemorySectionStart == m_pRtlPrefetchMemoryNonTemporal
                    && m_virginityContext.m_endOfModification - m_virginityContext.m_startOfModification == 1)
                {
                    // skip it
                    bSkip = true;
                }
            }

            if (!bSkip)
            {
                NT_CHECK(OnModification(&m_virginityContext, &needBreak));
            }

            NT_CHECK( Drv_GetNextModification(&m_virginityContext, 
                                                &pStart,
                                                &size));

        }
        return STATUS_SUCCESS;
    }
    
    virtual NTSTATUS ExecuteReal()
    {
        CAutoVirginity2 initer;
        NT_CHECK(initer.Init(&m_virginityContext));

        UNICODE_STRING fncName;
        RtlInitUnicodeString(&fncName, L"RtlPrefetchMemoryNonTemporal");
        m_pRtlPrefetchMemoryNonTemporal = MmGetSystemRoutineAddress(&fncName);

        NT_CHECK(DoInit());

        NT_CHECK(ScanSST());

        NT_CHECK(ScanAllModule());

        return NT_OK;
    }
    virtual void ExecuteImpl()
    {
        m_status = ExecuteReal();
    }
protected:
    Drv_VirginityContext2 * GetVirginityContext() { return &m_virginityContext; }

public:
    CScanTask()
    {
        m_pRtlPrefetchMemoryNonTemporal = 0;
        m_status = STATUS_UNSUCCESSFUL;
    }
    NTSTATUS GetStatus()
    {
        return m_status;
    }
};

static
NTSTATUS DispatchUnhook(PDEVICE_OBJECT  pDeviceObject,
                        drv::UnhookerDeviceDispatch * pExtension,   
                        void * pBuffer, 
                        size_t size, 
                        size_t outSizeIn, 
                        size_t * pOutSizeOut)
{
    drv::CAutoTask_t<CScanTask> task(new CScanTask());
    NT_CHECK_ALLOC(task.get());
    NT_CHECK(pExtension->AddTaskAndWait(task));
    NT_CHECK(task->GetStatus());
    return STATUS_SUCCESS;
}

class CReportingScanTask:public CScanTask
{
    int m_countOfModifiedSSTEntries;

    char * m_pPackBegin;
    char * m_pPackEnd;
    char * m_pPackCur;
    DRV_REPORT * m_pReport;


    virtual NTSTATUS OnSSTPatch(void ** pCurrentHandler, 
                                void * pRealHandler, 
                                int index, 
                                bool * pNeedBreak)
    {
        *pNeedBreak = false;

        ++m_pReport->m_countOfModifiedSSTEntries;
        SYSTEM_MODULE * pModule = GetVirginityContext()->m_resolver.LookupModule(*pCurrentHandler);

        ANSI_STRING kernelAnsiName;
        if (pModule)
        {
            RtlInitAnsiString(&kernelAnsiName, (char*)pModule->abName + pModule->wNameOffset);
        }
        else
        {
            RtlInitAnsiString(&kernelAnsiName, "-unknown-");
        }


        if (m_pPackCur + kernelAnsiName.Length + sizeof(DRV_REPORT_SST_ENTRY) <= m_pPackEnd)
        {
            DRV_REPORT_SST_ENTRY * pPackedEntry = (DRV_REPORT_SST_ENTRY * )m_pPackCur;
            memset(pPackedEntry, 0, sizeof(DRV_REPORT_SST_ENTRY));
            
            pPackedEntry->m_base.m_type = dreSST;
            pPackedEntry->m_base.m_sizeOfEntry = kernelAnsiName.Length + sizeof(DRV_REPORT_SST_ENTRY);
            pPackedEntry->m_sstIndex = index;
            if (pModule)
            {
                pPackedEntry->m_moduleAddress = (size_t)pModule->pAddress;
            }
            else
            {
                pPackedEntry->m_moduleAddress = 0;
            }
            pPackedEntry->m_newAddress = (size_t)*pCurrentHandler;

            pPackedEntry->m_nameOffset = sizeof(DRV_REPORT_SST_ENTRY);
            pPackedEntry->m_nameSize = kernelAnsiName.Length;
            memcpy(pPackedEntry+1, kernelAnsiName.Buffer, kernelAnsiName.Length);
            
            ++m_pReport->m_countOfReportEntries;
            m_pPackCur += pPackedEntry->m_base.m_sizeOfEntry;
        }
        return NT_OK;
    }

    virtual NTSTATUS OnModification(const Drv_VirginityContext2 * pContext,
                                    bool * pNeedBreak)
    {
        *pNeedBreak = false;

        ++m_pReport->m_countOfModifiedImageEntries;

        if (m_pPackCur + sizeof(DRV_REPORT_MODIFICATION_ENTRY) <= m_pPackEnd)
        {
            DRV_REPORT_MODIFICATION_ENTRY * pPackedEntry = (DRV_REPORT_MODIFICATION_ENTRY * )m_pPackCur;
            memset(pPackedEntry, 0, sizeof(DRV_REPORT_MODIFICATION_ENTRY));
            
            pPackedEntry->m_base.m_type = dreModification;
            pPackedEntry->m_base.m_sizeOfEntry = sizeof(DRV_REPORT_MODIFICATION_ENTRY);
            
            char * pMemorySectionStart = (char * )pContext->m_currentSectionInfo.m_sectionStart;
            pPackedEntry->m_address = (unsigned long long)(pMemorySectionStart + pContext->m_startOfModification);

            pPackedEntry->m_offsetInSection = pContext->m_startOfModification;
            pPackedEntry->m_sizeOfModification = pContext->m_endOfModification - pContext->m_startOfModification;
            memcpy(pPackedEntry->m_sectionName, 
                   pContext->m_pMappedSection->Name, 
                   sizeof(pContext->m_pMappedSection->Name));
            
            ++m_pReport->m_countOfReportEntries;
            m_pPackCur += pPackedEntry->m_base.m_sizeOfEntry;
        }
        return NT_OK;
    }

public:
    CReportingScanTask(DRV_REPORT * pReport, size_t outSizeIn)
        : m_pReport(pReport)
    {
        memset(pReport, 0, sizeof(DRV_REPORT));
        pReport->m_countOfModifiedImageEntries = 0;
        pReport->m_countOfModifiedSSTEntries = 0;
        pReport->m_countOfReportEntries = 0;
        pReport->m_reportEntryOffsets = sizeof(DRV_REPORT);

        m_pPackBegin  = (char*)m_pReport + m_pReport->m_reportEntryOffsets;
        m_pPackCur = m_pPackBegin;
        m_pPackEnd  = (char*)m_pReport + outSizeIn;
    }

    size_t GetReportSize() const
    {
        return (char*)m_pPackCur - (char*)m_pReport;
    }
};


static
NTSTATUS DispatchGetStatus(PDEVICE_OBJECT  pDeviceObject,
                           drv::UnhookerDeviceDispatch * pExtension,   
                           void * pBuffer, 
                           size_t size, 
                           size_t outSizeIn, 
                           size_t * pOutSizeOut)
{
    if (outSizeIn < sizeof(DRV_REPORT))
    {
        return STATUS_BUFFER_OVERFLOW;
    }

    DRV_REPORT * pReport = (DRV_REPORT * )pBuffer;
    
    drv::CAutoTask_t<CReportingScanTask> task(new CReportingScanTask(pReport, outSizeIn));
    NT_CHECK_ALLOC(task.get());
    NT_CHECK(pExtension->AddTaskAndWait(task));
    NT_CHECK(task->GetStatus());

    // pack out param
    *pOutSizeOut = task->GetReportSize();;
    return STATUS_SUCCESS;
}


static 
NTSTATUS UnhookerDeviceDispatchRoutine(
                                        IN  PDEVICE_OBJECT  pDeviceObject,
						                IN  PIRP            pIrp,
                                        IN  drv::UnhookerDeviceDispatch * pExtension
                                      )
{
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(pIrp);
    switch(irpSp->MajorFunction)
    {
    case IRP_MJ_CREATE: return CompleteIrp(pIrp);
    case IRP_MJ_CLOSE: return CompleteIrp(pIrp);
    case IRP_MJ_DEVICE_CONTROL:
        {
            NTSTATUS status = 0;
            size_t minor = irpSp->Parameters.DeviceIoControl.IoControlCode;
            size_t outSizeIn = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
            size_t outSizeOut = 0;

            switch(minor)
            {
            case DRV_UNHOOKER_GET_STATUS:
                {
                status = DispatchGetStatus(pDeviceObject,
                                        pExtension,
                                        pIrp->AssociatedIrp.SystemBuffer,
                                        irpSp->Parameters.DeviceIoControl.InputBufferLength,
                                        outSizeIn,
                                        &outSizeOut);
                break;
                }
            case DRV_UNHOOKER_UNHOOK:
                {
                status = DispatchUnhook(pDeviceObject,
                                        pExtension,
                                        pIrp->AssociatedIrp.SystemBuffer,
                                        irpSp->Parameters.DeviceIoControl.InputBufferLength,
                                        outSizeIn,
                                        &outSizeOut);
                break;
                }
            default:
                status = STATUS_INVALID_DEVICE_REQUEST;
            }
            return CompleteIrp(pIrp, status, outSizeOut);
        }
    };
    return CompleteIrp(pIrp, STATUS_INVALID_DEVICE_REQUEST);
}


NTSTATUS CreateUnhookerDevice(IN PDRIVER_OBJECT   pDriverObject, 
                              OUT drv::CDeviceOwner & deviceObject)
{
    UNICODE_STRING devName;
    RtlInitUnicodeString(&devName, DRV_UNHOOKER_DEVICE_NAME);

    NT_CHECK(IoCreateDevice(pDriverObject,
                            sizeof(drv::UnhookerDeviceDispatch),
                            &devName,
                            FILE_DEVICE_UNKNOWN,
                            0,
                            FALSE,
                            deviceObject.GetPtr2()));


    drv::UnhookerDeviceDispatch * pExtension = InitCommonDeviceExtension(deviceObject.get(), 
                                                                         UnhookerDeviceDispatchRoutine,
                                                                         UnhookerDeviceCleanupRoutine
                                                                         );
    NT_CHECK(pExtension->Init());

  
   
    // create symlink
    UNICODE_STRING devSymLink;
    RtlInitUnicodeString(&devSymLink, DRV_UNHOOKER_DEVICE_SYMBOLIC_LINK);

    NT_CHECK(IoCreateSymbolicLink(&devSymLink, &devName));
    return STATUS_SUCCESS;
}


}
