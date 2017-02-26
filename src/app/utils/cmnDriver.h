#ifndef CMN_DRIVER_H
#define CMN_DRIVER_H

#include "cmnGuards.h"
#include "cmnErrors.h"


namespace cmn
{

class CDriver
{
    HandleGuard m_handle;
    
public:
    CDriver()
    {
    }
	CDriver(const std::wstring & name)
    {
        Connect(name);
    }

    // bug here
    void SendIoCtl(DWORD dwCode, LPVOID pBuffer, DWORD BufferSize, DWORD * pResBufferSize)
    {
        if (!m_handle.get())
            throw std::exception("Driver.SendIoCtl.NotConnected");

        BOOL result = DeviceIoControl( m_handle.get(), 
                                   dwCode, 
                                   pBuffer, 
                                   BufferSize, 
                                   pBuffer, 
                                   BufferSize, 
                                   pResBufferSize, 
                                   NULL );
        if (!result)
        {
            throw CWinException("Driver.SendIoCtl.CallFailed", GetLastError());
        }
    }
    void SendIoCtl(DWORD dwCode, LPVOID pBuffer, DWORD MaxBufferSize, DWORD InBufferSize, DWORD * pResBufferSize)
    {
        if (!m_handle.get())
            throw std::exception("Driver.SendIoCtl.NotConnected");

        BOOL result = DeviceIoControl( m_handle.get(), 
                                   dwCode, 
                                   pBuffer, 
                                   InBufferSize, 
                                   pBuffer, 
                                   MaxBufferSize, 
                                   pResBufferSize, 
                                   NULL );
        if (!result)
        {
            throw CWinException("Driver.SendIoCtl.CallFailed", GetLastError());
        }
    }
    bool IsConnected() const
    {
        return m_handle.get() != 0;
    }
    bool Connect(const std::wstring & name, bool bSilent=false)
    {
        HANDLE hdev = CreateFileW( name.c_str(), 
                                       GENERIC_READ | GENERIC_WRITE, 
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,    
                                       NULL, 
                                       OPEN_EXISTING, 
                                       0, 
                                       NULL);

        if (hdev == INVALID_HANDLE_VALUE)
        {
            if (!bSilent)
                throw CWinException("Driver.Connect.ConnectFailed", GetLastError());
            return false;
        }
        m_handle.reset( hdev );
        return true;
    }
	void CreateAndStartService(const std::wstring &serviceName, const std::wstring & fullFileName)
	{
		SC_HANDLE scm = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
		if(scm == NULL)
		{
            throw CWinException("Driver.Install.OpenSCManagerFailed", GetLastError());
		}
		SCHandleGuard scmGuard(scm);
		
        // create service
        SC_HANDLE service =
			CreateServiceW ( scm,  
							serviceName.c_str(),      
							serviceName.c_str(),      
							SERVICE_ALL_ACCESS,
							SERVICE_KERNEL_DRIVER,
							SERVICE_DEMAND_START, 
							SERVICE_ERROR_NORMAL, 
							fullFileName.c_str(),         
							NULL,NULL, NULL, NULL, NULL);
		if (service == NULL)
		{
			DWORD err = GetLastError();
			if (err == ERROR_SERVICE_EXISTS)
			{
				service = OpenServiceW(scm, serviceName.c_str(), SERVICE_ALL_ACCESS);
				if (service == NULL)
				{
                    throw CWinException("Driver.Install.OpenServiceFailed", GetLastError());
				}
			}
			else 
			{
                throw CWinException("Driver.Install.CreateServiceFailed", GetLastError());
			}
		}
		SCHandleGuard serviceGuard(service);
		BOOL ret =  StartService( service,
					              0,       
						          NULL  );
		if (!ret) 
		{
			DWORD err = GetLastError();
			if (err == ERROR_SERVICE_ALREADY_RUNNING)
				ret = TRUE; 
			else 
			{ 
                throw CWinException("Driver.Install.StartFailed", GetLastError());
			}
		}
	}
};


inline void DeleteServiceSync(const std::wstring & svc_name)
{
	SC_HANDLE scm = OpenSCManager(NULL,
                                  NULL,
                                  SC_MANAGER_ALL_ACCESS);
	if(scm == NULL)
	{
        throw CWinException("Driver.DeleteServiceSync.OpenSCManagerFailed", GetLastError());
	}
	SCHandleGuard scmGuard(scm);
    SC_HANDLE serviceHandle = OpenServiceW(scm, 
                                    svc_name.c_str(), 
                                    SERVICE_ALL_ACCESS);
	
    if (serviceHandle == NULL)
	{
        if (GetLastError()==ERROR_SERVICE_DOES_NOT_EXIST)
            return;
        CMN_THROW_WIN32("Driver.DeleteServiceSync.OpenServiceW");
	}
    SCHandleGuard serviceGuard(serviceHandle);

    DWORD retValue = 0;
    SERVICE_STATUS ss;
    int ret = ControlService(serviceHandle, SERVICE_CONTROL_STOP, &ss);
    while (true)
    {        
        SERVICE_STATUS status = {0};
        if (!QueryServiceStatus(serviceHandle , &status))
            CMN_THROW_WIN32("Driver.DeleteServiceSync.QueryServiceStatus");

        if (status.dwCurrentState == SERVICE_STOPPED)  
            break;

        Sleep(500);
    }
    if (!DeleteService(serviceHandle))
        CMN_THROW_WIN32("Driver.DeleteServiceSync.DeleteService");
}

inline bool GetServiceStatus(const std::wstring & svc_name, SERVICE_STATUS * pStatus)
{
        memset(pStatus, 0, sizeof(*pStatus));
		SC_HANDLE scm = OpenSCManager(NULL,
                                      NULL,
                                      SC_MANAGER_CONNECT|
                                      SC_MANAGER_ENUMERATE_SERVICE|
                                      SC_MANAGER_QUERY_LOCK_STATUS|
                                      STANDARD_RIGHTS_READ);
		if(scm == NULL)
		{
            throw CWinException("Driver.IsServiceInstalled.OpenSCManagerFailed", GetLastError());
		}
		SCHandleGuard scmGuard(scm);
        SC_HANDLE service = OpenServiceW(scm, 
                                        svc_name.c_str(), 
                                        READ_CONTROL|
                                        SERVICE_ENUMERATE_DEPENDENTS|
                                        SERVICE_INTERROGATE|
                                        SERVICE_QUERY_CONFIG|
                                        SERVICE_QUERY_STATUS|
                                        SERVICE_USER_DEFINED_CONTROL);
		
        if (service == NULL)
		{
            return false;
		}
        SCHandleGuard serviceGuard(service);

        BOOL bRes = QueryServiceStatus(service, pStatus);
        if (!bRes)
		{
            throw CWinException("Driver.IsServiceInstalled.QueryServiceStatus", GetLastError());
        }
        return true;
}

inline bool CmnStartService(const std::wstring & svc_name)
{
		SC_HANDLE scm = OpenSCManager(NULL,
                                      NULL,
                                      SC_MANAGER_CONNECT|
                                      SC_MANAGER_ENUMERATE_SERVICE|
                                      SC_MANAGER_QUERY_LOCK_STATUS|
                                      STANDARD_RIGHTS_READ);
		if(scm == NULL)
		{
            throw CWinException("Driver.IsServiceInstalled.OpenSCManagerFailed", GetLastError());
		}
		SCHandleGuard scmGuard(scm);
        SC_HANDLE service = OpenServiceW(scm, 
                                        svc_name.c_str(), 
                                        READ_CONTROL|
                                        SERVICE_ENUMERATE_DEPENDENTS|
                                        SERVICE_INTERROGATE|
                                        SERVICE_QUERY_CONFIG|
                                        SERVICE_QUERY_STATUS|
                                        SERVICE_USER_DEFINED_CONTROL|
                                        SERVICE_START);
		
        if (service == NULL)
		{
            return false;
		}
        SCHandleGuard serviceGuard(service);

        
        BOOL bRes = StartService(service, 0, 0);
        if (!bRes)
		{
            if (GetLastError()!=ERROR_SERVICE_ALREADY_RUNNING)
                throw CWinException("Driver.StartService", GetLastError());
        }
        return true;
}

inline bool CmnContinueService(const std::wstring & svc_name)
{
		SC_HANDLE scm = OpenSCManager(NULL,
                                      NULL,
                                      SC_MANAGER_CONNECT|
                                      SC_MANAGER_ENUMERATE_SERVICE|
                                      SC_MANAGER_QUERY_LOCK_STATUS|
                                      STANDARD_RIGHTS_READ);
		if(scm == NULL)
		{
            throw CWinException("Driver.IsServiceInstalled.OpenSCManagerFailed", GetLastError());
		}
		SCHandleGuard scmGuard(scm);
        SC_HANDLE service = OpenServiceW(scm, 
                                        svc_name.c_str(), 
                                        READ_CONTROL|
                                        SERVICE_ENUMERATE_DEPENDENTS|
                                        SERVICE_INTERROGATE|
                                        SERVICE_QUERY_CONFIG|
                                        SERVICE_QUERY_STATUS|
                                        SERVICE_USER_DEFINED_CONTROL|
                                        SERVICE_CONTROL_CONTINUE);
		
        if (service == NULL)
		{
            return false;
		}
        SCHandleGuard serviceGuard(service);

        
        SERVICE_STATUS status;
        BOOL bRes = ControlService(service, SERVICE_CONTROL_CONTINUE, &status);
        if (!bRes)
		{
            throw CWinException("Driver.ContinueService", GetLastError());
        }
        return true;
}

struct IServiceStateObserver
{
    virtual ~IServiceStateObserver(){}
    virtual void on_service_continue_pending()=0; // the service continue is pending. 
    virtual void on_service_pause_pending()=0; // the service pause is pending. 
    virtual void on_service_paused()=0; // the service is paused. 
    virtual void on_service_running()=0; // the service is running. 
    virtual void on_service_start_pending()=0; // the service is starting. 
    virtual void on_service_stop_pending()=0; //the service is stopping. 
    virtual void on_service_stopped()=0; 
    virtual void on_invalid_status()=0; 
};



inline void DispatchServiceState(DWORD dwState, IServiceStateObserver * pObserver)
{
    switch(dwState)
    {
    case SERVICE_CONTINUE_PENDING:
        pObserver->on_service_continue_pending();
        return;
    case SERVICE_PAUSE_PENDING:
        pObserver->on_service_pause_pending();
        return;
    case SERVICE_PAUSED:
        pObserver->on_service_paused();
        return;
    case SERVICE_RUNNING:
        pObserver->on_service_running();
        return;
    case SERVICE_START_PENDING:
        pObserver->on_service_start_pending();
        return;
    case SERVICE_STOP_PENDING:
        pObserver->on_service_stop_pending();
        return;
    case SERVICE_STOPPED:
        pObserver->on_service_stopped();
        return;
    default:
        pObserver->on_invalid_status();
        return;
    }
}


struct GetNameServiceStateObserver:IServiceStateObserver
{
    std::wstring m_status;

