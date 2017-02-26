#ifndef CMN_ERRORS_H
#define CMN_ERRORS_H

#include "windows.h"
#include "string"
#include "vector"
#include "exception"

namespace cmn
{
    class CWinException:public std::runtime_error
    {
        DWORD m_dwErrorCode;
    public:
        CWinException(const std::string & str, DWORD dwErrorCode)
            : std::runtime_error(str), m_dwErrorCode(dwErrorCode)
        {
        }

        DWORD GetErrorCode() const
        {
            return m_dwErrorCode;
        }
    };

    inline std::wstring GetErrorText(DWORD dwError, DWORD dwLangId= MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT))
    {
          HLOCAL hlocal = NULL;   // Buffer that gets the error message string

          // Get the error code's textual description
          BOOL fOk = FormatMessageW(
             FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
             NULL, dwError, dwLangId,
             (LPWSTR) &hlocal, 0, NULL);

          if (!fOk) {
             // Is it a network-related error?
             HMODULE hDll = LoadLibraryEx(TEXT("netmsg.dll"), NULL,
                DONT_RESOLVE_DLL_REFERENCES);

             if (hDll != NULL) {
                FormatMessageW(
                   FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM,
                   hDll, dwError, dwLangId,
                   (LPWSTR) &hlocal, 0, NULL);
                FreeLibrary(hDll);
             }
          }

          if (hlocal != NULL)
          {
              std::wstring sErr=  (const wchar_t*) LocalLock(hlocal);
              LocalFree(hlocal);
              return sErr;
          }
          else
          {
              return L"Unknown error";
          }
    }

    
#define CMN_THROW_WIN32(X)  {  DWORD dwLastError = GetLastError(); throw cmn::CWinException(X, dwLastError); }
}
#endif