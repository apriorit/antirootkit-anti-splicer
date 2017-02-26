#ifndef CMN_GUARDS_H
#define CMN_GUARDS_H

#include "windows.h"
#include "winsvc.h"
namespace cmn
{
    template<class ImplType>
    class CBaseGuard
    {
        typename ImplType::TypeName m_handle;

        CBaseGuard(const CBaseGuard&);
        CBaseGuard& operator=(const CBaseGuard&);
    public:
        explicit CBaseGuard(typename ImplType::TypeName handle=0)
            :m_handle(handle)
        {
        }
        ~CBaseGuard()
        {
            if(m_handle)
                ImplType::Close(m_handle);
        }
        typename ImplType::TypeName get() const 
        {
            return m_handle;
        }
        typename ImplType::TypeName release()
        {
            typename ImplType::TypeName tmp = m_handle;
            m_handle = 0;
            return tmp;
        }
        void reset(typename ImplType::TypeName handle)
        {
            if(m_handle)
                ImplType::Close(m_handle);
            m_handle = handle;
        }
    };

    // HandleGuard
    struct HandleImpl
    {
        typedef HANDLE TypeName;
        static void Close(HANDLE h)
        {
            CloseHandle(h);
        }
    };
    typedef CBaseGuard<HandleImpl> HandleGuard;

    // SCHandleGuard
    struct SCHandleImpl
    {
        typedef SC_HANDLE TypeName;
        static void Close(SC_HANDLE h)
        {
            CloseServiceHandle(h);
        }
    };
    typedef CBaseGuard<SCHandleImpl> SCHandleGuard;


} // cmn
#endif

