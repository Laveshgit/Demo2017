///////////////////////////////////////////////////////////////////////////////
///
///                Copyright(c) 2017 FlexMedical
///
///                   *** Confidential Company Proprietary ***
///
/// \file AlarmAlertManager.h
///
/// \brief Manages active alarm/alerts and notifies the system as the
///        highest priority alarm/alert changes
///
///
///////////////////////////////////////////////////////////////////////////////

#ifndef _ALARM_ALERT_MANAGER_H_
#define _ALARM_ALERT_MANAGER_H_

///////////////////////////////////////////////////////////////////////////////
// Includes
///////////////////////////////////////////////////////////////////////////////

#include <ucos_ii.h>

#include "Common.h"
#include "Errors.h"
#include "TaskFramework.h"

///////////////////////////////////////////////////////////////////////////////
// Defines and macros
///////////////////////////////////////////////////////////////////////////////

#define AAM_TASK_STK_SIZE    ( 512u )

///////////////////////////////////////////////////////////////////////////////
// Structures, Enumerations, Typedefs
///////////////////////////////////////////////////////////////////////////////

// Enum entries can be added in any order
typedef enum
{
    ALL_CLEAR,              //0
    LINESET_ALARM,          //1
    LOW_BATT,               //2
    EXH_BATT,               //3
    STEP_ENDING,            //4
    NEAR_END_INF,           //5
    END_OF_HY,              //6
    PUMP_ERROR_ALARM,      //7
    AIL,                    //8
    OCC,                    //9
    END_OF_IG,              //10
    PRIMING_ERR,            //11
    REPLACE_BATT,           //12
    SHUTDOWN_BATT,          //13
    NO_ACTION,              //14
    END_OF_FLUSH,           //15
    NUM_OF_ALRTS            //16
}ALRM_ALRT_NOTI_T;

///////////////////////////////////////////////////////////////////////////////
// Local Variables
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Function Prototypes
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// \brief		Initializes AAM software unit.
///
/// \param[in]	void
///
/// \return		ERR_NONE
ERROR_T AAM_Init( void );

///////////////////////////////////////////////////////////////////////////////
/// \brief      Set the Alarm/Alert specified by id_ within the priority queue
///
/// \param[in]  id_ Alarm/Alert to set
///
/// \return     ERR status
ERROR_T AAM_Set( ALRM_ALRT_NOTI_T id_ );

///////////////////////////////////////////////////////////////////////////////
/// \brief      Clear the Alarm/Alert specified by id_ within the priority queue
///
/// \param[in]  id_ Alarm/Alert to clear
///
/// \return     ERR status
ERROR_T AAM_Clear( ALRM_ALRT_NOTI_T id_ );

///////////////////////////////////////////////////////////////////////////////
/// \brief      Unprotected peek into the priority queue. Intended for CLI debug only.
///
/// \param[in]  pBuff: Pointer to dump the Queue contents into
///             size:  Size of pBuff
///
/// \return     ERR_NONE
ERROR_T AAM_Query( char *sBuff_, uint32_t size_ );

///////////////////////////////////////////////////////////////////////////////
/// \brief      Returns if the enum id is active in the alarm alert queue
///
/// \param[in]  id_ :  enum value of the alarm/alert/notification
///             size:  true - id is queued. false - it is not queued
///
/// \return     ERR_NONE
///             ERR_PARAMETER
ERROR_T AAM_isActive( ALRM_ALRT_NOTI_T id_, bool *status_ );

///////////////////////////////////////////////////////////////////////////////
/// \brief      Initializes AAM software unit.
///
/// \param[in]  void
///
/// \return     ERR_NONE
ERROR_T AAM_EnterStandby( void );

///////////////////////////////////////////////////////////////////////////////
/// \brief      Provides access to the task descriptor
///
/// \param[in]  **pDesc pointer to a pointer for assignment
///
/// \param[out] **pDesc now points to the task descriptor
///
/// \return     ERR_NONE
ERROR_T AAM_GetTaskControlBlock( TF_DESC_T * *pDesc_ );

#endif //_ALARM_ALERT_MANAGER_H_
