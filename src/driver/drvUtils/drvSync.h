#ifndef DRV_SYNC_H
#define DRV_SYNC_H

#include "drvCpp.h"

namespace drv
{

template<class ResourceType>
class auto_ptr
{
    ResourceType * m_pResource;
public:
    auto_ptr()
        : m_pResource(0)
    {
    }

    explicit auto_ptr(ResourceType * pResource)
        : m_pResource( pResource )
    {
    }
    auto_ptr(auto_ptr & source_ptr)
        : m_pResource( source_ptr.get() )
    {
        source_ptr.m_pResource = 0;
    }

    auto_ptr & operator = (auto_ptr & source_ptr)
    {
        if (m_pResource)
        delete m_pResource;

        m_pResource = source_ptr.get();
        source_ptr.m_pResource = 0;
        return *this;
    }

    ~auto_ptr()
    {
        if (m_pResource)
        delete m_pResource;
    }

    void reset(ResourceType * pResource = 0)
    {
        if (m_pResource)
        delete m_pResource;
        m_pResource = pResource;
    }
    const ResourceType * get() const { return m_pResource; }
    ResourceType * get() { return m_pResource; }

    ResourceType * release()
    {
        ResourceType * pRes = m_pResource;
        m_pResource = 0;
        return pRes;
    }

    ResourceType * operator -> ()   {   return m_pResource;    }
    const ResourceType * operator -> () const {   return m_pResource;    }
    ResourceType & operator * ()   {   return *m_pResource;    }
    const ResourceType & operator * () const {   return *m_pResource;    }
};


class FastMutex
{
	drv::auto_ptr<FAST_MUTEX> pFastMutex_;
	FastMutex(const FastMutex& );
	FastMutex& operator = (const FastMutex& );
public:
    FastMutex()
    {
    }

	NTSTATUS Init()
	{
        FAST_MUTEX * p = new FAST_MUTEX;
        NT_CHECK_ALLOC(p);
        pFastMutex_.reset(p);
        ExInitializeFastMutex(pFastMutex_.get());
        return NT_OK;
	}
    FAST_MUTEX * get()
    {
        return pFastMutex_.get();
    }
    void enter()
    {
	    ExAcquireFastMutex(pFastMutex_.get());
    }
    void leave()
    {
	    ExReleaseFastMutex(pFastMutex_.get());
    }
};

template<class Guard>
class AutoGuard
{
    Guard & guard_;
    AutoGuard();
    AutoGuard(AutoGuard&);
    AutoGuard& operator = (const AutoGuard& );
public:
    explicit AutoGuard(Guard& guard)
        :guard_(guard)
    {
        KeEnterCriticalRegion();
        guard_.enter();
    }
    ~AutoGuard()
    {
        guard_.leave();
        KeLeaveCriticalRegion();
    }
};

typedef AutoGuard<FastMutex>   AutoFastMutex;



} // drv
#endif
