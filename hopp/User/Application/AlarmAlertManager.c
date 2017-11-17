//////////////////////////////////////////////////////////////////////////////
///
///                Copyright(c) 2017 FlexMedical
///
///                   *** Confidential Company Proprietary ***
///
/// \file AlarmAlertManager.c
///
/// \brief Tracks Highest Priority Alarm/Alert and notifies the system
///
/// Task is informed as alarm and alerts are detected and cleared. Task notifies
/// the rest of the system of the highest priority alarm and alert.
///////////////////////////////////////////////////////////////////////////////

#ifndef _ALARM_ALERT_MANAGER_C_
#define _ALARM_ALERT_MANAGER_C_

#include <stdio.h>

#include "AlarmAlertManager.h"
#include "InfusionManager.h"
#include "SafetyMonitor.h"
#include "UIManager.h"
#include "LoggingManager.h"

////////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND #DEFINES
////////////////////////////////////////////////////////////////////////////////

#define AAM_MAX_MSGS   ( 10 )
#define AAM_TIMEOUT_MS ( 3000 )
#define NAME_LEN ( 25 )
#define LOG_MSG_LEN ( NAME_LEN + 10 ) //Msg is "Name active\r\n"

#define AAM_TABLE_PTR_CHECK( ptr_ )                                            \
    do                                                                         \
    {                                                                          \
        if( ( ( ( uint32_t )( ptr_ ) > ( uint32_t )NULL ) &&                   \
              ( ( uint32_t )( ptr_ ) < ( uint32_t )( &s_AaQueue[0] ) ) ) ||    \
            ( (uint32_t)( ptr_ ) >= (uint32_t)( &s_AaQueue[NUM_OF_ALRTS] ) ) ) \
        {                                                                      \
            AAM_Trace( ERR_VERIFICATION, __LINE__ );                           \
            return; /* Abort the active function*/                             \
        }                                                                      \
    }                                                                          \
    while( 0 )

////////////////////////////////////////////////////////////////////////////////
// ENUMS, TYPEDEFS, AND STRUCTURES
////////////////////////////////////////////////////////////////////////////////
typedef enum
{
    AAM_SET,
    AAM_CLEAR,
    AAM_STANDBY,
    AAM_WDOG

}AAM_COMMAND_T;

typedef struct
{
    AAM_COMMAND_T Cmd;
    ALRM_ALRT_NOTI_T Alert;
    uint8_t Align[2];    // Struct must be 4 byte aligned.

}AAM_MESSAGE_T;

// Priority is ordered lowest to highest.
typedef enum
{
    PRIORITY_NONE,
    PRIORITY_REPLACE_BATT,
    PRIORITY_STEP_ENDING,
    PRIORITY_END_OF_FLUSH,
    PRIORITY_NEAR_END_INF,
    PRIORITY_LOW_BATT,
    PRIORITY_NO_ACTION,  //Applies to paused too long or door open waiting for inf set for too long.
    PRIORITY_END_OF_INF, //This applies to HY and IG
    PRIORITY_AIL,
    PRIORITY_OCC,        //This applies to priming and occlusion
    PRIORITY_LINESET,
    PRIORITY_EXH_BATT,
    PRIORITY_SHUTDOWN_BATT,
    PRIORITY_PUMP_ERROR
}ALRM_ALRT_PRIORITY_T;

typedef struct __AAM_LIST_ENTRY_T__
{
    const ALRM_ALRT_NOTI_T ID;                  // Alarm enum value
    const ALRM_ALRT_PRIORITY_T PRIORITY;        // Alarm priority
    struct __AAM_LIST_ENTRY_T__ *Next;          // Pointer to next alarm in active list. Can be NULL.
    bool Queued;                                // Flag indicating the alarm is in the active queue.
    const char NAME[NAME_LEN];
}AAM_LIST_ENTRY_T;

///////////////////////////////////////////////////////////////////////////////
// Local Variables
///////////////////////////////////////////////////////////////////////////////

static OS_EVENT *s_AAMQueue;
static void *s_AAMMsgs[AAM_MAX_MSGS];
static AAM_MESSAGE_T s_AAMMsgPool[AAM_MAX_MSGS];
static OS_STK s_AAMTaskStk[AAM_TASK_STK_SIZE];