    virtual void on_service_continue_pending()
    {
        m_status = L"continue_pending";
    }
    virtual void on_service_pause_pending()
    {
        m_status = L"pause_pending";
    }
    virtual void on_service_paused()
    {
        m_status = L"paused";
    }
    virtual void on_service_running()
    {
        m_status = L"service_running";
    }
    virtual void on_service_start_pending()
    {
        m_status = L"start_pending";
    }
    virtual void on_service_stop_pending()
    {
        m_status = L"stop_pending";
    }
    virtual void on_service_stopped()
    {
        m_status = L"stopped";
    }
    virtual void on_invalid_status()
    {
        m_status = L"invalid";
    }
};

inline std::wstring GetServiceStatusDefaultName(DWORD dwStatus)
{
    GetNameServiceStateObserver observer;
    DispatchServiceState(dwStatus, &observer);
    return observer.m_status;
}


inline std::wstring SymLinkUndecorate(const std::wstring & wstr)
{
    size_t to_skip = 0;
    for(;to_skip<wstr.size(); ++to_skip)
    {
        if (wstr[to_skip] == L'.')
            continue;
        if (wstr[to_skip] == L'\\')
            continue;
        
        std::wstring res(wstr.begin()+to_skip, wstr.end());
        return res;
    }
    return L"";
}

inline std::wstring AddBraces(const std::wstring & wstr)
{
    return L"\""+wstr+L"\"";
}


