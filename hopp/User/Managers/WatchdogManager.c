//////////////////////////////////////////////////////////////////////////////
///
///                Copyright(c) 2017 FlexMedical
///
///                   *** Confidential Company Proprietary ***
///
/// \file WatchdogManager.c
///
/// \brief Manages internal and external watchdogs
///
///
////////////////////////////////////////////////////////////////////////////////

#include "CommonTypes.h"
#include "GPIO.h"
#include "SafetyMonitor.h"
#include "WatchdogDriver.h"
#include "WatchdogManager.h"
#include "Platform.h"
#include "MemoryMap.h"

////////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND #DEFINES
////////////////////////////////////////////////////////////////////////////////
#define WDOG_CHECK_PERIOD_MS     ( IWDG_RELOAD_MS / 2 )          // ms
#define WDOG_CHECK_HZ            ( MS_PER_SEC / WDOG_CHECK_PERIOD_MS ) // Hz
#define WD_TASK_STK_SIZE      ( 256u )

////////////////////////////////////////////////////////////////////////////////
// ENUMS, TYPEDEFS, AND STRUCTURES
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    uint32_t Unused;
}WD_MSG_T;

typedef enum
{
    WD_CHECKED_IN,
    WD_FAILED_CHECK,
    WD_IGNORE_TIMEOUT
}WD_CHECK_IN_T;

////////////////////////////////////////////////////////////////////////////////
// Global
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Local Variables
////////////////////////////////////////////////////////////////////////////////
static OS_EVENT    *s_WDqueue;
static WD_MSG_T s_WDmsgPool[MIN_MSG_COUNT];
static void        *s_WDqueueBuf[MIN_MSG_COUNT];
static OS_STK s_WDTaskStk[WD_TASK_STK_SIZE];
static WD_CHECK_IN_T s_WatchdogCheckIn[ NUM_OF_WD ];
static OS_EVENT *s_WDmutex;

////////////////////////////////////////////////////////////////////////////////
// Prototypes
////////////////////////////////////////////////////////////////////////////////
static ERROR_T WD_Init( void );
static void WD_Handler( void );
static void WD_Trace( ERROR_T errMsg_, int line_ );
static ERROR_T WD_WatchdogCheck( WATCHDOG_T group_, bool *taskCheckedIn_ );
static ERROR_T WD_PreStart( void );