// Entry maps to Alarm & Alert enumeration order
static AAM_LIST_ENTRY_T s_AaQueue[NUM_OF_ALRTS] =
{   // id                   priority                pNext     queued    NAME
    {  ALL_CLEAR,           PRIORITY_NONE,          NULL,     FALSE,    "No Alarm"             },
    {  LINESET_ALARM,       PRIORITY_LINESET,       NULL,     FALSE,    "Linset Alarm"         }, //Occurs when the door opens during infusion
    {  LOW_BATT,            PRIORITY_LOW_BATT,      NULL,     FALSE,    "Low Batt Alert"       },
    {  EXH_BATT,            PRIORITY_EXH_BATT,      NULL,     FALSE,    "Exhausted Batt Alarm" },
    {  STEP_ENDING,         PRIORITY_STEP_ENDING,   NULL,     FALSE,    "Step Ending Alert"    },
    {  NEAR_END_INF,        PRIORITY_NEAR_END_INF,  NULL,     FALSE,    "Near End Inf Alert"   },
    {  END_OF_HY,           PRIORITY_END_OF_INF,    NULL,     FALSE,    "End of Hy Alarm"      },
    {  PUMP_ERROR_ALARM,    PRIORITY_PUMP_ERROR,    NULL,     FALSE,    "Pump Error Alarm"     },
    {  AIL,                 PRIORITY_AIL,           NULL,     FALSE,    "AIL Alarm"            },
    {  OCC,                 PRIORITY_OCC,           NULL,     FALSE,    "OCC Alarm"            },
    {  END_OF_IG,           PRIORITY_END_OF_INF,    NULL,     FALSE,    "End of IG Alarm"      },
    {  PRIMING_ERR,         PRIORITY_OCC,           NULL,     FALSE,    "Priming Alarm"        },
    {  REPLACE_BATT,        PRIORITY_REPLACE_BATT,  NULL,     FALSE,    "Replace Batt Alarm"   },
    {  SHUTDOWN_BATT,       PRIORITY_SHUTDOWN_BATT, NULL,     FALSE,    "Batt Shutdown Alarm"  },
    {  NO_ACTION,           PRIORITY_NO_ACTION,     NULL,     FALSE,    "No Action Alert"      }, //Occurs if paused too long or waiting for inf set and user takes too long.
    {  END_OF_FLUSH,        PRIORITY_END_OF_FLUSH,  NULL,     FALSE,    "End of Flush Alarm"   },
};

// Pointer to head of priority queue
static AAM_LIST_ENTRY_T *s_Qhead = NULL;

///////////////////////////////////////////////////////////////////////////////
// Prototypes
///////////////////////////////////////////////////////////////////////////////

static ERROR_T AAM_postMsg( const AAM_MESSAGE_T *pAamMsg_ );
static void AAM_Trace( ERROR_T errMsg_, int line_ );
static void AAM_onSet( ALRM_ALRT_NOTI_T id_ );
static void AAM_onClear( ALRM_ALRT_NOTI_T id_ );
static void AAM_onUpdate( void );
static void AAM_clearQueue( void );
static void AAM_Task( void *pArg_ );
static void AAM_PostWdgMsg( void );

static TF_DESC_T s_AamTaskDesc =
{
    .NAME = "AAM_Task",      // task name
    .ptos = &s_AAMTaskStk[AAM_TASK_STK_SIZE - 1],      // Stack top
    .prio = (int)AAM_TASK_PRIO,       // Task priority
    .pbos = s_AAMTaskStk,      // Stack bottom
    .stk_size = AAM_TASK_STK_SIZE,   // Stack size
    .taskId = AAM,
    .queue = &s_AAMQueue,
    .msgPool = (uint8_t *)s_AAMMsgPool,
    .queueBuf = s_AAMMsgs,
    .msgCount = (uint16_t)AAM_MAX_MSGS,
    .msgSize = (uint16_t)sizeof(AAM_MESSAGE_T),
    .PreStart = AAM_Init,
    .PostStart = NULL,
    .MsgHandler = AAM_Task,
    .TimeoutHandler = AAM_PostWdgMsg,
    .msgTimeoutMS = AAM_TIMEOUT_MS,
    .MemPtr = NULL,
    .maxQdepth = 0,
    .taskRunning = FALSE,
    .WdgId = WD_AAM,
};

///////////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////////
static void AAM_PostWdgMsg( void )
{
    ERROR_T status;
    AAM_MESSAGE_T msg;
    msg.Cmd = AAM_WDOG;
    status=AAM_postMsg(&msg);
    AAM_Trace( status, __LINE__ );
}

///////////////////////////////////////////////////////////////////////////////
ERROR_T AAM_Init ( void )
{
    return ERR_NONE;
}

