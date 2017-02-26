#include "drvSyncQueue.h"

namespace drv
{

CSyncQueue::CSyncQueue()
   : m_destroyed(0)
{
}

CSyncQueue::~CSyncQueue()
{
    DestroyNoLock();
}

void CSyncQueue::Destroy()
{
    CAutoTask taskGuard;
    DestroyNoLock();
}

void CSyncQueue::DestroyNoLock()
{
    CAutoTask taskGuard;

    if (InterlockedIncrement(&m_destroyed) != 1)
        return;

    PLIST_ENTRY listEntry;
    for ( listEntry = m_list.Flink;
          listEntry != &m_list;
          listEntry = listEntry->Flink ) 
    {
        CCommonTask * pTask = CONTAINING_RECORD(listEntry, CCommonTask, m_entry);
        taskGuard.reset(pTask);
    }
}

NTSTATUS CSyncQueue::Init()
{
    InitializeListHead( &m_list );
    NT_CHECK(m_lock.Init());
    return NT_OK;
}

NTSTATUS CSyncQueue::PushBack(CAutoTask & task)
{
    drv::AutoFastMutex guard(m_lock);
    if (m_destroyed)
        return STATUS_LOCAL_DISCONNECT;

    InsertTailList(&m_list, &task->m_entry);
    task.release();
    return NT_OK;
}

NTSTATUS CSyncQueue::PopFirst(CAutoTask & task, 
                               bool * pEmpty)
{
    pEmpty = false;
    drv::AutoFastMutex guard(m_lock);

    if (m_destroyed)
        return STATUS_LOCAL_DISCONNECT;

    if (IsListEmpty(&m_list))
    {
        *pEmpty = true;
        return NT_OK;
    }

    CCommonTask * pTask = CONTAINING_RECORD(m_list.Flink, CCommonTask, m_entry);
    task.reset(pTask);
    RemoveEntryList( &pTask->m_entry );
    return NT_OK;
}



}