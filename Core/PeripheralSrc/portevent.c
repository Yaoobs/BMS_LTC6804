#include "port.h"
#include "mb.h"
#include "mbport.h"
#include "main.h"

static volatile eMBEventType eQueuedEvent;
static volatile BOOL xEventInQueue;
static uint32_t primaskSave;

void vMBPortEnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    primaskSave = primask;
}

void vMBPortExitCritical(void)
{
    __set_PRIMASK(primaskSave);
}

BOOL xMBPortEventInit(void)
{
    xEventInQueue = FALSE;
    return TRUE;
}

BOOL xMBPortEventPost(eMBEventType eEvent)
{
    vMBPortEnterCritical();
    eQueuedEvent = eEvent;
    xEventInQueue = TRUE;
    vMBPortExitCritical();
    return TRUE;
}

BOOL xMBPortEventGet(eMBEventType *eEvent)
{
    BOOL xEventHappened = FALSE;

    vMBPortEnterCritical();
    if (xEventInQueue)
    {
        *eEvent = eQueuedEvent;
        xEventInQueue = FALSE;
        xEventHappened = TRUE;
    }
    vMBPortExitCritical();
    return xEventHappened;
}