///////////////////////////////////////////////////////////////////////////////
static void AAM_onSet ( ALRM_ALRT_NOTI_T id_ )
{
    AAM_LIST_ENTRY_T * *pCurr = &s_Qhead; // Pointer to a pointer
    AAM_LIST_ENTRY_T *pInsert;     // Pointer to list item to be added

    // Range check
    if( id_ >= NUM_OF_ALRTS )
    {
        AAM_Trace( ERR_PARAMETER, __LINE__ );
        return;
    }

    // Point to the definition we want inserted into the queue.
    pInsert = &s_AaQueue[id_];

    if( pInsert->Queued )
    {
        // id is already activated. Do nothing.
        return;
    }

    // Check for end of list and priority
    while( ( ( *pCurr ) != NULL ) && ( ( *pCurr )->PRIORITY > pInsert->PRIORITY ) )
    {
        pCurr = &( ( *pCurr )->Next);
        AAM_TABLE_PTR_CHECK( *pCurr );
    }

    pInsert->Next = ( *pCurr ); // Can be NULL indicating end of list
    ( *pCurr ) = pInsert;
    pInsert->Queued = TRUE;
}

///////////////////////////////////////////////////////////////////////////////
static void AAM_onClear ( ALRM_ALRT_NOTI_T id_ )
{
    AAM_LIST_ENTRY_T * *pCurr =  &s_Qhead; // Pointer to a pointer
    AAM_LIST_ENTRY_T *pClear;     // Pointer to list item to be cleared

    // Range check
    if( id_ >= NUM_OF_ALRTS )
    {
        AAM_Trace( ERR_PARAMETER, __LINE__ );
        return;
    }

    // Point to the definition to clear from the queue.
    pClear = &s_AaQueue[id_];

    if( ( !pClear->Queued ) || ( s_Qhead == NULL ) )
    {
        // id is already cleared. Do nothing.
        return;
    }

    // Find entry within the priority list.
    while( ( *pCurr ) != pClear )
    {
        // Navigate to the next list item
        pCurr = &( ( *pCurr )->Next );
        AAM_TABLE_PTR_CHECK( pCurr );
    }

    // Point over the cleared entry
    ( *pCurr ) = pClear->Next; // Can be NULL

    // Reset the cleared entry variables
    pClear->Next = NULL;
    pClear->Queued = FALSE;
}

///////////////////////////////////////////////////////////////////////////////
static void AAM_onUpdate ( void )
{
    ERROR_T status = ERR_NONE;
    SSM_MESSAGE_T ssMsg;
    ALRM_ALRT_NOTI_T id = ALL_CLEAR;
    char logMsg[LOG_MSG_LEN];

    if( s_Qhead != NULL )
    {
        id = s_Qhead->ID;
        snprintf( logMsg, sizeof(logMsg), "%s active\r\n", s_Qhead->NAME );
        status = LOG_Write( logMsg );
        AAM_Trace( status, __LINE__ );
    }

    ssMsg.MsgEvent = SSM_EVT_AA;
    ssMsg.AlarmId = id;
    status = SSM_CommandNotify( &ssMsg );
    AAM_Trace( status, __LINE__ );
}

///////////////////////////////////////////////////////////////////////////////
static void AAM_Task ( void *pArg_ )
{
    AAM_MESSAGE_T *aamMsg;
    static const AAM_LIST_ENTRY_T *pOrig = NULL;

    if( pArg_ == NULL )
    {
        return;
    }

    aamMsg = (AAM_MESSAGE_T *)pArg_;

    (void)aamMsg->Align; // Referenced to suppress LINT warning.

    switch( aamMsg->Cmd )
    {
        case AAM_SET:

            AAM_onSet( aamMsg->Alert );

            if( s_Qhead != pOrig )
            {
                AAM_onUpdate();
                pOrig = s_Qhead;
            }
            break;

        case AAM_CLEAR:

            AAM_onClear( aamMsg->Alert );

            if( s_Qhead != pOrig )
            {
                AAM_onUpdate();
                pOrig = s_Qhead;
            }
            break;

        case AAM_STANDBY:
            AAM_clearQueue();
            break;

        case AAM_WDOG:
            // Do nothing more than exercise the message queue
            break;

        default:
        {
            AAM_Trace( ERR_PARAMETER, __LINE__ );
            break;
        }
    }
} //lint !e818 arg cannot be declared constant without changing RTOS headers