 class CServiceStarter:public IServiceStateObserver
{
    std::wstring m_service;

    virtual void on_service_continue_pending()
    {
        throw std::exception("Services.CannotStartService.InvalidServiceStatus");
    }
    virtual void on_service_pause_pending()
    {
        throw std::exception("Services.CannotStartService.InvalidServiceStatus");
    }
    virtual void on_service_paused()
    {
        CmnContinueService(m_service);
    }
    virtual void on_service_running()
    {
    }
    virtual void on_service_start_pending()
    {
        throw std::exception("Services.CannotStartService.InvalidServiceStatus");
    }
    virtual void on_service_stop_pending()
    {
        throw std::exception("Services.CannotStartService.InvalidServiceStatus");
    }
    virtual void on_service_stopped()
    {
        CmnStartService(m_service);
    }
    virtual void on_invalid_status()
    {
        throw std::exception("Services.CannotStartService.InvalidServiceStatus");
    }
public:
    CServiceStarter(const std::wstring & service)
        : m_service(service)
    {
    }
    void Start()
    {
        SERVICE_STATUS status;
        if (!GetServiceStatus(m_service, &status))
        {
            throw std::exception("Services.ServiceNotFound");
        }
        DispatchServiceState(status.dwCurrentState, this);
    }
};


} // cmn
#endif