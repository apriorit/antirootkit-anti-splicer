#ifndef DRV_VIRGINITY2_H
#define DRV_VIRGINITY2_H

#include "drvVirginity.h"

extern "C"
{
#include "pe_utils.h"
}

#define DRV_SECTIONS_COUNT    2

typedef struct _Drv_VirginityContext2
{
    Drv_VirginityContext m_parent;

    Drv_Resolver m_resolver;

    DrvFindSectionInfo m_currentSectionInfo;

    int m_startOfModification;
    int m_endOfModification;
    PIMAGE_SECTION_HEADER m_pMappedSection;

    STRING m_sectionsToCheck[DRV_SECTIONS_COUNT];
}Drv_VirginityContext2;


NTSTATUS Drv_InitVirginityContext2(Drv_VirginityContext2 * pContext);
void Drv_FreeVirginityContext2(Drv_VirginityContext2 * pContext);

NTSTATUS Drv_GetFirstModification(Drv_VirginityContext2 * pContext, 
                                void ** ppStart,
                                int * pSize);
NTSTATUS Drv_GetNextModification(Drv_VirginityContext2 * pContext, 
                                void ** ppStart,
                                int * pSize);

const char * Drv_GetMappedSectionStart(const Drv_VirginityContext2 * pContext);

void Drv_GetRealSSTValue2(Drv_VirginityContext2 * pContext, long index, void ** ppValue);

class CAutoVirginity2
{
    Drv_VirginityContext2 * m_pContext;

    CAutoVirginity2(const CAutoVirginity2&);
    CAutoVirginity2&operator = (const CAutoVirginity2&);
public:
    CAutoVirginity2()
        : m_pContext(0)
    {
    }
    NTSTATUS Init(Drv_VirginityContext2 * pContext)
    {
        NT_CHECK(Drv_InitVirginityContext2(pContext));
        m_pContext = pContext;
        return NT_OK;
    }
    ~CAutoVirginity2()
    {
        if (m_pContext)
            Drv_FreeVirginityContext2(m_pContext);
    }
};


#endif