static TF_DESC_T s_WdTaskDesc =
{
    .NAME = "WD_Task",
    .ptos = &s_WDTaskStk[WD_TASK_STK_SIZE - 1],
    .prio = (int)WD_TASK_PRIO,
    .pbos = s_WDTaskStk,
    .stk_size = WD_TASK_STK_SIZE,
    .taskId = WDOG,
    .queue = &s_WDqueue,
    .queueBuf = s_WDqueueBuf,
    .msgPool = (uint8_t *)s_WDmsgPool,
    .msgCount = (uint16_t)MIN_MSG_COUNT,
    .maxQdepth = 0,
    .msgSize = (uint16_t)sizeof(WD_MSG_T),
    .PreStart = WD_PreStart,
    .PostStart = WD_Init,
    .MsgHandler = NULL,
    .TimeoutHandler = WD_Handler,
    .msgTimeoutMS = WDOG_CHECK_PERIOD_MS,
    .MemPtr = NULL,
    .WdgId = WD_WDOG
};

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////
static ERROR_T WD_PreStart( void )
{
    uint8_t osErr;

    s_WDmutex = OSMutexCreate( WD_MUTEX_PRIO, (INT8U*)&osErr );
    
    if( osErr != OS_ERR_NONE )
    {
        WD_Trace( ERR_OS, __LINE__ );
        return ERR_OS;
    }

    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
static ERROR_T WD_Init ( void )
{
    ERROR_T status;
    WD_MSG_T msg = {0}; // use of WD_MSG_T prevents lint warning. unused for now.
    UNUSED( msg.Unused );

    status = WDG_RuntimCfg();
    WD_Trace( status, __LINE__ );

    return status;
}

////////////////////////////////////////////////////////////////////////////////
static void WD_Handler ( void )
{
    static uint32_t s_DecimationCounter = 0;
    bool tasksCheckedIn = TRUE;

    ERROR_T status = WDG_Reload();
    WD_Trace( status, __LINE__ );

    s_DecimationCounter++;

    if( s_DecimationCounter % WDOG_CHECK_HZ == 0 ) 
    {
        // Test the 1 second task flags
        status = WD_WatchdogCheck( WD_1_SEC, &tasksCheckedIn );
        WD_Trace( status, __LINE__ );
    }

    if ( ( s_DecimationCounter % ( 5 * WDOG_CHECK_HZ ) == 0 ) && tasksCheckedIn )
    {
        // Test the 5 second task flags
        status = WD_WatchdogCheck( WD_5_SEC, &tasksCheckedIn );
        WD_Trace( status, __LINE__ );
    }

    if ( ( s_DecimationCounter % ( 10 * WDOG_CHECK_HZ ) == 0 ) && tasksCheckedIn )
    {
        // Test the 10 second task flags
        status = WD_WatchdogCheck( WD_10_SEC, &tasksCheckedIn );
        WD_Trace( status, __LINE__ );
    }

    if ( ( s_DecimationCounter % ( 45 * WDOG_CHECK_HZ ) == 0 ) && tasksCheckedIn )
    {
        // Test the 45 second task flags
        status = WD_WatchdogCheck( WD_45_SEC, &tasksCheckedIn );
        WD_Trace( status, __LINE__ );
    }

    if ( ( s_DecimationCounter % ( 90 * WDOG_CHECK_HZ ) == 0 ) && tasksCheckedIn )
    {
        // Test the 90 second task flags
        status = WD_WatchdogCheck( WD_90_SEC, &tasksCheckedIn );
        WD_Trace( status, __LINE__ );
        s_DecimationCounter = 0;
    }

    if( !tasksCheckedIn )
    {
        WDG_Reset();
    }
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T WD_GetTaskControlBlock ( TF_DESC_T * *pDesc_ )
{
    if( pDesc_ == NULL )
    {
        WD_Trace( ERR_PARAMETER, __LINE__ );
        return ERR_PARAMETER;
    }

    *pDesc_ = &s_WdTaskDesc;

    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
static ERROR_T WD_WatchdogCheck( WATCHDOG_T group_, bool *taskCheckedIn_ )
{
    ERROR_T status = ERR_NONE;
    uint32_t startIdx;
    uint32_t endIdx;
    uint8_t osErr;
    NO_INIT_RAM_T *pRam;

    if( taskCheckedIn_ == NULL )
    {
        WD_Trace( ERR_PARAMETER, __LINE__ );
        return ERR_PARAMETER;
    }

    *taskCheckedIn_ = TRUE;

    switch (group_)
    {
        case WD_1_SEC:
            startIdx = WD_1_SEC;
            endIdx = WD_5_SEC;
            break;
        case WD_5_SEC:
            startIdx = WD_5_SEC;
            endIdx = WD_10_SEC;
            break;
        case WD_10_SEC:
            startIdx = WD_10_SEC;
            endIdx = WD_45_SEC;
            break;
        case WD_45_SEC:
            startIdx = WD_45_SEC;
            endIdx = WD_90_SEC;
            break;
        case WD_90_SEC:
            startIdx = WD_90_SEC;
            endIdx = NUM_OF_WD;
            break;
        default:
            WD_Trace( ERR_PARAMETER, __LINE__ );
            return ERR_PARAMETER;
    }

    // Get watchdog mutex
    OSMutexPend( s_WDmutex, INFINITE_TO, &osErr );

    if( osErr != OS_ERR_NONE )
    {
        WD_Trace( ERR_OS, __LINE__ );
        return ERR_OS;
    }

    // Loop through group
    while( startIdx < endIdx )
    {
        // Track check-in
        if(s_WatchdogCheckIn[startIdx] == WD_FAILED_CHECK)
        {
            *taskCheckedIn_ = FALSE;

            status = MEM_GetNoInitRam( &pRam );
            WD_Trace( status, __LINE__ );

            if( ( pRam != NULL ) && ( pRam->AsStruct.GuiltyWatchdogTaskId == WD_UNDEFINED ) )
            {
                pRam->AsStruct.GuiltyWatchdogTaskId = ( WATCHDOG_T )startIdx;
            }
        }

        // Reset flag
        if( s_WatchdogCheckIn[startIdx] != WD_IGNORE_TIMEOUT )
        {
            s_WatchdogCheckIn[startIdx] = WD_FAILED_CHECK;
        }

        startIdx++;
    }

    // Release watchdog mutex
    osErr = OSMutexPost( s_WDmutex );

    if( osErr != OS_ERR_NONE )
    {
        WD_Trace( ERR_OS, __LINE__ );
        return ERR_OS;
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T WD_CheckIn( WATCHDOG_T id_ )
{
    ERROR_T status = ERR_NONE;
    uint8_t osErr;

    if( id_ < NUM_OF_WD )
    {
        if( s_WDmutex != NULL )
        {
            // Get watchdog mutex
            OSMutexPend( s_WDmutex, INFINITE_TO, &osErr );

            if( osErr != OS_ERR_NONE )
            {
                WD_Trace( ERR_OS, __LINE__ );
                return ERR_OS;
            }

            s_WatchdogCheckIn[ id_ ] = WD_CHECKED_IN;

            // Release watchdog mutex
            osErr = OSMutexPost( s_WDmutex );

            if( osErr != OS_ERR_NONE )
            {
                WD_Trace( ERR_OS, __LINE__ );
                return ERR_OS;
            }
        }
    }
    else
    {
        status = ERR_PARAMETER;
        WD_Trace( status, __LINE__ );
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T WD_DisableTaskWdog( WATCHDOG_T id_ )
{
    ERROR_T status = ERR_NONE;

    if( ( id_ >= NUM_OF_WD ) || ( id_ == WD_UNDEFINED ) )
    {
        status = ERR_PARAMETER;
    }
    else
    {
        s_WatchdogCheckIn[ id_ ] = WD_IGNORE_TIMEOUT;
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T WD_EnableTaskWdog( WATCHDOG_T id_ )
{
    ERROR_T status = ERR_NONE;

    if( ( id_ >= NUM_OF_WD ) || ( id_ == WD_UNDEFINED ) )
    {
        status = ERR_PARAMETER;
    }
    else
    {
        s_WatchdogCheckIn[ id_ ] = WD_CHECKED_IN;
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
static void WD_Trace ( ERROR_T errMsg_, int line_ )
{
    if( errMsg_ != ERR_NONE )
    {
        SAFE_ReportError( errMsg_, __FILENAME__, line_ ); //lint !e613 Filename is not NULL ptr
    }
}

