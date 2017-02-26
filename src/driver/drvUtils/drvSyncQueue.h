#ifndef DRV_SYNC_QUEUE_H
#define DRV_SYNC_QUEUE_H

#include "drvSync.h"

namespace drv
{

class CCommonTask
{
    CCommonTask(const  CCommonTask&);
    CCommonTask&operator =(const  CCommonTask&);
public:
    LIST_ENTRY m_entry;
    CCommonTask()
    {
        m_entry.Blink = 0;
        m_entry.Flink = 0;
    }
    virtual ~CCommonTask()
    {
    }
    virtual void Execute()=0;
    virtual void Cleanup()=0;
};

class CAutoTask
{
    CCommonTask * m_pCommonTask;

    CAutoTask(const CAutoTask&);
    CAutoTask&operator = (const CAutoTask&);
public:
    CAutoTask(CCommonTask * pCommonTask = 0)
        : m_pCommonTask(pCommonTask)
    {
    }
    CCommonTask * get ()
    {
        return m_pCommonTask;
    }
    const CCommonTask * get ()const 
    {
        return m_pCommonTask;
    }

    CCommonTask * operator -> ()
    {
        return m_pCommonTask;
    }
    const CCommonTask * operator -> ()const 
    {
        return m_pCommonTask;
    }
    CCommonTask * release()
    {
        CCommonTask * pCommonTask = m_pCommonTask;
        m_pCommonTask = 0;
        return pCommonTask;
    }

    void reset(CCommonTask * pTask)
    {
        if (pTask == m_pCommonTask)
            return;

        CCommonTask * pCommonTask = m_pCommonTask;
        m_pCommonTask = pTask;
        if (pCommonTask)
            pCommonTask->Cleanup();
    }
    ~CAutoTask()
    {
        if (m_pCommonTask)
            m_pCommonTask->Cleanup();
    }
};

template<class CommonTaskType>
class CAutoTask_t
{
    CAutoTask m_impl;
public:
    CAutoTask_t(CommonTaskType * pCommonTask = 0)
        : m_impl(pCommonTask)
    {
    }

    CommonTaskType * get ()
    {
        return (CommonTaskType *)m_impl.get();
    }
    const CommonTaskType * get ()const 
    {
        return (CommonTaskType *)m_impl.get();
    }

    CommonTaskType * operator -> ()
    {
        return (CommonTaskType *)m_impl.get();
    }
    const CommonTaskType * operator -> ()const 
    {
        return (CommonTaskType *)m_impl.get();
    }
    CommonTaskType * release()
    {
        return (CommonTaskType *)m_impl.release();
    }

    void reset(CommonTaskType * pTask)
    {
        m_impl.reset(pTask);
    }
};

class CSharedTask:public CCommonTask
{
    KEVENT m_event;
    long m_counter;

    virtual void ExecuteImpl()=0;
    virtual void CleanupImpl() { delete this; }
public:
    CSharedTask()
        : m_counter(1)
    {
        KeInitializeEvent( &m_event, NotificationEvent, FALSE );
    }
    void AddRef()
    {
        InterlockedIncrement(&m_counter);
    }
    void Wait()
    {
        KeWaitForSingleObject(
                          &m_event,
                          Executive, 
                          KernelMode,
                          FALSE,
                          NULL
                          );
    }
    virtual void Execute()
    {
        ExecuteImpl();
        KeSetEvent( &m_event, 0, FALSE );
    }
    virtual void Cleanup()
    {
        KeSetEvent( &m_event, 0, FALSE );
        if (InterlockedDecrement(&m_counter)==0)
            CleanupImpl();
    }
};

class CSyncQueue
{
    long m_destroyed;
    LIST_ENTRY m_list;
    FastMutex m_lock;

    void DestroyNoLock();
public:
    CSyncQueue();
    ~CSyncQueue();
    NTSTATUS Init();
    void Destroy();
    NTSTATUS PushBack(CAutoTask & task);
    NTSTATUS PopFirst(CAutoTask & task, 
                      bool * pEmpty);


    template <class Type>
    NTSTATUS PushBackSharedTask(CAutoTask_t<Type> & task)
    {
        drv::AutoFastMutex guard(m_lock);
        if (m_destroyed)
            return STATUS_LOCAL_DISCONNECT;

        task->AddRef();
        InsertTailList(&m_list, &task->m_entry);
        return NT_OK;
    }

};


} //drv
#endif