////////////////////////////////////////////////////////////////////////////////
static ERROR_T AAM_postMsg ( const AAM_MESSAGE_T *pAamMsg_ )
{
    ERROR_T status;
    status = TF_PostMsg( &s_AamTaskDesc, (const void *)pAamMsg_ );
    AAM_Trace( status, __LINE__ );
    return status;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T AAM_Set ( ALRM_ALRT_NOTI_T id_ )
{
    ERROR_T status;

    AAM_MESSAGE_T aamMsg;

    (void) memset( (void *)&aamMsg, 0, sizeof(aamMsg) );

    aamMsg.Cmd = AAM_SET;
    aamMsg.Alert = id_;

    if( id_ >= NUM_OF_ALRTS )
    {
        status = ERR_PARAMETER;
    }
    else
    {
        status = AAM_postMsg( (const AAM_MESSAGE_T *) &aamMsg );
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
//Specific alarms and alerts can be cleared by multiple systems that may or may not have set them.
//Example: END_OF_HY is set by infusion manager and cleared by UIM
ERROR_T AAM_Clear ( ALRM_ALRT_NOTI_T id_ )
{
    ERROR_T status;

    AAM_MESSAGE_T aamMsg;

    (void) memset( (void *)&aamMsg, 0, sizeof(aamMsg) );

    aamMsg.Cmd = AAM_CLEAR;
    aamMsg.Alert = id_;

    if( id_ >= NUM_OF_ALRTS )
    {
        status = ERR_PARAMETER;
    }
    else
    {
        status = AAM_postMsg( (const AAM_MESSAGE_T *)&aamMsg );
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
static void AAM_Trace ( ERROR_T errMsg_, int line_ )
{
    SAFE_ReportError( errMsg_, __FILENAME__, line_ ); //lint !e613 Filename is not NULL ptr
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T AAM_Query ( char *sBuff_, uint32_t size_ )
{
    AAM_LIST_ENTRY_T *pEntry = NULL;
    char sAppend[4];

    if( sBuff_ == NULL )
    {
        return ERR_PARAMETER;
    }

    if( size_ > 0 )
    {
        (void)memset( sBuff_, 0, size_ );
        (void)memset( sAppend, 0, sizeof(sAppend) );
    }
    else
    {
        return ERR_NONE;
    }

    // Point to head
    pEntry = s_Qhead;

    // While an item is queued
    while( pEntry != NULL )
    {
        // Convert ID to a string
        (void)snprintf( sAppend, sizeof(sAppend), "%d ", pEntry->ID ); //lint !e516 !e718 !e746 Function prototype is in stdio

        // Append to output buffer (appends NULL)
        (void)strncat( sBuff_, sAppend, size_ );

        // Adjust available size
        size_ = size_ - (uint32_t)strlen( sBuff_ );

        // Move through the queue. Assigning NULL indicates end of list and terminates the loop
        pEntry = pEntry->Next;
    }

    // Append to output buffer (appends NULL)
    (void)strncat( sBuff_, "\r\n", size_ );

    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T AAM_isActive ( ALRM_ALRT_NOTI_T id_, bool *status_ )
{
    ERROR_T err = ERR_NONE;

    if( ( id_ >= NUM_OF_ALRTS ) || ( status_ == NULL ) )
    {
        err = ERR_PARAMETER;
        AAM_Trace( err, __LINE__ );
        return err;
    }

    *status_ = s_AaQueue[id_].Queued;

    return err;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T AAM_EnterStandby ( void )
{
    AAM_MESSAGE_T aamMsg;

    (void) memset( (void *)&aamMsg, 0, sizeof(aamMsg) );

    aamMsg.Cmd = AAM_STANDBY;
    aamMsg.Alert = ALL_CLEAR;

    return AAM_postMsg( (const AAM_MESSAGE_T *)&aamMsg );
}

////////////////////////////////////////////////////////////////////////////////
static void AAM_clearQueue ( void )
{
    for( int i = 0; i < (int)NUM_OF_ALRTS; i++ )
    {
        s_AaQueue[i].Queued = false;
        s_AaQueue[i].Next = NULL;
    }

    s_Qhead = NULL;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T AAM_GetTaskControlBlock ( TF_DESC_T * *pDesc_ )
{
    if( pDesc_ == NULL )
    {
        return ERR_PARAMETER;
    }

    *pDesc_ = &s_AamTaskDesc;
    return ERR_NONE;
}

#endif // _ALARM_ALERT_MANAGER_C_
