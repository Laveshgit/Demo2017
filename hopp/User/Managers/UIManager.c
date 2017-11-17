///////////////////////////////////////////////////////////////////////////////
//
// Copyright(c) (2016 Flextronics)

// *** Confidential Company Proprietary ***
//
//
// DESCRIPTION:
//
/// \file   UIManager.c
/// \brief  Manages the states of the LEDs, Audio output, what displays on screen, and incoming states from buttons.
///
///
///////////////////////////////////////////////////////////////////////////////
#include <clk.h>
///USER INCLUDES
#include "AudioManager.h"
#include "Common.h"
#include "Errors.h"
#include "FileSystemManager.h"
#include "InfusionManager.h"
#include "InfusionManagerFunctions.h"
#include "InfusionResumeFunctions.h"
#include "LEDManager.h"
#include "CommonTypes.h"
#include "SafetyMonitor.h"
#include "ScreenFlowTables.h"
#include "SerialManager.h"
#include "SpiDriver.h"
#include "SwitchManager.h"
#include "UIManager.h"
#include "USBManager.h"
#include "UIM_ScreenFunctions.h"
#include "ConfigurationManager.h"
#include "BatteryMonitor.h"
#include "ScreenCallBacks.h"
#include "WatchdogManager.h"

///RTOS INCLUDES
#include "ucos_ii.h"

////////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND #DEFINES
////////////////////////////////////////////////////////////////////////////////

#define DEFAULT_PASSCODE_VALUE ( 1 )
#define UIM_MAX_MSGS   ( 16 )
#define UIM_TIMEOUT_MS ( 700 )

///////////////////////////////////////////////////////////////////////////////
// Global Variables
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Local Variables
///////////////////////////////////////////////////////////////////////////////

static WM_HWIN s_BaseWindow = 0;                                 ///Holds the main window of each screen which contains widgets,text
static WM_HWIN s_AlarmAlertPopUp = 0;
static WM_HWIN s_PowerPopUp = 0;
static LED_ALERT_LVL_T s_AlarmLEDStatus;
static OS_EVENT *s_UimQueue;
static void *s_UimMsgs[UIM_MAX_MSGS];

static OS_TMR *s_UimLedPatternTmr;
static OS_TMR *s_UimBaseRedrawTmr;         //Used to draw arrows as they blink during infusion
static OS_TMR *s_UimPopupRedrawTmr;         //Used to draw arrows as they blink during infusion
static OS_TMR *s_PowerTimer;
static OS_TMR *s_PowerProgBarTimer;//Used to draw power progress bar
static OS_TMR *s_PromptTimer;      // Used to draw PROGRAM_IS_READY_FOR_USE screen display messages
static OS_TMR *s_ScreenTimeoutTmr; //Used for alarm screens that have a finite timeout
static bool s_IsPausePressed = false;

static RATE_VALUE_T s_PrevFlowRate = ML_0; //Used to veriify that the flow rate has changed before sending to Inf Mgr

static const SCREEN_FUNC_T (*s_CurrTablePtr)[NUM_BUTTONS];
static const SCR_ATTR_T (*s_CurrDrawArrPtr);

static PROGRAM_INFO_T s_CurrProgram;
static float s_VolumeInfused = 0.0;

static uint16_t s_CurrStep = 0;                           

static bool s_HasPrimedFlag = false;
static bool s_AbortFlag = false;            //Keeps track if user pressed back and it caused a jump in between state tables.
static bool s_PowerReleased = false;        //Keeps track of the power button state.

///Holds current value of passcode and current selection of column
static PASSCODE_T s_Passcode = {0};

// UI task stack allocation
static OS_STK s_UimTaskStk[UIM_TASK_STK_SIZE];
static UIM_EVENT_MESSAGE_T s_UimMessage[UIM_MAX_MSGS];
static PROGRAM_TYPE_T s_ProgramType ;

static bool s_IsOffProgram = FALSE;
//Current List Box Index 
static int s_ListBoxIndex;
static int s_AuthMenuLBIndex=0;

//This determines if the UI is in screen review mode. If so, then only allow power, up, and down buttons.
static bool s_ScreenReviewFlag = false;

//This varilable stores total IG dosage till the current IG Step.
static float s_TotalStepDosage = 0;

static GUI_RECT s_Rect;

///////////////////////////////////////////////////////////////////////////////
// Prototypes
///////////////////////////////////////////////////////////////////////////////

static ERROR_T UIM_TaskPostStart( void );

static void UIM_ChangeScreen( const SCREEN_FUNC_T *screen_, SCREEN_T screenType_ );

static void UIM_SwitchChange( SWITCH_STATE_T switchEvent );

static void UIM_StepChange( const TIME_CHANGE_DATA_T *timeChangeData_  );

static void UIM_SysStateChange( SSM_STATES_T state_ );

static void UIM_StateChange( INF_STATE_T state_ );

static void UIM_PullBackComplete( void );

static void UIM_HandleAA( ALRM_ALRT_NOTI_T aa_ );
static void UIM_HandlePumpStatus( PUMP_STATUS_T pumpStatus_, bool newStatusFlag_ );

static void UIM_HandleTimeChange( const UIM_EVENT_MESSAGE_T *event_ );

static void UIM_CreateBaseRedrawTmr( void );
static void UIM_CreatePopupRedrawTmr( uint32_t period_ );

static void UIM_HandlePowerStateExit ( void );

static void UIM_DeletePopUp( WM_HWIN popUp_ );
static void UIM_GetActivePopUpScreenInfo( SCREEN_INFO_T *screenInfo_ );
static void UIM_GetActiveWinAndScreenInfo( SCREEN_INFO_T *screenInfo_, WM_HWIN **win_ );

static void UIM_BaseRedrawTmrCB ( void *ptmr_, void *parg_ );
static void UIM_PopupRedrawTmrCB ( void *ptmr_, void *parg_ );
static void UIM_PowerRedrawTmrCB ( void *ptmr_, void *parg_ );
static void UIM_LedPatternCB( void *ptmr_, void *parg_ );

static void UIM_SaveNeedleCheckStatus( int selection_ );
static void UIM_SaveFlushStatus( const int selection_ );
static void UIM_ExitAuthorizedSetupSelection( int selection_ );
static void UIM_FlushCompleteSelection ( int selection_ );
static void UIM_SaveAllowRateChange( int selection_ );
static void UIM_DetermineRate( int selection );
static void UIM_PromptTimeComplete( SCREEN_ID_T screenNo_ );
static void UIM_SavePatientWeight( int selection_ );
static void UIM_ProgramInfoScreen( void );
static void UIM_IgStepFlowRateScreen( void );

static void UIM_SetAlarmLED( LED_ALERT_LVL_T lvl_ );

static void UIM_CreatePowerTimer( int delay_ );
static void UIM_RequestPowerOn( void );

static void UIM_ResetListBoxIndex( void );
static void UIM_SetBackButtonListboxIndex( void );

static void UIM_ClearNearEndOfInfScreenCB(void *ptmr_, void *parg_);

static void UIM_AuthorizedMenuSelection( int selection );
static void UIM_ProgramTypeSelection( int selection );
static void UIM_NeedleTypeSelection ( int selection_ );
static void UIM_InfusionRampUpSelection( int selection_ );

static bool UIM_CheckForProgramError( uint8_t currIgStepScreenIndex_ );
static void UIM_UsbEnableStatus ( const int selection_ );
static void UIM_PostWdogMsg( void );

static TF_DESC_T s_UimTaskDesc =
{
    .NAME = "UIM_Task",      // task name
    .ptos = &s_UimTaskStk[UIM_TASK_STK_SIZE - 1],      // Stack top
    .prio = (int)UIM_TASK_PRIO,       // Task priority
    .pbos = s_UimTaskStk,      // Stack bottom
    .stk_size = UIM_TASK_STK_SIZE,   // Stack size
    .taskId = UIM,
    .queue = &s_UimQueue,
    .queueBuf = s_UimMsgs,
    .msgPool = (uint8_t *)s_UimMessage,
    .msgCount = (uint16_t)UIM_MAX_MSGS,
    .msgSize = (uint16_t)sizeof(UIM_EVENT_MESSAGE_T),
    .PreStart = UIM_Initialize,
    .PostStart = UIM_TaskPostStart,
    .MsgHandler = UIM_Task,
    .TimeoutHandler = UIM_PostWdogMsg,
    .msgTimeoutMS = UIM_TIMEOUT_MS,
    .MemPtr = NULL,
    .maxQdepth = 0,
    .taskRunning = FALSE,
    .WdgId = WD_UIM
};

////////////////////////////////////////////////////////////////////////////////
static void UIM_Trace( ERROR_T errMsg_, int line_ )
{
    if( errMsg_ != ERR_NONE)
    {
        SAFE_ReportError( errMsg_, __FILENAME__, line_ ); //lint !e613 Filename is not NULL ptr
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_PostWdogMsg( void )
{
    UIM_EVENT_MESSAGE_T msg;
    ERROR_T status;
    msg.EventType = UIM_WDOG;
    status = UIM_CommandNotify( &msg );
    UIM_Trace( status, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
void UI_SetScreen( SCREEN_ID_T screen_ )
{
    static SCREEN_ID_T s_currScreen = MAX_SCREENS;
    WM_HWIN window;
    SCREEN_INFO_T screenInfo;
    int numOfBytesWritten = 0;
    SCREEN_FUNC_T substate;

    WM_DeleteWindow(s_AlarmAlertPopUp); 
    WM_DeleteWindow(s_PowerPopUp);
    s_PowerPopUp = 0;
    s_AlarmAlertPopUp = 0;
    
    if( screen_ == SCREEN_9_1I )
    {
        WM_DeleteWindow(s_AlarmAlertPopUp); 
        WM_DeleteWindow(s_PowerPopUp);
    }
    else
    {
        WM_DeleteWindow(s_AlarmAlertPopUp); 
        WM_DeleteWindow(s_PowerPopUp);
    }

    WM_DeleteWindow( s_BaseWindow ); 
    s_BaseWindow = UIM_DrawScreen( screen_, SCREENS_ARR[ screen_ ].TestTemplateId, SCREENS_ARR[ screen_ ].TestInfo, ( s_currScreen == screen_ ));
    
    s_currScreen = screen_;

    substate.Type = ID;
    substate.ID = ( SCREEN_STATE_T )screen_; // Assumed that the screen id is the table offset too
    screenInfo.ScreenNumber = screen_;
    screenInfo.SubStateScreen = substate;
    screenInfo.TABLE_PTR = SCREEN_REVIEW_TABLE;
    screenInfo.ScreenType = INFUSION_TYPE;

    window = WM_GetClientWindow( s_BaseWindow );

    numOfBytesWritten = WINDOW_SetUserData( window, (void*)&screenInfo , sizeof( screenInfo )); 

    if( numOfBytesWritten == 0 )
    {
        UIM_Trace( ERR_GUI, __LINE__ );
    }
    
    s_ScreenReviewFlag = true;

    GUI_Delay( GUI_DELAY_AMOUNT );

}

////////////////////////////////////////////////////////////////////////////////
static ERROR_T UIM_TaskPostStart( void )
{
    ERROR_T status = ERR_NONE;
    SCREEN_FUNC_T nextScreen;
    
    GUI_UC_SetEncodeUTF8();

    //Call LISTVIEW_SetDefaultGridColor before WM_INIT_DIALOG case ,LISTVIEW_SetDefaultGridColor Function is not working at first time when we will call from create screen in WM_INIT_DIALOG case
    LISTVIEW_SetDefaultGridColor( GUI_BLACK );

    // Blank screen initially
    nextScreen.ID = BLANK;
    nextScreen.Type = ID;
    s_CurrTablePtr = SPLASH_SCREEN_STATE_TABLE;
    s_CurrDrawArrPtr = LOGO_ATTRIBUTE_ARR;
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );

    return status;
}

////////////////////////////////////////////////////////////////////////////////
void UIM_Task( void *pArg_ )
{    
    ERROR_T err = ERR_NONE;
    UIM_EVENT_MESSAGE_T *event = (UIM_EVENT_MESSAGE_T*) pArg_;
    
    SCREEN_INFO_T screenInfo;

    if( event != NULL  )
    {
        switch ( event->EventType )
        {
            case UIM_WDOG:
                break;

            case UIM_REDRAW_TMR:   //Timer task must send a message to the UIM task to avoid delays and free up resources in timer task.
                UIM_GetActivePopUpScreenInfo( &screenInfo );

                if( event->ScreenRedraw == screenInfo.ScreenType )
                {
                    // Only redraw if this screen type applies to the timer tick
                    UIM_ChangeScreen( &screenInfo.SubStateScreen, screenInfo.ScreenType );
                }
                break;

            case UIM_SWITCH:
                UIM_SwitchChange( event->SwitchEvent );
                break;

            case UIM_INF_STATE_CHANGE:
                UIM_StateChange( event->State );
                break;

            case UIM_SYS_STATE_CHANGE:
                UIM_SysStateChange( event->SystemState );
                break;

            case UIM_ALERT_OR_ALARM:
                UIM_HandleAA( event->AlarmAlert );
                break;

            case UIM_BATTERY_STATUS:
                LED_SetBatteryLevel( &event->BatteryStatus );
                break;

            case UIM_INFUSION_STEP_COMPLETE:
                UIM_StepChange( &(event->TimeChangeData) );
                UIM_HandleTimeChange( event );
                break;

            case UIM_PULLBACK_COMPLETE:
                UIM_PullBackComplete();
                break;

            case UIM_TIME_CHANGE:
                UIM_HandleTimeChange( event );
                break;

            case UIM_PROMPT_TIMER:
                UIM_PromptTimeComplete( event->ScreenNo );
                break;

            case UIM_POWER_TIMER:
                UIM_HandlePowerStateExit();
                break;

            case UIM_PUMP_STATUS_T:
                UIM_HandlePumpStatus( event->PumpStatus, true );
                break;

            default:
                break;
        }
    }
    else
    {
        err = ERR_OS;
        UIM_Trace( err, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_BaseRedrawTmrCB ( void *ptmr_, void *parg_ )
{
    UNUSED(ptmr_);
    UNUSED(parg_);
    UIM_EVENT_MESSAGE_T uimMsg;
    
    ERROR_T err = ERR_NONE;
    
    uimMsg.EventType = UIM_REDRAW_TMR;
    uimMsg.ScreenRedraw = INFUSION_TYPE;

    err = UIM_CommandNotify( &uimMsg );
    UIM_Trace( err, __LINE__ );


}//lint !e818 p_arg could be const. //Should not be const since it is OS function

////////////////////////////////////////////////////////////////////////////////
static void UIM_PopupRedrawTmrCB ( void *ptmr_, void *parg_ )
{
    UNUSED(ptmr_);
    UNUSED(parg_);
    UIM_EVENT_MESSAGE_T uimMsg;

    ERROR_T err = ERR_NONE;

    uimMsg.EventType = UIM_REDRAW_TMR;
    uimMsg.ScreenRedraw = ALARM_TYPE;

    err = UIM_CommandNotify( &uimMsg );
    UIM_Trace( err, __LINE__ );


}//lint !e818 p_arg could be const. //Should not be const since it is OS function

////////////////////////////////////////////////////////////////////////////////
static void UIM_PowerRedrawTmrCB ( void *ptmr_, void *parg_ )
{
    UNUSED(ptmr_);
    UNUSED(parg_);
    ERROR_T err = ERR_NONE;
    UIM_EVENT_MESSAGE_T uimMsg;

    uimMsg.EventType = UIM_REDRAW_TMR;
    uimMsg.ScreenRedraw = POWER_TYPE;
    
    err = UIM_CommandNotify( &uimMsg );
    UIM_Trace( err, __LINE__ );

    
}//lint !e818 p_arg could be const. //Should not be const since it is OS function

///////////////////////////////////////////////////////////////////////////////
//The near end of infusion screen only displays for a short time. Both the infusion manager and the UIM will clear the alarm/alert
//since it is both a known infusion state and a timed screen.
static void UIM_ClearNearEndOfInfScreenCB(void *ptmr_, void *parg_)
{
    UNUSED(ptmr_);
    UNUSED(parg_);
    ERROR_T status = ERR_NONE;

    status = AAM_Clear( NEAR_END_INF );
    
    UIM_Trace( status, __LINE__ );
}


////////////////////////////////////////////////////////////////////////////////
static void UIM_SetAlarmLED( LED_ALERT_LVL_T lvl_ )
{
    ERROR_T err = ERR_NONE;

    if( lvl_ >= LED_NUM_LEVELS )
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
    }
    else
    {
        s_AlarmLEDStatus = lvl_;
        err = LED_SetAlarmAlertNotification( s_AlarmLEDStatus );
        UIM_Trace( err, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_LedPatternCB( void *ptmr_, void *parg_ )
{
    OS_TMR *pTimer = (OS_TMR *)ptmr_;
    UNUSED(parg_);
    ERROR_T err = ERR_NONE;
    AM_MESSAGE_T amMsg;

    if( pTimer == NULL ) 
    {
        // Unexpected NULL pointer
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return;
    }

    if ( pTimer->OSTmrDly == LED_PATTERN_TIMEOUT_YELLOW )
    {
        // Pattern was yellow for 5s. Change LED to flashing green.
        UIM_SetAlarmLED( LED_BLINKING_GREEN );
        pTimer->OSTmrDly = LED_PATTERN_TIMEOUT_GREEN;
    }
    else if( pTimer->OSTmrDly == LED_PATTERN_TIMEOUT_GREEN )
    {
        // Pattern was flashing green for 25s. Change LED to yellow.
        UIM_SetAlarmLED( LED_SOLID_YELLOW );
        pTimer->OSTmrDly = LED_PATTERN_TIMEOUT_YELLOW;

        // And trigger audio. UIM controls this so the LED and audio
        // remain in sync.
        amMsg.EventType = ALERT;
        amMsg.Alert = AUDIO_ALERT_NO_REPEAT;
        err = AM_CommandNotify( &amMsg );
        UIM_Trace( err, __LINE__ );
    }
    else
    {
        // Unexpected value.
        UIM_Trace( ERR_VERIFICATION, __LINE__ );

        // Choosing flashing green.
        UIM_SetAlarmLED( LED_BLINKING_GREEN );
        pTimer->OSTmrDly = LED_PATTERN_TIMEOUT_GREEN;
    }

    // Restart the one-shot timer with the newly configured delay
    err = TF_StartTimer( pTimer );
    UIM_Trace( err, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_PromptTimerCB ( void *ptmr_, void *parg_ )
{    
    ERROR_T reportedError = ERR_NONE;
    SCREEN_ID_T id;
    UIM_EVENT_MESSAGE_T uimMsg;
    
    UNUSED(ptmr_);

    if( parg_ == NULL )
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return;
    }

    id = *((SCREEN_ID_T *)parg_);
    
    uimMsg.EventType = UIM_PROMPT_TIMER;
    uimMsg.ScreenNo = id;
    reportedError = UIM_CommandNotify( &uimMsg ); //Direct call Of UIM Change function cause issues So use CommandNotify method
    UIM_Trace( reportedError, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_CreateBaseRedrawTmr( void )
{
    SCREEN_INFO_T screenInfo;
    ERROR_T status;
    TF_TIMER_DESC_T timer;
    static const char name[] = "UIM Draw Timer";
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
        
    int drawDelay = 0;
    
    if( ( screenInfo.ScreenNumber == SCREEN_17 )  ||        //FILLING
        ( screenInfo.ScreenNumber == SCREEN_32_2 )||        //CLEANING_AIR
        ( screenInfo.ScreenNumber == SCREEN_20 ) )          //PULLING BACK
    {
        drawDelay = ACTIVITY_BAR_DELAY;
    }
    else if ( s_IsPausePressed )
    {
        drawDelay = PAUSE_BATT_PERCENT_DELAY;
    }
    else
    {
        drawDelay = INF_ARROW_DELAY;
    }    

    if( s_UimBaseRedrawTmr != NULL )
    {
        status = TF_DeleteTimer( &s_UimBaseRedrawTmr );
        UIM_Trace( status, __LINE__ );
    }

    timer.ptmr = &s_UimBaseRedrawTmr;
    timer.dly = ( INT32U ) 0;
    timer.period = ( INT32U ) drawDelay;
    timer.opt = ( INT8U ) OS_TMR_OPT_PERIODIC;
    timer.callback = ( OS_TMR_CALLBACK ) UIM_BaseRedrawTmrCB;
    timer.callback_arg = ( void * ) 0;
    timer.NAME = name;
    
    status = TF_CreateAndStartTimer( &timer );
    UIM_Trace( status, __LINE__ );

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_CreatePopupRedrawTmr( uint32_t period_ )
{
    TF_TIMER_DESC_T timer;
    ERROR_T err = ERR_NONE;
    static const char name[] = "UIM Popup Redraw";

    timer.ptmr = &s_UimPopupRedrawTmr;
    timer.dly = ( INT32U ) 0;
    timer.period = ( INT32U )period_;
    timer.opt = ( INT8U ) OS_TMR_OPT_PERIODIC;
    timer.callback = ( OS_TMR_CALLBACK ) UIM_PopupRedrawTmrCB;
    timer.callback_arg = ( void * ) 0;
    timer.NAME = name;

    err = TF_CreateAndStartTimer( &timer ); 
    UIM_Trace(err,__LINE__);
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_PowerTimerCB( void *ptmr_, void *parg_ )
{
    UNUSED(ptmr_);
    UNUSED(parg_);
    UIM_EVENT_MESSAGE_T uimMsg;

    ERROR_T err = ERR_NONE;
    
    uimMsg.EventType = UIM_POWER_TIMER;
    
    err = UIM_CommandNotify( &uimMsg );
    UIM_Trace( err, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_RequestPowerOn( void )
{
    ERROR_T err = ERR_NONE;
    SSM_MESSAGE_T ssmMsg;
    ssmMsg.MsgEvent = SSM_EVT_STATE_REQ;
    ssmMsg.State = SSM_POST;
    err = SSM_CommandNotify( &ssmMsg );
    UIM_Trace( err, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_HandlePowerStateExit ( void )
{
    SSM_MESSAGE_T ssmMsg;
    INT8U osError = OS_ERR_NONE;
    INT8U powerTmrState = OS_TMR_STATE_UNUSED;
    ERROR_T status;
    
    SCREEN_INFO_T screenInfo;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    if(s_PowerTimer != NULL)
    {
        powerTmrState = OSTmrStateGet( s_PowerTimer, &osError );
        if( osError != OS_ERR_NONE )
        {
            UIM_Trace( ERR_OS, __LINE__ );
        }
    }
    
    if( screenInfo.ScreenNumber == SCREEN_0 )       //POWER_OFF_S
    {
        if( powerTmrState == OS_TMR_STATE_RUNNING ) //If the power button is released before the progress bar reaches 100%
        {           
            if( s_PowerProgBarTimer != NULL )
            {
                status = TF_StopAndDeleteTimer( &s_PowerProgBarTimer );
                UIM_Trace(status,__LINE__);
            }

            if( s_PowerTimer != NULL )
            {
                status = TF_StopAndDeleteTimer( &s_PowerTimer );
                UIM_Trace(status,__LINE__);
            }
            
            UIM_CreatePowerTimer( PWR_XTRA_SEC_DELAY ); //Set a new timer that holds the screen on for a bit longer after the 
            status = TF_StartTimer( s_PowerTimer );             //user releases, to allow them to fully understand why system didnt power down.
            UIM_Trace( status, __LINE__ );
            
            

        }
        else if(( powerTmrState == OS_TMR_STATE_COMPLETED ) && ( s_PowerReleased )) //Once this function is called again from the timer
        {                                                                           //callback, since the power button is not currently
            UIM_DeletePopUp( s_PowerPopUp );                                        //pressed, the UIM will finally redirect to prev screen.
  
            if( s_PowerTimer != NULL )
            {
                status = TF_StopAndDeleteTimer( &s_PowerTimer );
                UIM_Trace(status,__LINE__);
            }
        }
        else if(( powerTmrState == OS_TMR_STATE_COMPLETED ) && ( !s_PowerReleased )) //If the user continuously holds the power button
        {                                                                            //for the duration of the overall power timer,
                                                                                     //then the system will power down.
            ssmMsg.MsgEvent = SSM_EVT_STATE_REQ;
            ssmMsg.State = SSM_SHUTDOWN;
            status = SSM_CommandNotify(&ssmMsg);
            UIM_Trace( status, __LINE__ );
        }
    }
    else if( screenInfo.ScreenNumber == SCREEN_0_1 ) //PAUSE_BEFORE_POWER_S
    {
        if(( s_PowerReleased ) && ( powerTmrState == OS_TMR_STATE_COMPLETED ))
        {
            UIM_DeletePopUp( s_PowerPopUp );
        }
    }
    
    UIM_HandlePumpStatus( NUM_OF_PUMP_STATUS, false ); // Restore the status LED
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_CreatePowerTimer( int delay_ )
{
    ERROR_T status;
    TF_TIMER_DESC_T timer;
    static const char name[] = "UIM Power Timer";
    
    timer.ptmr = &s_PowerTimer;
    timer.dly = ( INT32U ) delay_;
    timer.period = ( INT32U ) 0;
    timer.opt = ( INT8U ) OS_TMR_OPT_ONE_SHOT;
    timer.callback = (OS_TMR_CALLBACK)UIM_PowerTimerCB;
    timer.callback_arg = ( void *) 0;
    timer.NAME = name;
    
    status = TF_CreateTimer( &timer );
    UIM_Trace( status, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_Initialize( void )
{
    
    GUI_SetFont( FONT_MAIN ); //This size of font is something considered "safe", but should change for each string.

    return ERR_NONE;

}

////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_StandbyPrep( void )
{
    ERROR_T err = ERR_NONE;
        
    //lint -e65 -e708 -e641 -e64 -e485 //Disable warnings again.
    s_CurrTablePtr = SPLASH_SCREEN_STATE_TABLE;
    s_CurrDrawArrPtr = LOGO_ATTRIBUTE_ARR;
    //lint +e65 +e708 +e641 +e64 +e485 //Enable warnings again.
    
    WM_DeleteWindow( s_PowerPopUp ); 
    WM_DeleteWindow( s_AlarmAlertPopUp );
    
    s_PowerPopUp = 0;
    s_AlarmAlertPopUp = 0;   

    if( s_UimBaseRedrawTmr != NULL )
    {
        err = TF_DeleteTimer( &s_UimBaseRedrawTmr );
        UIM_Trace(err,__LINE__);
    }

    if( s_PowerTimer != NULL )
    {
        err = TF_DeleteTimer( &s_PowerTimer );
        UIM_Trace(err,__LINE__);
    }

    if( s_PowerProgBarTimer != NULL )
    {
        err = TF_DeleteTimer( &s_PowerProgBarTimer );
        UIM_Trace(err,__LINE__);
    }

    if( s_PromptTimer != NULL )
    {
        err = TF_DeleteTimer( &s_PromptTimer );
        UIM_Trace(err,__LINE__);
    }

    if( s_ScreenTimeoutTmr != NULL )
    {
        err = TF_DeleteTimer( &s_ScreenTimeoutTmr );
        UIM_Trace(err,__LINE__);
    }
    
    s_IsPausePressed = false;
    s_PrevFlowRate = ML_0; 

    memset( &s_CurrProgram, 0, sizeof( s_CurrProgram ) );
    s_VolumeInfused = 0.0;
    s_CurrStep = 0; 
    s_HasPrimedFlag = false;
    s_AbortFlag = false;            
    s_PowerReleased = false;            

    memset( &s_Passcode, 0, sizeof( s_Passcode ));

    s_ProgramType =  PROGRAM_TYPE_MAX;
    s_CurrProgram.PatientWeight = PATIENT_WEIGHT_MAX;
    s_IsOffProgram = FALSE;
    s_ListBoxIndex = 0;

    // Entering into Standby mode. Stop Button back light and Status LED
    // Battery LED will remain as it is.
    err = LED_ControlButtonBackLightLed( (bool)OFF );
    UIM_Trace( err, __LINE__ );

    // Clear the status LED behavior
    UIM_SetAlarmLED( LED_CLEARED );

    return err;
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_PullBackComplete( void )
{
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.ID = BLOOD_IN_TUBING;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
    nextScreen.Type = ID;
    UIM_ResetListBoxIndex();          //Start Blood screen with zero listbox index  
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_StartPowerDown( void )
{
    SCREEN_FUNC_T nextScreen;
    ERROR_T status;
    TF_TIMER_DESC_T progBarTimer;
    static const char name[] = "UIM Power Progress Bar Timer";

    UIM_ResetProgressCounter();

    if( s_PowerTimer != NULL )
    {
        status = TF_DeleteTimer( &s_PowerTimer );
        UIM_Trace(status,__LINE__);
    }

    UIM_CreatePowerTimer( PWR_TOTAL_DELAY );
                   
    nextScreen.Type = ID;
    nextScreen.ID = POWER_OFF_S;
    UIM_ChangeScreen( &nextScreen, POWER_TYPE );
    LED_SetAlarmAlertNotification( LED_BLINKING_GREEN );

    if( s_PowerProgBarTimer != NULL )
    {
        status = TF_DeleteTimer( &s_PowerProgBarTimer );
        UIM_Trace(status,__LINE__);
    }

    progBarTimer.ptmr = &s_PowerProgBarTimer;
    progBarTimer.dly = (INT32U) 0;
    progBarTimer.period = ( INT32U ) PWR_PROG_DRAW_DELAY;
    progBarTimer.opt = ( INT8U ) OS_TMR_OPT_PERIODIC;
    progBarTimer.callback = ( OS_TMR_CALLBACK ) UIM_PowerRedrawTmrCB;
    progBarTimer.callback_arg = ( void * ) 0;
    progBarTimer.NAME = name;

    status = TF_CreateAndStartTimer( &progBarTimer );
    UIM_Trace( status, __LINE__ );

    status = TF_StartTimer( s_PowerTimer );
    UIM_Trace(status,__LINE__);
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_DenyPowerDown( void )
{
    SCREEN_FUNC_T nextScreen;
    ERROR_T status;
    
    if( s_PowerTimer != NULL )
    {
        status = TF_DeleteTimer( &s_PowerTimer );
        UIM_Trace(status,__LINE__);
    }

    UIM_CreatePowerTimer( PWR_TOTAL_DELAY );
    nextScreen.Type = ID;
    nextScreen.ID = PAUSE_BEFORE_POWER_S;
    
    UIM_ChangeScreen( &nextScreen, POWER_TYPE );

    status = TF_StartTimer( s_PowerTimer );
    UIM_Trace(status,__LINE__);
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SwitchChange( SWITCH_STATE_T switchEvent_ )
{    
    ERROR_T err = ERR_NONE;
    AM_MESSAGE_T amMsg;

    SCREEN_INFO_T screenInfo;
    SCREEN_FUNC_T nextScreen;
        
    amMsg.EventType = ALERT;
    amMsg.Alert = AUDIO_BUTTON;
    
        
    UIM_GetActivePopUpScreenInfo( &screenInfo );

    switch ( switchEvent_ )
    {
        case SWITCH_PBOUT_PRESSED:
            if( ( s_CurrTablePtr == SPLASH_SCREEN_STATE_TABLE ) &&
                    ( s_CurrDrawArrPtr == LOGO_ATTRIBUTE_ARR ) &&
                    screenInfo.ScreenNumber == SCREEN_BLANK)
            {
                UIM_RequestPowerOn();
            }
            break;

        case SWITCH_INFO_PRESSED:
            
            if( !s_ScreenReviewFlag )
            {
                err = AM_CommandNotify( &amMsg );
                UIM_Trace( err, __LINE__ );
                
                nextScreen = screenInfo.TABLE_PTR[screenInfo.SubStateScreen.ID][INFO];
                UIM_ChangeScreen( &nextScreen , screenInfo.ScreenType );
            }
            
            break;
        case SWITCH_POWER_PRESSED:     
            
            s_PowerReleased = false;

            if( !s_ScreenReviewFlag )
            {
                nextScreen = screenInfo.TABLE_PTR[screenInfo.SubStateScreen.ID][POWER];
            }
            else
            {
                nextScreen = screenInfo.TABLE_PTR[0][POWER]; //If in review state, all screens have the same function pointers.
            }
            UIM_ChangeScreen( &nextScreen , screenInfo.ScreenType );

            
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;
            
        case SWITCH_POWER_RELEASED:            

            s_PowerReleased = true;
            
            UIM_HandlePowerStateExit();
          
            break;

        case SWITCH_PAUSE_PRESSED:
            
            if( !s_ScreenReviewFlag )
            {
                err = AM_CommandNotify( &amMsg );
                UIM_Trace( err, __LINE__ );

                nextScreen = screenInfo.TABLE_PTR[screenInfo.SubStateScreen.ID][PAUSE];
                UIM_ChangeScreen( &nextScreen , screenInfo.ScreenType );
            }
            
            break;

        case SWITCH_BACK_PRESSED:

            if( !s_ScreenReviewFlag )
            {
                UIM_SetBackButtonListboxIndex();
                nextScreen = screenInfo.TABLE_PTR[screenInfo.SubStateScreen.ID][BACK];
                UIM_ChangeScreen( &nextScreen , screenInfo.ScreenType );
                
                amMsg.EventType = ALERT;
                amMsg.Alert = AUDIO_BUTTON;
                err = AM_CommandNotify( &amMsg );
                UIM_Trace( err, __LINE__ );
            }
            
            break;

        case SWITCH_UP_PRESSED:

            if( !s_ScreenReviewFlag )
            {
                nextScreen = screenInfo.TABLE_PTR[screenInfo.SubStateScreen.ID][UP];
            }
            else
            {
                nextScreen = screenInfo.TABLE_PTR[0][UP]; //If in review state, all screens have the same function pointers.
            }
            UIM_ChangeScreen( &nextScreen , screenInfo.ScreenType );
            
            amMsg.EventType = ALERT;
            amMsg.Alert = AUDIO_BUTTON;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );

            break;

        case SWITCH_OK_PRESSED:       
            if( !s_ScreenReviewFlag )
            {
                err = AM_CommandNotify( &amMsg );
                UIM_Trace( err, __LINE__ );

                nextScreen = screenInfo.TABLE_PTR[screenInfo.SubStateScreen.ID][SELECT];
                UIM_ChangeScreen( &nextScreen , screenInfo.ScreenType );
            }
            
            break;

        case SWITCH_DOWN_PRESSED:
          
            if( !s_ScreenReviewFlag )
            {
                nextScreen = screenInfo.TABLE_PTR[screenInfo.SubStateScreen.ID][DOWN];
            }
            else
            {
                nextScreen = screenInfo.TABLE_PTR[0][DOWN]; //If in review state, all screens have the same function pointers.
            }
            UIM_ChangeScreen( &nextScreen , screenInfo.ScreenType );
            
            amMsg.EventType = ALERT;
            amMsg.Alert = AUDIO_BUTTON;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;

        case SWITCH_START_PRESSED:
            if( !s_ScreenReviewFlag )
            {
                err = AM_CommandNotify( &amMsg );
                UIM_Trace( err, __LINE__ );

                nextScreen = screenInfo.TABLE_PTR[screenInfo.SubStateScreen.ID][START];
                UIM_ChangeScreen( &nextScreen , screenInfo.ScreenType );
            }
            
            break;

        case SWITCH_START_RELEASED: 
            
            if( !s_ScreenReviewFlag )
            {
                UIM_StopPrimeReq();
            }
      
            break;       
            
        default:
            break;
    }

}

////////////////////////////////////////////////////////////////////////////////
// This function is used to determine what screen to go to next when there are multiple options for Screen
static void UIM_DetermineBackPath( void )
{
    SCREEN_FUNC_T nextScreen;
    SCREEN_INFO_T screenInfo;
    WM_HWIN *win;   

    // Initialization
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;

    UIM_GetActiveWinAndScreenInfo( &screenInfo, &win );

    switch( screenInfo.ScreenNumber )
    {
        case SCREEN_12:          // PROGRAM_INFO1
        {
            LINESET_INFO_T needle;
            ERROR_T err;

            err = INF_GetLinesetInfo( &needle );
            UIM_Trace( err, __LINE__ );

            if( ( needle == SINGLE_NEEDLE_LINESET ) &&
                ( s_CurrProgram.ProgNeedleSet == BIFURCATED_NEEDLE_LINESET ) )
            {
                nextScreen.Type = ID;
                nextScreen.ID = NOTE_SINGLE_NEEDLE_DETECTED;
            }
            else if( ( needle == BIFURCATED_NEEDLE_LINESET ) &&
                   ( s_CurrProgram.ProgNeedleSet == SINGLE_NEEDLE_LINESET ) )
            {
                nextScreen.Type = ID;
                nextScreen.ID = NOTE_BI_NEEDLE_DETECTED;
            }
            else
            {
                nextScreen.Type = FP;
                nextScreen.FP = UIM_ReqAbortSysState;
            }
        }
        break;

        default:
            break;
    }

    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_StepChange( const TIME_CHANGE_DATA_T *timeChangeData_  )
{    
    SCREEN_FUNC_T nextScreen;
    ERROR_T err = ERR_NONE;
    AM_MESSAGE_T amMsg;

    if( timeChangeData_ == NULL )
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return;
    }

    s_CurrProgram.ProgStepTime[s_CurrStep] = timeChangeData_->StepTimeLeft;
    s_CurrProgram.TotalIgStepTime = timeChangeData_->TotalTimeLeft;
    
    if( s_CurrStep == HY_STEP_INDEX )
    {    
        nextScreen.Type = ID;    
        nextScreen.ID = IG_INF_PROG; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
        
        s_CurrStep = MIN( timeChangeData_->StepComplete, s_CurrProgram.ProgIgSteps );    
        
        UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
        
        // At HY to IG transition  ALERT tone
        amMsg.EventType = ALERT;
        amMsg.Alert = AUDIO_ALERT_NO_REPEAT;
        err = AM_CommandNotify( &amMsg );
        UIM_Trace( err, __LINE__ ); 
    }
    else
    {
        // Infusion Manager is sending completed step into time change info message
        // So to track current step UIM needs to add one into timeChangeData_.stepComplete
        s_CurrStep = MIN( timeChangeData_->StepComplete + 1, s_CurrProgram.ProgIgSteps ); 
    }

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SysStateChange( SSM_STATES_T state_ )
{
    SCREEN_FUNC_T nextScreen;
    ERROR_T err = ERR_NONE;
    uint8_t volume = 0;
    uint8_t brightness = 0;
    AM_MESSAGE_T amMsg;
    
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;

    switch(state_)
    {

        case SSM_STANDBY:
            UIM_StandbyPrep();
            nextScreen.ID = BLANK;
            nextScreen.Type = ID;
            s_CurrTablePtr = SPLASH_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = LOGO_ATTRIBUTE_ARR;
            break;

        case SSM_POST:
        {
            SSM_MESSAGE_T msg;

            // Always set the LED brightness and Audio Volume at startup
            //
            // Start Button LED again
            err = LED_ControlButtonBackLightLed( ( bool )ON );
            UIM_Trace( err, __LINE__ );

            // Volume back to normal configuration
            err = CFG_GetVolume( &volume );
            UIM_Trace( err, __LINE__ );
            
            UIM_SetDefaultVolumeLevel( volume );
            
            err = CFG_GetBrightness( &brightness );
            UIM_Trace( err, __LINE__ );
            
            UIM_SetDefaultBrightnessLevel( brightness );

            amMsg.EventType = VOLUME;
            amMsg.SaveVolFlag = FALSE;
            amMsg.Volume = volume;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );

            amMsg.EventType = ALERT;
            amMsg.Alert = AUDIO_PWR_ON;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );

            // Render the splash screen
            s_CurrTablePtr = SPLASH_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = LOGO_ATTRIBUTE_ARR;
            nextScreen.Type = ID;
            nextScreen.ID = LOGO;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup

            err = LM_LogLanguageFiles();
            UIM_Trace( err, __LINE__ );

            msg.MsgEvent = SSM_EVT_POST_COMPLETE;
            msg.TaskId = s_UimTaskDesc.taskId;
            SSM_CommandNotify(&msg);
        }
            break;

        case SSM_INF_RESTART:
            s_CurrTablePtr = MODE_SELCTION_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = MODE_ATTRIBUTE_ARR;
            nextScreen.Type = ID;
            nextScreen.ID = POWER_LOSS_RECOVERY;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            break;

        case SSM_MODE_SELECT:
            UIM_HandlePumpStatus( NUM_OF_PUMP_STATUS, false ); // Restore the status LED
            s_CurrTablePtr = MODE_SELCTION_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = MODE_ATTRIBUTE_ARR;
            nextScreen.Type = ID;
            nextScreen.ID = START_SETUP;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            break;

        case SSM_SETTINGS:
            err = LED_SetAlarmAlertNotification( LED_CLEARED );
            UIM_Trace( err, __LINE__ );

            s_CurrTablePtr = SETUP_MENU_TABLE;
            s_CurrDrawArrPtr = SETUP_MENU_ARR;
            UIM_ResetListBoxIndex();
            s_Passcode.PasscodeValue = DEFAULT_PASSCODE_VALUE;
            nextScreen.Type = ID;
            nextScreen.ID = PASSCODE_SCREEN;//lint !e641 Ignoring conversion warning due to the way the multi state table is se
            break;

        case SSM_SHUTDOWN:
            if( s_PowerProgBarTimer != NULL  )
            {
                err = TF_StopAndDeleteTimer( &s_PowerProgBarTimer );
                UIM_Trace(err,__LINE__);
            }

            if( s_PowerTimer )
            {
                err = TF_StopAndDeleteTimer( &s_PowerTimer );
                UIM_Trace(err,__LINE__);
            }

            nextScreen.Type = ID;
            nextScreen.ID = BLANK;
            s_CurrTablePtr = SPLASH_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = LOGO_ATTRIBUTE_ARR;

            // Clear the LCD and Button LEDs
            err = SPI_DisplayClear();
            UIM_Trace( err, __LINE__ );

            // Change to blank screen now
            UIM_ChangeScreen( &nextScreen , INFUSION_TYPE );

            // Delete the power popup window if active
            if( s_PowerPopUp )
            {
                UIM_DeletePopUp( s_PowerPopUp );
            }

            // Play audible confirmation
            amMsg.EventType = ALERT;
            amMsg.Alert = AUDIO_PWR_OFF;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );

            err = LED_ControlBatteryLed( (bool)OFF );
            UIM_Trace( err, __LINE__ );

            err = LED_ControlButtonBackLightLed( (bool)OFF );
            UIM_Trace( err, __LINE__ );

            // Clear the status LED behavior
            LED_SetAlarmAlertNotification( LED_CLEARED );

            // Stop Button Sound
            amMsg.EventType = VOLUME;
            amMsg.SaveVolFlag = FALSE;
            amMsg.Volume = 0;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            break;

        case SSM_INFUSION:
        case SSM_FAIL_SAFE:
        default:
            break;
    }

    UIM_ChangeScreen( &nextScreen , INFUSION_TYPE );

    s_AbortFlag = false;
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_StateChange( INF_STATE_T state_ )
{
    ERROR_T err = ERR_NONE;
    
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;
    
    bool flushFlag = false;
    bool siteCheckFlag = false;

    switch ( state_ )
    {
        case INF_STATE_LINESET_CHECK:
            s_CurrTablePtr = LINESET_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = LINESET_ATTRIBUTE_ARR;
            UIM_ResetListBoxIndex();

            LINESET_INFO_T needle;
            err = INF_GetLinesetInfo( &needle );
            UIM_Trace( err, __LINE__ );

            DOOR_INFO_T door;
            err = INF_GetDoorInfo( &door );
            UIM_Trace( err, __LINE__ );

            if( ( ( needle == SINGLE_NEEDLE_LINESET ) || ( needle == BIFURCATED_NEEDLE_LINESET ) ) && ( door == DOOR_CLOSE ) )
            {
                nextScreen.Type = ID;
                nextScreen.ID = ATTACH_HY;          //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            }
            else
            {
                nextScreen.Type = ID;
                nextScreen.ID = INSERT_INFUSION_SET;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            }
            break;

        case INF_STATE_INFUSION_SETUP:
            s_CurrTablePtr = SETUP_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = SETUP_ATTRIBUTE_ARR;

            if( s_AbortFlag )//The abort flag is used in scenarios where a previous state is required, but the screen needed is not
            {                 //the first screen of the new state.
                nextScreen.Type = ID;
                if ( s_CurrProgram.ProgIgSteps > NUM_STEP_ROWS_PER_SCR )
                {
                    nextScreen.ID = STEP_INFO2;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
                }
                else
                {
                    nextScreen.ID = STEP_INFO1;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
                }
            }
            else
            {
                nextScreen.Type = FP;
                nextScreen.FP = UIM_NeedleSetCheckScreen;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            }
            break;

        case INF_STATE_AUTO_PRIME:
            s_CurrTablePtr = PRIMING1_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = PRIMING_AUTO_ATTRIBUTE_ARR;
            nextScreen.Type = ID;
            nextScreen.ID = PRIMING_STEP1; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            break;

        case INF_STATE_MANUAL_PRIME:
            s_CurrTablePtr = PRIMING2_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = PRIMING_MAN_ATTRIBUTE_ARR;
            nextScreen.Type = ID;
            nextScreen.ID = PRIMING_STEP2;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            break;

        case INF_STATE_NEEDLE_SITE_CHECK:
            s_CurrTablePtr = SITE_CHECK_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = SITECHECK_ATTRIBUTE_ARR;

            err = PM_GetNeedlePlacementCheckEnabled( &siteCheckFlag );
            UIM_Trace( err, __LINE__ );

            if(( s_AbortFlag ) &&
               ( siteCheckFlag ))
            {
                UIM_ResetListBoxIndex();
                nextScreen.Type = ID;
                nextScreen.ID = BLOOD_IN_TUBING; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            }
            else
            {
                nextScreen.Type = ID;
                nextScreen.ID = INSERT_NEEDLE; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            }
            break;

        case INF_STATE_INFUSION_PENDING:
            s_CurrTablePtr = INFUSION_DELIVERY_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = INFUSION_ATTRIBUTE_ARR;

           nextScreen.Type = ID;
           nextScreen.ID = START_HY; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            break;

        case INF_STATE_FLUSH:
            s_CurrTablePtr = FLUSH_STATE_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = FLUSH_ATTRIBUTE_ARR;

            err = PM_GetFlushEnabled( &flushFlag );
            UIM_Trace( err, __LINE__ );

            if( flushFlag )
            {
                nextScreen.Type = ID;
                nextScreen.ID = CONNECT_FLUSH;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            }
            else
            {
                nextScreen.Type = ID;
                nextScreen.ID = REMOVE_NEEDLE_FLUSH;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            }
            break;

        case INF_STATE_INFUSION_DELIVERY: //Ideally This state should only be reached with aborting from flush state.
            s_CurrTablePtr   = INFUSION_DELIVERY_SCREEN_STATE_TABLE;
            s_CurrDrawArrPtr = INFUSION_ATTRIBUTE_ARR;
            nextScreen.Type = ID;
            if( s_AbortFlag != true ) //When in case we got the event apart from aborting flush state then set the screen accordingly 
            {
                if( s_CurrStep == HY_STEP_INDEX )
                {
                    nextScreen.ID = INF_PROG;
                }
                else
                {
                    nextScreen.ID = IG_INF_PROG;
                }
            }
            break;

        default:
            UIM_Trace( ERR_PARAMETER, __LINE__ );
            break;
    }

    UIM_ChangeScreen( &nextScreen , INFUSION_TYPE );

    s_AbortFlag = false;

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_HandlePumpStatus( PUMP_STATUS_T pumpStatus_, bool newStatusFlag_ )
{
    ERROR_T err = ERR_NONE;
    SCREEN_INFO_T screenInfo;
    static PUMP_STATUS_T s_PumpStatus = NUM_OF_PUMP_STATUS;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );

    if( newStatusFlag_ )
    {
        s_PumpStatus = pumpStatus_;
    }
    
    if( s_PowerPopUp == 0 )
    {
        // On Screen 32_2 we need to show that the pump is moving so the status LED
        // should reflect pump status, not alarm status. On 32_3 and 32_4 we want
        // to show the green status LED for pump OK.
        if( ( s_AlarmAlertPopUp != 0 ) &&
                ( screenInfo.ScreenNumber != SCREEN_32_2 ) &&
                ( screenInfo.ScreenNumber != SCREEN_32_3 ) &&
                ( screenInfo.ScreenNumber != SCREEN_32_4 ) )
        {
            LED_SetAlarmAlertNotification( s_AlarmLEDStatus );
        }
        else if( ( s_CurrTablePtr == SETUP_MENU_TABLE ) && ( s_AbortFlag == false ) )
        {
            // Clear the status LED behavior
            //Make always LED off in setting mode
            LED_SetAlarmAlertNotification( LED_CLEARED );
        }
        else
        {
            switch ( s_PumpStatus )
            {
                case PUMP_IDLE:
                    
                    err = LED_SetAlarmAlertNotification( LED_SOLID_GREEN );
                    UIM_Trace( err, __LINE__ );
                    
                    if ( s_IsPausePressed )
                    {
                        UIM_CreateBaseRedrawTmr();      //This is required to update Battery percentage during pause screen SCREEN_30_3I
                    }
                    else
                    {
                        if ( s_UimBaseRedrawTmr != NULL )
                        {
                            err = TF_StopTimer( s_UimBaseRedrawTmr );
                            UIM_Trace( err, __LINE__ );
                        }
                    }
                    break;
                    
                case PUMP_MOVING:
                    
                    err = LED_SetAlarmAlertNotification( LED_BLINKING_GREEN );
                    UIM_Trace( err, __LINE__ );
                    
                    UIM_CreateBaseRedrawTmr();
                    break;
                    
                case NUM_OF_PUMP_STATUS:
                    // This is a do-nothing case for refreshing the LED.
                    break;

                default:
                    UIM_Trace( ERR_PARAMETER, __LINE__ );
                    break;
            }
        }
    }
    
}

////////////////////////////////////////////////////////////////////////////////
//Prepare Alarm/Alert screen and notify Audio Manager and LED manager
static void UIM_HandleAA( ALRM_ALRT_NOTI_T aa_ )
{    
    
    ERROR_T err = ERR_NONE;
    WM_HWIN window;
    TF_TIMER_DESC_T timer;
    AM_MESSAGE_T amMsg;

    SCREEN_FUNC_T nextScreen;
    SCREEN_INFO_T screenInfo;
    amMsg.EventType = ALERT;
    
    // Clear the LED pattern timer whenever the active alarm changes. It will be
    // restarted in the case handler if needed.
    if( s_UimLedPatternTmr != NULL )
    {
        err = TF_StopAndDeleteTimer( &s_UimLedPatternTmr );
        UIM_Trace(err,__LINE__);
    }

    switch( aa_ )
    {                  

        case ALL_CLEAR:
            
            if( s_UimPopupRedrawTmr !=  NULL )
            {
                // No longer needed.
                err = TF_DeleteTimer( &s_UimPopupRedrawTmr );
                UIM_Trace(err,__LINE__);
            }

            err = LED_SetAlarmAlertNotification( LED_CLEARED );
            UIM_Trace( err, __LINE__ );
            
            amMsg.Alert = AUDIO_NONE;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            if( s_AlarmAlertPopUp != 0 )
            {
                UIM_DeletePopUp( s_AlarmAlertPopUp );   //Only delete the alarm popup since the power popup could be running
                                                        //when an alarm/alert/notification comes through.
            }
            else
            {
                // Redraw screen
                window = WM_GetClientWindow( s_BaseWindow );
                WINDOW_GetUserData( window, ( void *)&screenInfo, sizeof( screenInfo ));
                UIM_ChangeScreen( &screenInfo.SubStateScreen, screenInfo.ScreenType );
            }

            UIM_HandlePumpStatus( NUM_OF_PUMP_STATUS, false );
            
            break;
        
        case LINESET_ALARM:
            window = WM_GetClientWindow( s_BaseWindow );
            WINDOW_GetUserData( window, ( void *)&screenInfo, sizeof( screenInfo ));
            
            // Do not show the alarm on these screens since the user is intentionally
            // opening the door.
            if( ( screenInfo.ScreenNumber != SCREEN_22   ) && // REMOVE_NEEDLE_BLOOD
                ( screenInfo.ScreenNumber != SCREEN_22_1 ) )  // REPLACE_NEEDLE
            {
                nextScreen.Type = ID;
                nextScreen.ID = NO_SET_S;
                UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
                UIM_SetAlarmLED( LED_BLINKING_RED );

                amMsg.Alert = AUDIO_ALARM;
                err = AM_CommandNotify( &amMsg );
                UIM_Trace( err, __LINE__ );
            }
            
            break;
            
        case LOW_BATT:
            
            nextScreen.Type = ID;
            nextScreen.ID = LOW_BATT_MINS_REMAIN_S;
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );

            window = WM_GetClientWindow( s_BaseWindow );
            WINDOW_GetUserData( window, ( void *)&screenInfo, sizeof( screenInfo ));

            // Special case when pump is moving. The UI will change LED between yellow and
            // flashing green while also triggering the Audio alert tones when appropriate.
            // UIM controls both LED and Audio to maintain synchronization.
            switch( screenInfo.ScreenNumber)
            {
                case SCREEN_29_1:
                case SCREEN_29_2:
                case SCREEN_29_3:
                case SCREEN_29_1I:
                case SCREEN_29_2I:
                case SCREEN_29_3I:
                case SCREEN_29_4I:
                case SCREEN_29_5I:
                    timer.ptmr = &s_UimLedPatternTmr;
                    timer.dly = ( INT32U ) LED_PATTERN_TIMEOUT_YELLOW;
                    timer.period = ( INT32U ) 0;
                    timer.opt = ( INT8U ) OS_TMR_OPT_ONE_SHOT;
                    timer.callback = ( OS_TMR_CALLBACK ) UIM_LedPatternCB;
                    timer.callback_arg = ( void * ) 0;
                    timer.NAME = "LED Pattern";

                    amMsg.Alert = AUDIO_ALERT_NO_REPEAT;
                    err = AM_CommandNotify( &amMsg );
                    UIM_Trace( err, __LINE__ );

                    err = TF_CreateAndStartTimer( &timer );                //Triggers screen timeout event
                    UIM_Trace(err,__LINE__);
                    break;

                default:
                    amMsg.Alert = AUDIO_ALERT;
                    err = AM_CommandNotify( &amMsg );
                    UIM_Trace( err, __LINE__ );
                    break;
            }
            
            UIM_SetAlarmLED( LED_SOLID_YELLOW );
            
            break;
            
        case EXH_BATT:
            
            nextScreen.Type = ID;
            nextScreen.ID = LOW_BATT_CONN_CHARGE_S;
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
            
            UIM_SetAlarmLED( LED_BLINKING_RED );
            
            amMsg.Alert = AUDIO_ALARM;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;
            
        case SHUTDOWN_BATT:

            nextScreen.Type = ID;
            nextScreen.ID = SHUT_DOWN_CONN_CHARGE_S;  
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
            
            UIM_SetAlarmLED( LED_BLINKING_RED );

            amMsg.Alert = AUDIO_ALARM;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
                            
            break;
            
        case REPLACE_BATT:
            
            nextScreen.Type = ID;
            nextScreen.ID = REPLACE_BATT_S;  
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
            
            UIM_SetAlarmLED( LED_SOLID_YELLOW );

            amMsg.Alert = AUDIO_ALERT;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;
            
        case STEP_ENDING:   //This is only sent when current program is off program

            // Stop this timer so that the selection highlight doesn't periodically reset during redraw
            if( s_UimPopupRedrawTmr !=  NULL )
            {                
                err = TF_DeleteTimer( &s_UimPopupRedrawTmr );
                UIM_Trace(err,__LINE__);
            }

            UIM_ResetListBoxIndex();
            nextScreen.Type = ID;
            nextScreen.ID = STEP_ENDING_S;
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );

            UIM_SetAlarmLED( LED_BLINKING_GREEN );
            
            amMsg.Alert = AUDIO_ALERT_NO_REPEAT;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;
            
        case NEAR_END_INF:
        {
            static const char name[] = "UIM Clear Near End Inf";
            nextScreen.Type = ID;
            nextScreen.ID = NEAR_END_INF_S;
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );

            if( s_UimPopupRedrawTmr != NULL )
            {
                err = TF_DeleteTimer( &s_UimPopupRedrawTmr );                 // Delete this timer in case it was in use
                UIM_Trace(err,__LINE__);
            }

            UIM_CreatePopupRedrawTmr( (uint32_t)INF_ARROW_DELAY );

            if( s_ScreenTimeoutTmr != NULL )
            {
                err = TF_DeleteTimer( &s_ScreenTimeoutTmr );                 // Delete this timer in case it was in use
                UIM_Trace(err,__LINE__);
            }

            timer.ptmr = &s_ScreenTimeoutTmr;
            timer.dly = ( INT32U ) NEAR_END_INF_TIMEOUT;
            timer.period = ( INT32U ) 0;
            timer.opt = ( INT8U ) OS_TMR_OPT_ONE_SHOT;
            timer.callback = ( OS_TMR_CALLBACK ) UIM_ClearNearEndOfInfScreenCB;
            timer.callback_arg = ( void * ) 0;
            timer.NAME = name;

            err = TF_CreateAndStartTimer( &timer );                //Triggers screen timeout event
            UIM_Trace(err,__LINE__);
            
            UIM_SetAlarmLED( LED_SOLID_YELLOW );

            amMsg.Alert = AUDIO_ALERT;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );

            break;
        }
        case AIL:
 
            nextScreen.Type = ID;
            nextScreen.ID = AIL_S;
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
            
            if( s_UimPopupRedrawTmr != NULL )
            {
                err = TF_DeleteTimer( &s_UimPopupRedrawTmr );                 // Delete this timer in case it was in use
                UIM_Trace(err,__LINE__);
            }

            UIM_CreatePopupRedrawTmr( (uint32_t)( AUTO_PRIMING_DURATION / MAX_NUM_RIGHT_ARROWS ) );
            UIM_SetAlarmLED( LED_BLINKING_RED );
            
            amMsg.Alert = AUDIO_ALARM;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;
            
        case OCC:

            nextScreen.Type = ID;
            nextScreen.ID = OCC_S;
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
            UIM_SetAlarmLED( LED_BLINKING_RED );
            
            amMsg.Alert = AUDIO_ALARM;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;
            
        case END_OF_HY:
            
            // Stop this timer so that the selection highlight doesn't periodically reset during redraw
            if( s_UimPopupRedrawTmr !=  NULL )
            {
                err = TF_DeleteTimer( &s_UimPopupRedrawTmr );
                UIM_Trace(err,__LINE__);
            }

            nextScreen.Type = ID;
            nextScreen.ID = HY_COMPLETE_S;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup

            UIM_ResetListBoxIndex();
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );

            UIM_SetAlarmLED( LED_BLINKING_RED );
            
            amMsg.Alert = AUDIO_ALARM;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;

         case END_OF_FLUSH:
             // Stop this timer so that the selection highlight doesn't periodically reset during redraw
             if( s_UimPopupRedrawTmr !=  NULL )
             {
                 err = TF_DeleteTimer( &s_UimPopupRedrawTmr );
                 UIM_Trace(err,__LINE__);
             }

            nextScreen.Type = ID;
            nextScreen.ID = FLUSH_COMPLETE;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup

            UIM_ResetListBoxIndex();
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );

            UIM_SetAlarmLED( LED_BLINKING_RED );
            
            amMsg.Alert = AUDIO_ALARM;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;
            
        case END_OF_IG: 
            
            // Stop this timer so that the selection highlight doesn't periodically reset during redraw
            if( s_UimPopupRedrawTmr !=  NULL )
            {
                err = TF_DeleteTimer( &s_UimPopupRedrawTmr );
                UIM_Trace(err,__LINE__);
            }
            
            nextScreen.Type = ID;
            nextScreen.ID = IG_COMPLETE_S;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
            
            UIM_ResetListBoxIndex();
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );

            UIM_SetAlarmLED( LED_BLINKING_RED );
            
            amMsg.Alert = AUDIO_ALARM;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;
                
            
        case PRIMING_ERR:
            nextScreen.Type = ID;
            nextScreen.ID = PRIME_ERROR_S;
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
            
            UIM_SetAlarmLED( LED_BLINKING_RED );
            
            amMsg.Alert = AUDIO_ALARM;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;
            
        case NO_ACTION:
          
            //Currently this status is sent from infusion manager after paused for 3 minutes
            UIM_SetAlarmLED( LED_SOLID_YELLOW );

            amMsg.EventType = ALERT;
            amMsg.Alert = AUDIO_ALERT;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );

            UIM_GetActivePopUpScreenInfo( &screenInfo );
            if( screenInfo.ScreenNumber == SCREEN_6 )   //INSERT_INFUSION_SET
            {
                nextScreen.Type = ID;
                nextScreen.ID = NO_SET_S;
                UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
            }
            else
            {
                UIM_DeletePopUp( s_AlarmAlertPopUp );
            }

            if ( !s_IsPausePressed )
            {
                if ( s_UimBaseRedrawTmr != NULL )
                {
                    err = TF_StopTimer( s_UimBaseRedrawTmr );
                    UIM_Trace( err, __LINE__ );
                }
            }
            
            break;
            
        case PUMP_ERROR_ALARM:
             
            nextScreen.Type = ID;
            nextScreen.ID =  PUMP_ERROR; //SCREEN_70
     
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );

            UIM_SetAlarmLED( LED_BLINKING_RED );
            
            amMsg.EventType = ALERT;
            amMsg.Alert = AUDIO_ALARM;
            err = AM_CommandNotify( &amMsg );
            UIM_Trace( err, __LINE__ );
            
            break;
         
        default:
            //No screen associated with other alarms. Do not change screens.
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_HandleTimeChange( const UIM_EVENT_MESSAGE_T *event_ )
{
    SCREEN_INFO_T screenInfo;
    
    if( event_ == NULL )
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return;
    }

    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    s_CurrProgram.ProgStepTime[s_CurrStep] = event_->TimeChangeData.StepTimeLeft;
    s_VolumeInfused = event_->TimeChangeData.VolumeInfused;
    
    s_CurrProgram.TotalInfusionTime = event_->TimeChangeData.TotalTimeLeft;
    UIM_ChangeScreen( &screenInfo.SubStateScreen , screenInfo.ScreenType );
    
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_GetActivePopUpScreenInfo( SCREEN_INFO_T *screenInfo_ )
{
    WM_HWIN *activePopUp;
    WM_HWIN window;
       
    if( s_PowerPopUp > 0 )
    {
        activePopUp = &s_PowerPopUp;
    }    
    else if( s_AlarmAlertPopUp > 0 )
    {
        activePopUp = &s_AlarmAlertPopUp;
    }
    else 
    {
        activePopUp = &s_BaseWindow;
    }
       
    window = WM_GetClientWindow( *activePopUp );
    WINDOW_GetUserData( window, ( void *)screenInfo_, sizeof( *screenInfo_ ));
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_GetActiveWinAndScreenInfo( SCREEN_INFO_T *screenInfo_, WM_HWIN **win_ )
{
    WM_HWIN window;

    if( ( screenInfo_ == NULL ) || ( win_ == NULL ) )
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return;
    }

    if( s_PowerPopUp > 0 )
    {
        *win_ = &s_PowerPopUp;
    }
    else if( s_AlarmAlertPopUp > 0 )
    {
        *win_ = &s_AlarmAlertPopUp;
    }
    else
    {
        *win_ = &s_BaseWindow;
    }

    window = WM_GetClientWindow( *( *win_ ) );
    WINDOW_GetUserData( window, ( void *)screenInfo_, sizeof( *screenInfo_ ));
}

////////////////////////////////////////////////////////////////////////////////
// The power pop up has precedence over the alarm/alert/notification pop up while the standard infusion screen is the lowest priority.
static void UIM_DeletePopUp( WM_HWIN popUp_ )
{
    WM_HWIN window = 0;
    
    SCREEN_INFO_T screenInfo;
    
    window = WM_GetClientWindow( popUp_ );
    WINDOW_GetUserData( window, ( void *)&screenInfo, sizeof( screenInfo ));

    if( screenInfo.ScreenType == POWER_TYPE )    
    {
        WM_DeleteWindow( s_PowerPopUp ); 
        s_PowerPopUp = 0;
        
        if( s_AlarmAlertPopUp > 0 )
        {
            WM_SetFocus( s_AlarmAlertPopUp );
        }
        else
        {
            WM_SetFocus( s_BaseWindow );
        }
    }
    else if( screenInfo.ScreenType == ALARM_TYPE )
    {
         WM_DeleteWindow( s_AlarmAlertPopUp ); 
         s_AlarmAlertPopUp = 0;
         
        if( s_PowerPopUp > 0 )
        {
            WM_SetFocus( s_PowerPopUp );
        }
        else
        {
            WM_SetFocus( s_BaseWindow );
        }
    }
    
    GUI_Delay( GUI_DELAY_AMOUNT );
    
}

////////////////////////////////////////////////////////////////////////////////
//This function determines what screen template should be used, sets up the windows, variables for screen iterators,  
//determines if the screen is an info screen or if it has an associated info screen, and calls the nessecary function pointer
//if needed.
static void UIM_ChangeScreen( const SCREEN_FUNC_T *screen_, SCREEN_T screenType_ )
{
    bool sameScreenFlag = false;
    SCREEN_ID_T screenNumber;
    TEMPLATE_ID_T templateNumber;
    INFO_T infoEnum;
    
    int numOfBytesWritten = 0;
    
    WM_HWIN window;
    
    if( screen_ == NULL )
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return;
    }

    SCREEN_INFO_T screenInfo;
    
    if(( screen_->Type == ID ) &&
       ( screen_->ID != NO_CHANGE ))//This ensures that the state never becomes NO_CHANGE. NO_CHANGE is more or less used to show
    {                              //that nothing will happen versus being an actual screen/state.
        
        switch( screenType_ )
        {
            case INFUSION_TYPE:
                
                screenNumber = s_CurrDrawArrPtr[ screen_->ID ].St[0].ScreenId;
                templateNumber = s_CurrDrawArrPtr[ screen_->ID ].St[1].TemplateId;
                infoEnum = s_CurrDrawArrPtr[ screen_->ID ].Info;
                
                if( s_BaseWindow > 0 )
                {
                    UIM_GetActivePopUpScreenInfo( &screenInfo );
                    if( screenInfo.ScreenNumber == screenNumber )
                    {
                        sameScreenFlag = true;
                    }
                    else
                    {
                        sameScreenFlag = false;
                    }
                }
                
                WM_DeleteWindow( s_BaseWindow ); 
                              
                s_BaseWindow = UIM_DrawScreen( screenNumber, templateNumber, infoEnum, sameScreenFlag );
                                
                screenInfo.ScreenNumber = screenNumber;
                screenInfo.SubStateScreen = *screen_;
                screenInfo.TABLE_PTR = s_CurrTablePtr;
                screenInfo.ScreenType = INFUSION_TYPE;
                
                window = WM_GetClientWindow( s_BaseWindow );
                
                numOfBytesWritten = WINDOW_SetUserData( window, (void*)&screenInfo , sizeof( screenInfo ));
                
                if( numOfBytesWritten == 0 )
                {
                    UIM_Trace( ERR_GUI, __LINE__ );
                }
                
                if(( s_AlarmAlertPopUp == 0 ) && ( s_PowerPopUp == 0 ))
                {
                    GUI_Delay( GUI_DELAY_AMOUNT );                    
                }
                
            break;
            
            case ALARM_TYPE:
                
                screenNumber = ALARM_ALERT_ATTRIBUTE_ARR[ screen_->ID ].St[0].ScreenId;
                templateNumber = ALARM_ALERT_ATTRIBUTE_ARR[ screen_->ID ].St[1].TemplateId;
                infoEnum = ALARM_ALERT_ATTRIBUTE_ARR[ screen_->ID ].Info;
                
                if( s_AlarmAlertPopUp > 0 )
                {
                    UIM_GetActivePopUpScreenInfo( &screenInfo );
                    if( screenInfo.ScreenNumber == screenNumber )
                    {
                        sameScreenFlag = true;
                    }
                    else
                    {
                        sameScreenFlag = false;
                    }
                }
    
                WM_DeleteWindow( s_AlarmAlertPopUp ); 
                            
                s_AlarmAlertPopUp = UIM_DrawScreen( screenNumber, templateNumber, infoEnum, sameScreenFlag );
                
                screenInfo.ScreenNumber = screenNumber;
                screenInfo.SubStateScreen = *screen_;
                screenInfo.TABLE_PTR = ALARMS_ALERTS_TABLE;
                screenInfo.ScreenType = ALARM_TYPE;
                
                window = WM_GetClientWindow( s_AlarmAlertPopUp );
                
                numOfBytesWritten = WINDOW_SetUserData( window, (void*)&screenInfo , sizeof( screenInfo ) );
                
                if( numOfBytesWritten == 0 )
                {
                    UIM_Trace( ERR_GUI, __LINE__ );
                }
                
                if( s_PowerPopUp == 0 )
                {
                    GUI_Delay( GUI_DELAY_AMOUNT );
                    
                }
                
            break;
            
            case POWER_TYPE:
                
                screenNumber = POWER_ATTRIBUTE_ARR[ screen_->ID ].St[0].ScreenId;
                templateNumber = POWER_ATTRIBUTE_ARR[ screen_->ID ].St[1].TemplateId;
                infoEnum = POWER_ATTRIBUTE_ARR[ screen_->ID ].Info;
                
                if( s_PowerPopUp > 0 )
                {
                    UIM_GetActivePopUpScreenInfo( &screenInfo );
                    if( screenInfo.ScreenNumber == screenNumber )
                    {
                        sameScreenFlag = true;
                    }
                    else
                    {
                        sameScreenFlag = false;
                    }
                }
                
                WM_DeleteWindow( s_PowerPopUp ); 
          
                
                s_PowerPopUp = UIM_DrawScreen( screenNumber, templateNumber, infoEnum, sameScreenFlag );
                
                screenInfo.ScreenNumber = screenNumber;
                screenInfo.SubStateScreen = *screen_;
                screenInfo.TABLE_PTR = POWER_TABLE;
                screenInfo.ScreenType = POWER_TYPE;
                
                window = WM_GetClientWindow( s_PowerPopUp );
                
                numOfBytesWritten = WINDOW_SetUserData( window, (void*)&screenInfo , sizeof( screenInfo ) );
                
                if( numOfBytesWritten == 0 )
                {
                    UIM_Trace( ERR_GUI, __LINE__ );
                }
                
                GUI_Delay( GUI_DELAY_AMOUNT );
                
                
            break;
            
            default:
                break;
        }
        

    }
    else if( screen_->Type == FP )
    {
        (screen_)->FP();
    }

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ReqNextState( void )
{
    ERROR_T err = ERR_NONE;
    INF_MESSAGE_T infMsg;

    infMsg.InfEventType = INF_STATE_FINISHED;

    err = INF_SendMessage( &infMsg );
    UIM_Trace( err, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ReqAbortSiteCheck( void )
{
    // The UIM is not supposed to know where to go after aborting a state, but here
    // we need to set this flag so the appropriate screen is rendered after state change.
    //
    // Set s_HasPrimedFlag. This flag was cleared upon exiting manual prime, but
    // we want it restored when returning to manual prime.
    s_HasPrimedFlag = true;

    UIM_ReqAbortState();
}

////////////////////////////////////////////////////////////////////////////////
//This is used when the back button is pressed and a infusion state change is required.
static void UIM_ReqAbortState( void )
{
    ERROR_T err = ERR_NONE;
    INF_MESSAGE_T infMsg;

    infMsg.InfEventType = INF_ABORT;
    s_AbortFlag = true;

    err = INF_SendMessage( &infMsg );
    UIM_Trace( err, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
//This is used when the back button is pressed and a system state change is required.
static void UIM_ReqAbortSysState( void )
{
    ERROR_T err = ERR_NONE;
    SSM_MESSAGE_T ssMsg;
    
    (void)memset( (void*)&ssMsg, 0, sizeof(SSM_MESSAGE_T) );

    s_AbortFlag = true;
    ssMsg.MsgEvent = SSM_EVT_STATE_ABORT;
    err = SSM_CommandNotify( &ssMsg );
    UIM_Trace( err, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_AutoPrimeReq()
{
    ERROR_T err = ERR_NONE;
    INF_MESSAGE_T infMsg;
    SCREEN_FUNC_T nextScreen;

    nextScreen.Type = ID;
    nextScreen.ID = FILLING1; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup

    infMsg.InfEventType = INF_REQ_MOTOR_START;

    err = INF_SendMessage( &infMsg );
    UIM_Trace( err, __LINE__ );

    UIM_ChangeScreen( &nextScreen , INFUSION_TYPE );
    UIM_Trace( err, __LINE__ );

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ManPrimeReq()
{
    ERROR_T err = ERR_NONE;
    INF_MESSAGE_T infMsg;
    SCREEN_FUNC_T nextScreen;    
    SCREEN_INFO_T screenInfo;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );

    nextScreen.Type = ID;
    if ( screenInfo.ScreenType == ALARM_TYPE )
    {
        nextScreen.ID = CLEARING_AIR; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
    }
    else if( screenInfo.ScreenType == INFUSION_TYPE )
    {
        nextScreen.ID = FILLING2; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
    }

    infMsg.InfEventType = INF_REQ_MOTOR_START;

    err = INF_SendMessage( &infMsg );
    UIM_Trace( err, __LINE__ );

    UIM_ChangeScreen( &nextScreen , screenInfo.ScreenType );
    UIM_Trace( err, __LINE__ );

    s_HasPrimedFlag = true;

}

////////////////////////////////////////////////////////////////////////////////
//This function handles both manual priming screens along with AIL screens
static void UIM_StopPrimeReq()
{
    ERROR_T err = ERR_NONE;
    SCREEN_FUNC_T nextScreen;
    INF_MESSAGE_T infMsg;
    
    SCREEN_INFO_T screenInfo;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    if(( screenInfo.ScreenNumber == SCREEN_32_2 ) ||  //CLEARING_AIR
       (( screenInfo.ScreenNumber == SCREEN_17 ) && s_HasPrimedFlag )) //FILLING
    {

        nextScreen.Type = ID; 
        if( screenInfo.ScreenNumber == SCREEN_32_2 ) //CLEARING_AIR
        {
            nextScreen.ID = AIL_ACTION_DONE;   //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
        }
        else if( screenInfo.ScreenNumber == SCREEN_17 )
        {
            nextScreen.ID = PRIMING_STEP2; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
        }

        infMsg.InfEventType = INF_REQ_MOTOR_STOP;

        err = INF_SendMessage( &infMsg );
        UIM_Trace( err, __LINE__ );

        UIM_ChangeScreen( &nextScreen , screenInfo.ScreenType );
        UIM_Trace( err, __LINE__ );
    }

}


////////////////////////////////////////////////////////////////////////////////
static void UIM_CheckPriming()
{
    SCREEN_FUNC_T nextScreen;
    SCREEN_INFO_T screenInfo;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    if( s_HasPrimedFlag )         //Manual Priming must have occurred at least once for the next state to be allowed.
    {
        s_HasPrimedFlag = false; //Reset the flag b/c the next time it is checked the system should prime again.
        
        if( screenInfo.ScreenType == INFUSION_TYPE )
        {
            UIM_ReqNextState();
        }
        else if( screenInfo.ScreenType == ALARM_TYPE )
        {
            nextScreen.Type = ID;
            nextScreen.ID = AIL_RECONNECT_NEEDLE;
            UIM_ChangeScreen( &nextScreen , screenInfo.ScreenType );
        }

    }

}

////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetPrimedFlag( bool *flag_ )
{
    if( flag_ == NULL )
    {
        return ERR_PARAMETER;
    }
    else
    {
        *flag_ = s_HasPrimedFlag;
    }
    return ERR_NONE;
}


////////////////////////////////////////////////////////////////////////////////
static void UIM_ReqPullBack( void )
{
    ERROR_T err = ERR_NONE;
    SCREEN_FUNC_T nextScreen;
    INF_MESSAGE_T infMsg;

    nextScreen.Type = ID;
    nextScreen.ID = PULLING_BACK; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup

    infMsg.InfEventType = INF_REQ_MOTOR_START;

    err = INF_SendMessage( &infMsg );
    UIM_Trace( err, __LINE__ );

    UIM_ChangeScreen( &nextScreen , INFUSION_TYPE );
    UIM_Trace( err, __LINE__ );

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_PromptTimeComplete( SCREEN_ID_T screenNo_ )
{
    SCREEN_FUNC_T nextScreen;    
    ERROR_T err;
        
    // Delete Prompt Timer
    if( s_PromptTimer != NULL )
    {
        err = TF_DeleteTimer( &s_PromptTimer );
        UIM_Trace(err,__LINE__);
    }

    nextScreen.Type = ID;
    if ( screenNo_ == SCREEN_29_3 )         //Infusion PRESS_PAUSE_PROMPT screen 
    {
        s_CurrTablePtr = INFUSION_DELIVERY_SCREEN_STATE_TABLE;
        s_CurrDrawArrPtr = INFUSION_ATTRIBUTE_ARR;
        if( s_CurrStep == HY_STEP_INDEX )
        {
            nextScreen.ID = INF_PROG;
        }
        else
        {
            nextScreen.ID = IG_INF_PROG;
        }
    }
    else
    {
        nextScreen.ID = AUTHORIZATION;//lint !e641 Ignoring conversion warning due to the way the multi state table is setup
        s_ListBoxIndex = EDIT_PROGRAM_LISTBOX;
    }
    UIM_ChangeScreen( &nextScreen , INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ReqFlush( void )
{
    ERROR_T err = ERR_NONE;
    INF_MESSAGE_T infMsg;
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;
    nextScreen.ID = FLUSHING; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup

    infMsg.InfEventType = INF_REQ_MOTOR_START;

    err = INF_SendMessage( &infMsg );
    UIM_Trace( err, __LINE__ );

    UIM_ChangeScreen( &nextScreen , INFUSION_TYPE );
    UIM_Trace( err, __LINE__ );

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_PopupPause( void )
{
    ERROR_T err = ERR_NONE;

    // Pressing pause should pause infusion and return to infusion screen according to
    // the story board.
    err = AAM_Clear(STEP_ENDING);
    UIM_Trace( err, __LINE__ );

    err = AAM_Clear(NEAR_END_INF);
    UIM_Trace( err, __LINE__ );

    UIM_ReqPause();

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ReqPause( void )
{
    ERROR_T err = ERR_NONE;
    INF_MESSAGE_T infMsg;
    SCREEN_FUNC_T nextScreen;
    SCREEN_INFO_T screenInfo;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    if( s_PromptTimer != NULL )
    {
        err = TF_StopAndDeleteTimer( &s_PromptTimer );
        UIM_Trace(err,__LINE__);
    }

    nextScreen.Type = ID;
    
    if( screenInfo.SubStateScreen.ID == FLUSHING )
    {
        s_CurrTablePtr = FLUSH_STATE_SCREEN_STATE_TABLE;
        s_CurrDrawArrPtr = FLUSH_ATTRIBUTE_ARR;
        
        nextScreen.ID = FLUSHING_PAUSE;
    }
    else
    {
        s_CurrTablePtr = INFUSION_DELIVERY_SCREEN_STATE_TABLE;
        s_CurrDrawArrPtr = INFUSION_ATTRIBUTE_ARR;

        nextScreen.ID = PROG_PAUSE; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
    }
    
    infMsg.InfEventType = INF_REQ_MOTOR_STOP;

    err = INF_SendMessage( &infMsg );
    UIM_Trace( err, __LINE__ );

    UIM_ChangeScreen( &nextScreen , INFUSION_TYPE );
    UIM_Trace( err, __LINE__ );

    s_IsPausePressed = true;
    s_PrevFlowRate = s_CurrProgram.ProgStepFlowRate[s_CurrStep];
}

////////////////////////////////////////////////////////////////////////////////
//This function is called when
//  1) Starting HY
//  2) Resuming infusion after AIL, OCC, and pause with or without rate change
static void UIM_ReqInfDelivery ( void )
{
    ERROR_T err = ERR_NONE;
    SCREEN_INFO_T screenInfo;
    SCREEN_FUNC_T nextScreen;
    INF_MESSAGE_T infMsg;
    AM_MESSAGE_T amMsg;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    if(( s_IsPausePressed ==  true ) && 
       ( s_CurrProgram.ProgStepFlowRate[s_CurrStep] != s_PrevFlowRate ))
    {
        if ( s_IsOffProgram )
        {
            for( int i=s_CurrStep+1; i<=s_CurrProgram.ProgIgSteps; i++ )
            {
                s_CurrProgram.ProgStepFlowRate[i] = s_CurrProgram.ProgStepFlowRate[s_CurrStep];
            }
        }
        infMsg.InfEventType = INF_MAN_RATE_ADJ;
        infMsg.ManRate = s_CurrProgram.ProgStepFlowRate[s_CurrStep];

        err = INF_SendMessage( &infMsg );
        UIM_Trace( err, __LINE__ );

    }

    s_IsPausePressed = false;

    if( screenInfo.ScreenType == INFUSION_TYPE )    //If the system is simply paused or beginning infusion
    {
        infMsg.InfEventType = INF_REQ_MOTOR_START;
        err = INF_SendMessage( &infMsg );
        UIM_Trace( err, __LINE__ );

        // Be sure the pause audio alert is OFF
        amMsg.EventType = ALERT;
        amMsg.Alert = AUDIO_NONE;
        err = AM_CommandNotify( &amMsg );
        UIM_Trace( err, __LINE__ );
    }
    else if( screenInfo.ScreenType == ALARM_TYPE ) //If the system is in occlusion or AIL state.
    {
        AAM_Clear( AIL ); //These alarms are mutually exclusive, therefore we can assume it is safe to clear both of them if one or 
        AAM_Clear( OCC ); //the other has happened.
    }
    else
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ ); //shouldnt reach this
    }

    nextScreen.Type = ID;
    
    if( s_CurrStep == HY_STEP_INDEX )
    {
        nextScreen.ID = INF_PROG; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
    }
    else
    {
        nextScreen.ID = IG_INF_PROG;
    }
    
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );

}

////////////////////////////////////////////////////////////////////////////////
//This function is called when the user indicates HY or IG is incomplete even
//though the pump believes programmed volume was infused. Infusion manager is
//notified through a unique INF message, indicating it should resume for a
//fixed time while preserving relevant data such as infused volume.
static void UIM_CompleteInfDelivery ( void )
{
    ERROR_T err = ERR_NONE;
    SCREEN_FUNC_T nextScreen;
    nextScreen.Type = ID;
    INF_MESSAGE_T infMsg;

    if( s_CurrStep == HY_STEP_INDEX  )
    {
        nextScreen.ID = INF_PROG;
    }
    else
    {
        nextScreen.ID = IG_INF_PROG;
    }

    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
    infMsg.InfEventType = INF_REQ_PENDING_INFUSION;
    err = INF_SendMessage( &infMsg );
    UIM_Trace( err, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
//This function and message is to let the infusion manager know that Hy is complete
//and that Infusion Manager should begin the process of switching to Ig.
//The process involves leaving the UI on the Hy Inf progress screen until the
//Infusion manager lets the UI know that Hy has completely left the tubing.
//At that point the UI should switch to the Ig Inf Progress screen.
static void UIM_ReqStartIG ( void )
{
    ERROR_T err = ERR_NONE;
    INF_MESSAGE_T infMsg;
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;    
    nextScreen.ID = INF_PROG; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
    
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );

    infMsg.InfEventType = INF_SWITCH_TO_IG;
    err = INF_SendMessage( &infMsg );
    UIM_Trace( err, __LINE__ );
    
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SettingsMenuSelection ( int selection_, uint16_t listSize_ )
{    
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;

    //Selection of Highlighted List Box Item from Settings menu list
    switch( selection_ )
    {
        case BRIGHTNESS_LISTBOX:
            nextScreen.ID = ADJUST_BRIGHTNESS;          //SCREEN_40
            break;
            
        case VOLUME_LISTBOX:
            nextScreen.ID = ADJUST_VOLUME;              //SCREEN_41
            break;

        case INDICATION_LISTBOX:
            s_ListBoxIndex = 0;                         // Set Listbox Index always to 0
            nextScreen.ID = INDICATION_MENU;            //SCREEN_39_5
            break;

        case LANGUAGE_LISTBOX:
            s_ListBoxIndex = 0;                         // Set Listbox Index always to 0
            // Language option is only available when more than one language
            // exists on the filesystem. Determine whether this selection
            // navigates to language menu or exit based on listSize_.
            if( listSize_ > EXIT_LISTBOX )
            {
                nextScreen.ID = LANGUAGE_MENU;          //SCREEN_39_8
            }
            else
            {
                nextScreen.ID = EXIT_AUTHORIZED_SETUP;  //SCREEN_43_1
            }
            break;
            
        case EXIT_LISTBOX:
            s_ListBoxIndex = PUMP_SETTINGS_LISTBOX;      // Set Listbox Index to PUMP SETTINGS LISTBOX Index
            nextScreen.ID = AUTHORIZATION;      		//SCREEN_57
            break;

        default:
            UIM_Trace( ERR_VERIFICATION, __LINE__ );
            break;
    }
    
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_DeviceModeSelection ( int selection_ )
{
    ERROR_T err;
    SSM_MESSAGE_T ssMsg;
    SCREEN_FUNC_T nextScreen;
    bool progAvailableF = false;

    //Selection of Highlighted List Box Item from setup menu list
    switch( selection_ )
    {
        case DEVICE_MODE_INFUSION:
            err = PM_GetProgramStatus ( &progAvailableF );
            UIM_Trace( err, __LINE__ );
            if ( progAvailableF == false )
            {
                // No Program Created
                s_ListBoxIndex = DEVICE_MODE_INFUSION;
                nextScreen.Type = ID;
                nextScreen.ID = NO_ACTIVE_PROGRAM;
                UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            }
            else
            {
                // Always star with first HY Step
                s_CurrStep = HY_STEP_INDEX;
                ssMsg.MsgEvent = SSM_EVT_STATE_REQ;
                ssMsg.State = SSM_INFUSION;
                err = SSM_CommandNotify( &ssMsg );
                UIM_Trace( err, __LINE__ );
            }
            break;

        case DEVICE_MODE_SETUP:
          UIM_ResetListBoxIndex();
          ssMsg.MsgEvent = SSM_EVT_STATE_REQ;
          ssMsg.State = SSM_SETTINGS;
          err = SSM_CommandNotify( &ssMsg );
          UIM_Trace( err, __LINE__ );
          break;

        default:
          break;
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_PasscodeProcess ( void )
{
    SCREEN_INFO_T screenInfo;
    SCREEN_FUNC_T nextScreen;
    ERROR_T status;
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    char textString[NUM_OF_PASSCOD_COL] = {0};
    char usbTextString[NUM_OF_PASSCOD_COL] = {0};
    GUI_HWIN currWindow = 0;
    UIM_GetWindow( &currWindow );

    s_Passcode.PassColumn[s_Passcode.PassCurrentCol] =  s_Passcode.PasscodeValue;

    //check user entered 4 digit password
    if( (s_Passcode.PassCurrentCol) >=  (PASSCODE_COL4) )
    {
      status = CFG_GetPassword( textString );
      UIM_Trace( status, __LINE__ );
      status = CFG_GetUsbPassword( usbTextString );
      UIM_Trace( status, __LINE__ );
      //check valid password
      if( (memcmp(s_Passcode.PassColumn,textString,sizeof(s_Passcode.PassColumn))) == 0 )
      {
        nextScreen.Type = ID;
        nextScreen.ID = AUTHORIZATION;
        s_ListBoxIndex = DEFAULT_VOL_BRIGHT_LVLS;
        s_Passcode.PassCurrentCol = 0;
        s_Passcode.PasscodeValue = 0;
      }
      else if( (memcmp(s_Passcode.PassColumn,usbTextString,sizeof(s_Passcode.PassColumn))) == 0 )
      {
        nextScreen.Type = ID;
        nextScreen.ID = USB_ENABLE;
        UIM_ResetListBoxIndex(); //Reset the listbox index since this function implies that we are moving forward in screenflow.
        s_Passcode.PassCurrentCol = 0;
        s_Passcode.PasscodeValue = 0;
      }
      else
      {
        nextScreen.Type = ID;
        nextScreen.ID = PASSCODE_INCORRECT;
        s_Passcode.PassCurrentCol = 0;
        s_Passcode.PasscodeValue = DEFAULT_PASSCODE_VALUE;
      }
    }
    else
    {
        nextScreen.Type = ID;
        nextScreen.ID = PASSCODE_SCREEN;
        s_Passcode.PasscodeValue = 0;

        UIM_PasscodeAction( currWindow, false, false );       //Display default 1 value as per UI SRS

        //Reset Passcode value
        if( ( screenInfo.ScreenNumber == SCREEN_44_1) || //Incorrect  password
          ( screenInfo.ScreenNumber == SCREEN_9 ))     //Main Menu
        {
            s_Passcode.PassCurrentCol = 0;
        }
        else
        {
            s_Passcode.PassCurrentCol++;
            UIM_PasscodeAction( currWindow, true, false );       //Display default 1 value as per UI SRS
        }
    } 
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_PasscodeBackProcess( void )
{
    GUI_HWIN currWindow = 0;
    SCREEN_FUNC_T nextScreen;
    UIM_GetWindow( &currWindow );

    //check for last digit of password
    if( s_Passcode.PassCurrentCol ==  PASSCODE_COL1 )
    {
        UIM_PasscodeExit();
    }
    else
    {
        //display previous Column with underline on back button
        s_Passcode.PassCurrentCol--;
        s_Passcode.PasscodeValue =  s_Passcode.PassColumn[s_Passcode.PassCurrentCol];
        UIM_PasscodeAction( currWindow, false, false );
        nextScreen.Type = ID;
        nextScreen.ID = PASSCODE_SCREEN;
        UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_PasscodeExit( void )
{
    UIM_SetBackButtonListboxIndex();
    UIM_ReqAbortSysState();
}                

////////////////////////////////////////////////////////////////////////////////
static void UIM_LockRatesBack( void )
{
	SCREEN_FUNC_T nextScreen;
    LINESET_INFO_T needle;
    ERROR_T reportedError;
  
    reportedError = INF_GetLinesetInfo( &needle );
    UIM_Trace( reportedError, __LINE__ );
    
	nextScreen.Type = ID;
    
	if( needle == BIFURCATED_NEEDLE_LINESET )
	{
		nextScreen.ID = BIFURCATED_NOTE_IG;         //SCREEN_71
	}
	else
	{
		if( s_CurrProgram.ProgType  ==  CUSTOM )
		{
			nextScreen.ID  = EDIT_IG_STEPS_FLOW_RATE;        //SCREEN_50 
		}
		else
		{
			nextScreen.ID = HYQVIA_IG_TIME;          //SCREEN_69   
		}
	}
	UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_DosageProgramSelection ( void )
{
    SCREEN_FUNC_T nextScreen;
    
    float currProgDosage;                                   // Configured Dosage(g)
    float currProgHyDosage;                                 // Calculated HY Dosage(ml)
    float currProgIgDosage;                                 // Calculated IG Dosage(ml)
    ERROR_T reportedError;
    reportedError =  UIM_GetDosageValues( &currProgDosage, &currProgHyDosage, &currProgIgDosage );
    if ( reportedError != ERR_NONE )
    {
        UIM_Trace( reportedError, __LINE__ );     ///there is issue with UIM
        return;
    }
    s_CurrProgram.ProgDosage = currProgDosage;
    s_CurrProgram.ProgHyDosage = currProgHyDosage;
    s_CurrProgram.ProgIgDosage = currProgIgDosage;
    nextScreen.Type = ID;

    nextScreen.ID = EDIT_HY_FLOW_RATE;


    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_HyFlowRateSelection ( void )
{
    RATE_VALUE_T hyFlowRate;
    SCREEN_FUNC_T nextScreen;
    LINESET_INFO_T needle = NO_LINESET;
    ERROR_T err;
    
    err =  UIM_GetHyFlowRateValue( &hyFlowRate );
    if ( err != ERR_NONE )
    {
        UIM_Trace( err, __LINE__ );     ///there is issue with UIM
        return;
    }
    
    s_CurrProgram.ProgStepFlowRate[HY_STEP_INDEX] = hyFlowRate;
    nextScreen.Type = ID;
    
    err = INF_GetLinesetInfo( &needle );
    UIM_Trace( err, __LINE__ );

    if( needle == BIFURCATED_NEEDLE_LINESET )
    {
        if( s_CurrProgram.ProgType  == CUSTOM )
        {
            nextScreen.Type = ID;
            nextScreen.ID = BIFURCATED_NOTE_HY_CUSTOM;
        }
        else if( s_CurrProgram.ProgType  == HYQVIA_TEMPLATE )
        {
            nextScreen.Type = ID;
            nextScreen.ID = BIFURCATED_NOTE_HY;
        }
    }
    else
    {
        if( s_CurrProgram.ProgType  == CUSTOM )
        {
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_IG_STEPS;
        }
        else if( s_CurrProgram.ProgType  == HYQVIA_TEMPLATE )
        {
            nextScreen.Type = ID;
            nextScreen.ID = HYQVIA_IG_TIME;  //SCREEN_69
        }
    }

    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_IgSelectionProcess ( void )
{
    SCREEN_INFO_T screenInfo;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    U8 currIgStepScreenNoIndex;
    U8 currIgDurationValue;
    RATE_VALUE_T currIgFlowValue;
    ERROR_T reportedError;
    reportedError =  UIM_GetCurrentIgValues( &currIgStepScreenNoIndex, &currIgFlowValue, &currIgDurationValue );
    if ( reportedError != ERR_NONE )
    {
        UIM_Trace( reportedError, __LINE__ );     ///there is issue with UIM
        return;
    }
    s_CurrProgram.ProgStepFlowRate[currIgStepScreenNoIndex+IG_STEP_START_INDEX] = currIgFlowValue;          // Store value on selction of OK Button of ig step
    s_CurrProgram.ProgStepTime[currIgStepScreenNoIndex+IG_STEP_START_INDEX].Min = currIgDurationValue;      // Store value on selction of OK Button of ig step
    //Fill value upto # of Ig steps  
    if( currIgStepScreenNoIndex >= ( s_CurrProgram.ProgIgSteps-1 ) )
    {
        UIM_ProgramInfoScreen();                                // Display Custom program review screen
    }
    else
    {
        if( screenInfo.SubStateScreen.ID == EDIT_IG_STEPS_FLOW_RATE )  // Do flip(alternate) ig Step Duration Screen and Ig Step Flow Rate Screen until it will reach to ig step index
        {
            UIM_IgStepDurationScreen();
        }
        else
        {   
            // For CUSTOM type program, if calculated IG dosage for entered IG steps is greater than the progIgDosage,
            // system should terminate further execution and show PROGRAM ERROR Screen (Screen 73)
            if ( ( s_ProgramType == CUSTOM ) && ( UIM_CheckForProgramError( currIgStepScreenNoIndex ) ) )
            {
                return;
            }
          
            currIgStepScreenNoIndex++;      //Current Ig step index to store IG duration and IG flowrate 

            // If get 0 value from saved program or wrong data
            // Set Default value if somewhere going wrong to read previusly sotred data
            // This is just for the safety purpose
            if( ((s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX+currIgStepScreenNoIndex].Min % PM_IG_TIME_DEFAULT_VALUE) != 0) ||
                 (s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX+currIgStepScreenNoIndex].Min == 0) )
            {
                 s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX+currIgStepScreenNoIndex].Min = PM_IG_TIME_DEFAULT_VALUE;
                 s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX+currIgStepScreenNoIndex].Hour = 0;
            }
            
            if( s_CurrProgram.ProgStepFlowRate[IG_STEP_START_INDEX+currIgStepScreenNoIndex] == 0 )
            {
                s_CurrProgram.ProgStepFlowRate[IG_STEP_START_INDEX+currIgStepScreenNoIndex] = PM_IG_FlOW_RATE_DEFAULT_VALUE;
            }
            UIM_SetCurrentIgValues( s_CurrProgram.ProgStepFlowRate[IG_STEP_START_INDEX+currIgStepScreenNoIndex], s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX+currIgStepScreenNoIndex].Min );

            UIM_SetCurrentIgStepNo( currIgStepScreenNoIndex );     //Update IG Screen Index
            UIM_IgStepFlowRateScreen(); 
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
static bool UIM_CheckForProgramError( uint8_t currIgStepScreenIndex_ )
{
    RATE_VALUE_T flowRate = ML_0;
    SCREEN_FUNC_T nextScreen;
    
    //Get flowrate based on needle
    flowRate = PM_GetFlowrateBasedOnNeedle( s_CurrProgram.ProgNeedleSet, s_CurrProgram.ProgStepFlowRate[IG_STEP_START_INDEX + currIgStepScreenIndex_] );

    //Calculate IG dosage for each step and add it into s_TotalStepDosage to make a total of IG dosages for all steps
    s_TotalStepDosage = s_TotalStepDosage + ( ( s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX + currIgStepScreenIndex_].Min * (float)RATE_VALUE_ARR[flowRate] ) / MIN_PER_HOUR );
      
    //If total IG dosage till the current step is greater than the programmed IG dosage, 
    //system should show PROGRAM ERROR screen (Screen 73)
    if ( s_TotalStepDosage > s_CurrProgram.ProgIgDosage )
    {
        //Reset total IG dosage though this is being reset in UIM_IgStepSelection
        s_TotalStepDosage = 0;
        
        //Reset IG Step Number
        UIM_SetCurrentIgStepNo( 0 );

        //Go to the next SCREEN_73
        nextScreen.Type = ID;
        nextScreen.ID = PROGRAM_ERROR;
        
        UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
        
        return true;
    }
    
    return false;
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_IgBackButtonProcess( void )
{
    
    SCREEN_INFO_T screenInfo;
    SCREEN_FUNC_T nextScreen;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    uint8_t currIgStepScreenNoIndex = 0;
    uint8_t currIgDurationValue = 0;
    RATE_VALUE_T currIgFlowValue;
    ERROR_T reportedError;
    reportedError =  UIM_GetCurrentIgValues( &currIgStepScreenNoIndex, &currIgFlowValue, &currIgDurationValue );
    UIM_Trace( reportedError, __LINE__ );     ///there is issue with UIM

    switch( screenInfo.SubStateScreen.ID )
    {
      
        case EDIT_IG_STEPS_FLOW_RATE:

            if( currIgStepScreenNoIndex )
            {
                currIgFlowValue =  s_CurrProgram.ProgStepFlowRate[currIgStepScreenNoIndex+IG_STEP_START_INDEX] ;          // Store value on selction of OK Button of ig step
                currIgDurationValue = s_CurrProgram.ProgStepTime[currIgStepScreenNoIndex].Min;      // Store value on selction of OK Button of ig step
                //If get 0 value from saved program or wrong data
                if( ((currIgDurationValue % PM_IG_TIME_DEFAULT_VALUE) != 0) || (currIgDurationValue == 0) ) // Set Default value if somewhere going wrong to read previusly sotred data  
                {
                    currIgDurationValue = PM_IG_TIME_DEFAULT_VALUE; 
                }
                UIM_SetCurrentIgValues( currIgFlowValue, currIgDurationValue );
                currIgStepScreenNoIndex--;
                UIM_SetCurrentIgStepNo( currIgStepScreenNoIndex );     //Update IG Screen Index 
                nextScreen.Type = ID;
                nextScreen.ID = EDIT_IG_STEPS_DURATION;          
                UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            }
            else
            {
                currIgFlowValue = PM_IG_FlOW_RATE_DEFAULT_VALUE;          // Store value on selction of OK Button of ig step
                currIgDurationValue = PM_IG_TIME_DEFAULT_VALUE;          // Store value on selction of OK Button of ig step
                UIM_SetCurrentIgValues( currIgFlowValue, currIgDurationValue );
                nextScreen.Type = ID;
                nextScreen.ID = EDIT_IG_STEPS;          
                UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            }

            break;

        case EDIT_IG_STEPS_DURATION:
        case REVIEW_EDIT_PROGRAM_INFO1:

            currIgFlowValue =  s_CurrProgram.ProgStepFlowRate[currIgStepScreenNoIndex+IG_STEP_START_INDEX] ;          // Store previous value on selction of back Button of ig step
            currIgDurationValue = s_CurrProgram.ProgStepTime[currIgStepScreenNoIndex].Min;                            // Store value previous value on selction of back Button  of ig step
            //If get 0 value from saved program or wrong data
            if( ((currIgDurationValue % PM_IG_TIME_DEFAULT_VALUE) != 0) || (currIgDurationValue == 0) ) // Set Default value if somewhere going wrong to read previusly sotred data  
            {
                currIgDurationValue = PM_IG_TIME_DEFAULT_VALUE; 
            }
            UIM_SetCurrentIgValues( currIgFlowValue, currIgDurationValue );
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_IG_STEPS_FLOW_RATE;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );

            break;
                
        default:

            break;
    }
}


////////////////////////////////////////////////////////////////////////////////
static void UIM_IgStepSelection ( void )
{
    SCREEN_FUNC_T nextScreen;
    U8 currIgStepValue;
    uint8_t structLen;
    ERROR_T reportedError;
    
    s_TotalStepDosage = 0;  //Reset stored IG dosage value
    
    reportedError = UIM_GetIgStepValue( &currIgStepValue );
    if ( reportedError != ERR_NONE )
    {
        UIM_Trace( reportedError, __LINE__ );     ///there is issue with UIM
        return;
    }
    
    nextScreen.ID = EDIT_IG_STEPS_FLOW_RATE;
    //If get 0 value from saved program or wrong data
    if( ((s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX].Min % PM_IG_TIME_DEFAULT_VALUE) != 0) ||
         (s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX].Min == 0) )  
    {
        s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX].Min = PM_IG_TIME_DEFAULT_VALUE; 
        s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX].Hour = 0; 
    }// Set Default value if somewhere going wrong to read previusly sotred data 
    if( s_CurrProgram.ProgStepFlowRate[IG_STEP_START_INDEX] == 0 )
    {
        s_CurrProgram.ProgStepFlowRate[IG_STEP_START_INDEX] = PM_IG_FlOW_RATE_DEFAULT_VALUE;
    }
    UIM_SetCurrentIgValues( s_CurrProgram.ProgStepFlowRate[IG_STEP_START_INDEX], s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX].Min );
    if( s_CurrProgram.ProgIgSteps < currIgStepValue )
    {
        //Reset buffer to fill new value accordingly New IG step Selection
        structLen = ( (sizeof( s_CurrProgram.ProgStepFlowRate) ) - ( sizeof(s_CurrProgram.ProgStepFlowRate[IG_STEP_START_INDEX]) ) );
        memset( &s_CurrProgram.ProgStepFlowRate[IG_STEP_START_INDEX], NULL, structLen );
        structLen = ( (sizeof( s_CurrProgram.ProgStepTime)) - (sizeof(s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX])) );
        memset( &s_CurrProgram.ProgStepTime[IG_STEP_START_INDEX], NULL, structLen );
    }
    s_CurrProgram.ProgIgSteps = currIgStepValue;
    nextScreen.Type = ID;
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static ERROR_T UIM_CreatePromptTimer ( uint16_t timer_ )
{
    ERROR_T err;
    TF_TIMER_DESC_T promptTimer;
    static const char name[] = "UIM Prompt Timer";
    WM_HWIN *win;
    static SCREEN_INFO_T screenInfo;

    UIM_GetActiveWinAndScreenInfo( &screenInfo, &win );
    if( s_PromptTimer != NULL )
    {
        err = TF_DeleteTimer( &s_PromptTimer );
        UIM_Trace(err,__LINE__);
    }
    promptTimer.ptmr = &s_PromptTimer;
    promptTimer.dly = ( INT32U ) timer_;
    promptTimer.period = ( INT32U ) 0;
    promptTimer.opt = ( INT8U ) OS_TMR_OPT_ONE_SHOT;
    promptTimer.callback = ( OS_TMR_CALLBACK ) UIM_PromptTimerCB;
    promptTimer.callback_arg = ( void * ) &screenInfo.ScreenNumber;
    promptTimer.NAME = name;

    err = TF_CreateAndStartTimer( &promptTimer );
    UIM_Trace(err,__LINE__);
    
    return err;
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ProgramStepSelectionProcess ( void )
{    
    SCREEN_FUNC_T nextScreen;
        
    UIM_ResetListBoxIndex();
    if( s_CurrProgram.ProgIgSteps <=  MAX_LISTVIEW_ROWS )
    {
        nextScreen.Type = ID;
        nextScreen.ID = REVIEW_EDIT_PROGRAM_INFO3;
        UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
    }
    else
    {
        nextScreen.Type = ID;
        //Next screen as per UI SRS flow as it is 
        nextScreen.ID = REVIEW_EDIT_STEP_INFO2;
        UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_BackButtonStepProcess ( void )
{    
    SCREEN_FUNC_T nextScreen;
        
    if( s_CurrProgram.ProgIgSteps <=  MAX_LISTVIEW_ROWS )
    {
        nextScreen.Type = ID;
        //Next screen as per UI SRS flow as it is 
        nextScreen.ID = REVIEW_EDIT_STEP_INFO1;
        UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
    }
    else
    {
        nextScreen.Type = ID;
        //Previous screen as per UI SRS flow 
        nextScreen.ID = REVIEW_EDIT_STEP_INFO2;
        UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SaveProgramInfo ( void )
{
    ERROR_T reportedError;
    SCREEN_FUNC_T nextScreen;

    // Save  program values into program manager
    reportedError = PM_SetProgramInfo( &s_CurrProgram, s_ProgramType );
    UIM_Trace( reportedError, __LINE__ );     ///there is issue with UIM
    
    UIM_ResetListBoxIndex();

    nextScreen.Type = ID;
    nextScreen.ID = PROGRAM_IS_READY_FOR_USE;   //SCREEN_59_7
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
    
    //Create timer of 5 seconds
    reportedError = UIM_CreatePromptTimer( PROMPT_SCREEN_DURATION );
    if ( reportedError != ERR_NONE )
    {
        UIM_Trace( reportedError, __LINE__ );     ///there is issue with UIM
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SaveHyqviaProgramInfo ( void )
{
    SCREEN_FUNC_T nextScreen;
    ERROR_T reportedError;
    // Save  program values into program manager
    reportedError = PM_SetProgramInfo( &s_CurrProgram, s_ProgramType );
    if ( reportedError != ERR_NONE )
    {
        UIM_Trace( reportedError, __LINE__ );     ///there is issue with UIM
        return;
    }
    UIM_ResetListBoxIndex();
    nextScreen.Type = ID;
    nextScreen.ID = REVIEW_EDIT_PROGRAM_INFO3;          
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_IgStepFlowRateScreen ( void )
{
    SCREEN_FUNC_T nextScreen;
    nextScreen.Type = ID;

    nextScreen.ID = EDIT_IG_STEPS_FLOW_RATE;

    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_IgStepDurationScreen ( void )
{
    SCREEN_FUNC_T nextScreen;
    nextScreen.Type = ID;

    nextScreen.ID = EDIT_IG_STEPS_DURATION;
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ProgramInfoScreen ( void )
{
    SCREEN_FUNC_T nextScreen;
    U8 programId[MAX_NUM_PROGRAMS] = {0};                // Get the program value # from program manager
    ERROR_T reportedError = ERR_NONE;
    LINESET_INFO_T needle;
    reportedError = PM_PopulateAndCalculateCustomProgInfo( &s_CurrProgram );
    if ( reportedError != ERR_NONE )
    {
        memset(programId,0,sizeof(programId));
        UIM_Trace( reportedError, __LINE__ );     ///there is issue with UIM
    }
    
    reportedError = INF_GetLinesetInfo( &needle );
    UIM_Trace( reportedError, __LINE__ );
    
    nextScreen.Type = ID;

    if( needle == BIFURCATED_NEEDLE_LINESET )
    {
        nextScreen.ID = BIFURCATED_NOTE_IG_CUSTOM; //SCREEN_71
    }
    else
    {
        nextScreen.ID = ALLOW_RATE_CHANGES;    // SCREEN_60_1
    }
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_HyqviaReviewInfoScreen ( void )
{
    U8 programId[MAX_NUM_PROGRAMS] = {0};                // Get the program value # from program manager
    ERROR_T reportedError = ERR_NONE;
    uint8_t igTime;
    SCREEN_FUNC_T nextScreen;

    reportedError =  UIM_GetIgTimeValues( &igTime );
    if ( reportedError != ERR_NONE )
    {
        memset(programId,0,sizeof(programId));
        UIM_Trace( reportedError, __LINE__ );     ///there is issue with UIM
    }
    s_CurrProgram.ProgIgTimeInterval = igTime;
    reportedError = PM_PopulateAndCalculateHyQviaProgInfo( s_CurrProgram.PatientWeight, s_CurrProgram.InfusionRampUp, &s_CurrProgram );
    if ( reportedError != ERR_NONE )
    {
        memset(programId,0,sizeof(programId));
        UIM_Trace( reportedError, __LINE__ );     ///there is issue with UIM
    }
    if( s_CurrProgram.ProgNeedleSet == SINGLE_NEEDLE_LINESET )
    {
        nextScreen.Type = ID;
        nextScreen.ID = ALLOW_RATE_CHANGES;    //SCREEN_60_1
    }
    else
    {
       nextScreen.Type = ID;
       nextScreen.ID = BIFURCATED_NOTE_IG;     //SCREEN_71
    }
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_HyqviaIgStepTimeBack( void )
{
   SCREEN_FUNC_T nextScreen;
   
   if( s_CurrProgram.ProgNeedleSet == BIFURCATED_NEEDLE_LINESET )
    {
        nextScreen.Type = ID;
        nextScreen.ID = BIFURCATED_NOTE_HY;         //SCREEN_71
    }
    else
    {
        nextScreen.Type = ID;
        nextScreen.ID = EDIT_HY_FLOW_RATE;        //SCREEN_48  
    }
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
  
}

////////////////////////////////////////////////////////////////////////////////
void UIM_PasscodeAction ( GUI_HWIN currWindow_ , bool incrementFlag_, bool decrementFlag_ )
{
    WM_HWIN hItem;
    char textString[MAX_NUM_LINE_CHARS] = {0};
    GUI_RECT rect;
    //Increment Passcode Value
    if( incrementFlag_ == true )
    {
        if( s_Passcode.PasscodeValue >= 9 )
        {
            s_Passcode.PasscodeValue = 0;
        }
        else
        {
            s_Passcode.PasscodeValue++;
        }
    }
    else if( decrementFlag_ == true )     //decrement Passcode Value
    {
        if( s_Passcode.PasscodeValue > 0 )
        {
            s_Passcode.PasscodeValue--;
        }
        else
        {
            s_Passcode.PasscodeValue = 9;
        }
    }
    hItem = WM_GetDialogItem( currWindow_, ( TEXTBOX_ID + s_Passcode.PassCurrentCol ));
    textString[0] = s_Passcode.PasscodeValue +'0';
    TEXT_SetText( hItem, textString );
    UIM_SetUnderLineNum( hItem, textString, &rect );
    UIM_SetRectValue( &rect );
    GUI_Delay( GUI_DELAY_AMOUNT );
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetRectValue( const GUI_RECT *rect_ )
{
    if( rect_ == NULL )
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return;
    }

    s_Rect = *rect_;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetRectValue( GUI_RECT *rect_ )
{
    if( rect_ == NULL)
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return ERR_PARAMETER;
    }
    else
    {
        *rect_= s_Rect ;
        return ERR_NONE;
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_AuthorizedMenuSelection ( int selection_ )
{
    ERROR_T err = ERR_NONE;
    
    SCREEN_FUNC_T nextScreen;

    UIM_SetBackButtonListboxIndex();
    UIM_ResetListBoxIndex();
    //Selection of Highlighted List Box Item from setup menu list
    switch( selection_ )
    {
        case EDIT_PROGRAM_LISTBOX:
            UIM_SetDefaultProgramValue();
            //Program screen will call from many places so make default all parameters before create new program
            err = PM_GetProgramInfo ( &s_CurrProgram );
            UIM_Trace( err, __LINE__ );
            
            if ( s_CurrProgram.ProgAvailableF )
            {
                UIM_SetDefaultProgramValue();
            }
            nextScreen.Type = ID;
            nextScreen.ID = PROGRAM_TYPE;
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            break;

        case PUMP_SETTINGS_LISTBOX:
            nextScreen.Type = ID;
            nextScreen.ID = SETTINGS_MENU;      //SCREEN_39
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            break;

        case EXIT_AUTH_SETUP_LISTBOX:
            nextScreen.Type = ID;
            nextScreen.ID = EXIT_AUTHORIZED_SETUP;
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );

            break;
            
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SaveAllowRateChange ( int selection_ )
{
    SCREEN_FUNC_T nextScreen;
    
    //Selection of Highlighted List Box Item from  Allow Rate Change list
    switch( selection_ )
    {
        case YES_LISTBOX:       //  Allow Rate Change ON and Save Allow Rate Change value
            
            s_CurrProgram.RateLocked = true;
                        
            break;

        case NO_LISTBOX:     // Allow Rate Change OFF and Save Allow Rate Change value
                      
            s_CurrProgram.RateLocked = false;
                        
            break;

        default:
          
            break;
    }
    
    s_ListBoxIndex = 0; //Reset the listbox index since this function implies that we are moving forward in screenflow.
                 
    nextScreen.Type = ID;
    nextScreen.ID = NEEDLE_CHECK;          
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_IndicationSelection ( int selection_ )
{
	ERROR_T status = ERR_NONE;
	SCREEN_FUNC_T nextScreen;

	switch(selection_)
	{
		case PI_LISTBOX:
			status = CFG_SetCIDP( FALSE );
			UIM_Trace( status, __LINE__ );
			break;

		case CIDP_LISTBOX:
			status = CFG_SetCIDP( TRUE );
			UIM_Trace( status, __LINE__ );
			break;

		default:
			UIM_Trace( ERR_PARAMETER, __LINE__ );
			break;
	}

	s_ListBoxIndex = INDICATION_LISTBOX;                      	   // Set Listbox Index with INDICATION LISTBOX index
	nextScreen.Type = ID;
	nextScreen.ID = SETTINGS_MENU;
	UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_LanguageSelection ( int selection_ )
{
    ERROR_T status = ERR_NONE;
    SCREEN_FUNC_T nextScreen;
    uint8_t langCount = 0;
    s_ListBoxIndex = LANGUAGE_LISTBOX;                         // Set Listbox Index with LANGUAGE LISTBOX index
    
    char tempString[MAX_FILE_NAME_CHAR_SIZE]= {0};
    status = LM_GetLanguageCount( &langCount );
    UIM_Trace( status, __LINE__ );

    if( selection_ >  langCount)
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
    }
    else
    {
        nextScreen.Type = ID;
        nextScreen.ID =  LOADING_LANGAUGE; //SCREEN_103
        UIM_ChangeScreen( &nextScreen , ALARM_TYPE );

        // Disable UIM task watchdog to prevent timeout while loading language.
        status = WD_DisableTaskWdog( WD_UIM );
        UIM_Trace( status, __LINE__ );

        status = LM_GetLanguageName( tempString, (uint8_t)selection_, 0 );
        UIM_Trace( status, __LINE__ );
    
        status = CFG_SetLanguage( tempString );
        UIM_Trace( status, __LINE__ );
        
        //set the selected language in the system
        status = LM_SetLanguageFile( true );
        UIM_Trace( status, __LINE__ );

        // Re-enable UIM task watchdog now that language file is loaded.
        status = WD_DisableTaskWdog( WD_UIM );
        UIM_Trace( status, __LINE__ );

        if( status == ERR_NONE )
        {
            // Request reboot
            NVIC_SystemReset();
        }
        else
        {
            // Something failed return to previous screen
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ExitAuthorizedSetupSelection ( int selection_ )
{
    SCREEN_FUNC_T nextScreen;
    
    //Selection of Highlighted List Box Item from  Exit Authorized Setup list Box
    switch( selection_ )
    {
        case YES_LISTBOX:       //Allow Exit from Exit Authorized Setup
            UIM_ReqAbortSysState();
            s_ListBoxIndex = DEVICE_MODE_SETUP;
            break;

        case NO_LISTBOX:         //Back to  Authorized Setup
            UIM_SetBackButtonListboxIndex();
            nextScreen.Type = ID;
            nextScreen.ID = AUTHORIZATION;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            break;     
            
        default:

            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_FlushCompleteSelection ( const int selection_ )
{
    SCREEN_FUNC_T nextScreen;
    
    AAM_Clear( END_OF_FLUSH );
    
    //Selection of Highlighted List Box Item from Flush Complete list Box
    switch( selection_ )
    {
        case YES_LISTBOX:       //If Press Yes than Jump to REMOVE_NEEDLE_FLUSH Screen

            nextScreen.Type = ID;
            nextScreen.ID = REMOVE_NEEDLE_FLUSH;  

            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );

            break;

        case NO_LISTBOX:         //Back to Flushing
        
            UIM_ReqFlush();
            
            break;     
            
        default:

            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SaveNeedleCheckStatus ( int selection_ )
{
    SCREEN_FUNC_T nextScreen;
    //Selection of Highlighted List Box Item from  NEEDLE CHECK list
    switch( selection_ )
    {
        case YES_LISTBOX:  //Enable needle Placement Check
            
            s_CurrProgram.NeedlePlacementCheckEnabled = true;
            
            break;

        case NO_LISTBOX:   //Disable needle Placement Check 
          
            s_CurrProgram.NeedlePlacementCheckEnabled = false;
            
            break;

        default:

            break;
    }
    
    UIM_ResetListBoxIndex(); //Reset the listbox index since this function implies that we are moving forward in screenflow.
    
    //Jump To next screen 
    nextScreen.Type = ID;
    nextScreen.ID = FLUSH;          
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ResumeInfusionSelect( const int selection_ )
{
    SCREEN_FUNC_T nextScreen;
    ERROR_T err = ERR_NONE;
    SSM_MESSAGE_T ssMsg;
    INF_MESSAGE_T infMsg;
    INF_PROGRESS_SAVE_T progress;
    (void)memset( (void*)&ssMsg, 0, sizeof(SSM_MESSAGE_T) );

    UIM_ResetListBoxIndex(); //Reset the listbox index since this function implies that we are moving forward in screenflow.

    //Selection of Highlighted List Box Item from  FLUSH list
    switch( selection_ )
    {
        case YES_LISTBOX:  //Resume infusion
            // Get default information.
            err = PM_GetProgramInfo ( &s_CurrProgram );
            UIM_Trace( err, __LINE__ );

            // Update variables that may have been changed off program
            INF_GetProgress( &progress );
            s_CurrStep = progress.Resume.InfStep;
            s_CurrProgram.ProgIgSteps = progress.Resume.InfProgSteps;

            if( (s_CurrProgram.ProgStepFlowRate[s_CurrStep] != progress.Resume.ProgStepFlowRate[s_CurrStep]) &&
                    ( s_CurrStep != HY_STEP_INDEX ) )
            {
                s_IsOffProgram = TRUE;
            }

            memcpy( s_CurrProgram.ProgStepFlowRate, progress.Resume.ProgStepFlowRate, sizeof( progress.Resume.ProgStepFlowRate ) );
            memcpy( s_CurrProgram.ProgStepTime, progress.Resume.ProgStepTime, sizeof( progress.Resume.ProgStepTime ) );

            // Inform IM of restart
            infMsg.InfEventType = INF_RESTART;
            err = INF_SendMessage( &infMsg );
            UIM_Trace( err, __LINE__ );

            //Jump To next screen
            if( ( progress.Resume.InfState == INF_STATE_INFUSION_DELIVERY ) ||
                ( progress.Resume.InfState == INF_STATE_HY_TO_IG_TRANSITION ) )
            {
                // Change to resume HY-IG screen
                s_CurrTablePtr = INFUSION_DELIVERY_SCREEN_STATE_TABLE;
                s_CurrDrawArrPtr = INFUSION_ATTRIBUTE_ARR;
                nextScreen.Type = ID;
                nextScreen.ID = RESUME_INF_HY;
                UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            }
            else if ( progress.Resume.InfState == INF_STATE_INFUSION_PENDING )
            {
                // Change to START_HY
                s_CurrTablePtr = INFUSION_DELIVERY_SCREEN_STATE_TABLE;
                s_CurrDrawArrPtr = INFUSION_ATTRIBUTE_ARR;
                nextScreen.Type = ID;
                nextScreen.ID = START_HY;
                UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            }
            else
            {
                // Change to manual prime screen with 'OK' option available
                s_HasPrimedFlag = true;
                s_CurrTablePtr = PRIMING2_SCREEN_STATE_TABLE;
                s_CurrDrawArrPtr = PRIMING_MAN_ATTRIBUTE_ARR;
                nextScreen.Type = ID;
                nextScreen.ID = PRIMING_STEP2;
                UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            }
            break;

        case NO_LISTBOX:
            ssMsg.MsgEvent = SSM_EVT_STATE_ABORT;
            err = SSM_CommandNotify( &ssMsg );
            UIM_Trace( err, __LINE__ );
            // No screen change needed. Incoming message will tell UIM what to do.
            break;

        default:
            UIM_Trace( ERR_PARAMETER, __LINE__ );
            break;
    }

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SaveFlushStatus ( const int selection_ )
{
    
    SCREEN_FUNC_T nextScreen;
    //Selection of Highlighted List Box Item from  FLUSH list
    switch( selection_ )
    {
        case YES_LISTBOX:  //Enable FLUSH
            s_CurrProgram.FlushEnabled = true;
            break;

        case NO_LISTBOX:   //Disable FLUSH
            s_CurrProgram.FlushEnabled = false;
            break;

        default:
            UIM_Trace( ERR_PARAMETER, __LINE__ );
            break;
    }

    UIM_ResetListBoxIndex(); //Reset the listbox index since this function implies that we are moving forward in screenflow.
    
    //Jump To next screen 
    nextScreen.Type = ID;
    if ( s_CurrProgram.ProgType == CUSTOM )
    {
        nextScreen.ID = REVIEW_EDIT_PROGRAM_INFO1;  //SCREEN_12
    }
    else
    {
        nextScreen.ID = HYQVIA_PROGRAM_INFO1;  //SCREEN_12
    }
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_UsbEnableStatus ( const int selection_ )
{
    
    SCREEN_FUNC_T nextScreen;
    ERROR_T reportedError = ERR_NONE;
    bool usbEnabled = FALSE;
      
    //Selection of Highlighted List Box Item from  USB list
    switch( selection_ )
    {
        case YES_LISTBOX:  //Enable USB
          
            reportedError = FS_AcquireAccess( FS_DEFAULT_TIMEOUT );
            UIM_Trace( reportedError, __LINE__ );

            if( reportedError == ERR_NONE )
            {
                reportedError = USB_InterfaceEnable();
                UIM_Trace( reportedError, __LINE__ );
            }
            break;

        case NO_LISTBOX:   //Disable USB
          
            reportedError = USB_State( &usbEnabled );
            UIM_Trace( reportedError, __LINE__ );

            if( ( reportedError == ERR_NONE ) && usbEnabled )
            {
                reportedError = FS_ReleaseAccess();
                UIM_Trace( reportedError, __LINE__ );

                if( reportedError == ERR_NONE )
                {
                    reportedError = USB_InterfaceDisable();
                    UIM_Trace( reportedError, __LINE__ );
                }
            }
            break;

        default:
            UIM_Trace( ERR_PARAMETER, __LINE__ );
            break;
    }

    UIM_ResetListBoxIndex(); //Reset the listbox index since this function implies that we are moving forward in screenflow.
    //Jump To next screen 
    nextScreen.Type = ID; 
    nextScreen.ID = AUTHORIZATION;  //SCREEN_57
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SavePatientWeight ( int selection_ )
{
    SCREEN_FUNC_T nextScreen;
    
    //Selection of Highlighted List Box Item from  Patient Weight list
    s_CurrProgram.PatientWeight = ( PATIENT_WEIGHT_T )selection_;

    //Reset listbox index for next screen
    UIM_ResetListBoxIndex();

    //Jump To next screen
    nextScreen.Type = ID;
    nextScreen.ID = INFUSION_RAMP_UP;          
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}


////////////////////////////////////////////////////////////////////////////////
static void UIM_ProgramTypeSelection ( int selection_ )
{   
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;
    nextScreen.ID = NEEDLE_SET;         //SCREEN_68_2 
    
    switch( selection_ )
    {
        case CUSTOM:       //set program type custom
            s_CurrProgram.ProgType  = CUSTOM;
            break;

        case HYQVIA_TEMPLATE:   //set program type       
            s_CurrProgram.ProgType  = HYQVIA_TEMPLATE;
            UIM_SetHyqviaDefaults();
            break;     
            
        default:
            break;
    }
    
    UIM_ResetListBoxIndex();
    
    //Selection of Highlighted List Box Item from setup menu list
    s_ProgramType = ( PROGRAM_TYPE_T )selection_;
    
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_NeedleTypeSelection ( int selection_ )
{   
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;
    
    //Selection of Highlighted List Box Item from Needle Set menu list
    switch( selection_ )
    {
        case SINGLE_NEEDLE_LISTBOX:
            s_CurrProgram.ProgNeedleSet = SINGLE_NEEDLE_LINESET;
            break;

        case BIFURCATED_NEEDLE_LISTBOX:
            s_CurrProgram.ProgNeedleSet = BIFURCATED_NEEDLE_LINESET;
            break;

        default:
            UIM_Trace(ERR_VERIFICATION, __LINE__ );
            break;
    }
    
    //Reset s_ListBoxIndex
    UIM_ResetListBoxIndex();
    
    if ( CUSTOM == s_ProgramType )
    {
        nextScreen.ID = EDIT_DOSAGE_PROGRAM;    //SCREEN_47
    }
    else if ( HYQVIA_TEMPLATE == s_ProgramType )
    {
        nextScreen.ID = EDIT_PATIENT_WEIGHT;    //SCREEN_68
    }
    else
    {
        UIM_Trace(ERR_VERIFICATION, __LINE__ );
    }
    
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_InfusionRampUpSelection( int selection_ )
{
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;
    nextScreen.ID = EDIT_DOSAGE_PROGRAM;     //SCREEN_47

    s_CurrProgram.InfusionRampUp = ( INFUSION_RAMPUP_T )selection_; //Update infusion rampup
    
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_DoSiteCheck( void )
{
    ERROR_T err = ERR_NONE;
    
    SCREEN_FUNC_T nextScreen;
    bool siteCheckFlag = false;
    
    err = PM_GetNeedlePlacementCheckEnabled( &siteCheckFlag );
    UIM_Trace( err, __LINE__ );
    
    if( siteCheckFlag )
    {
        nextScreen.Type = ID;
        nextScreen.ID = START_PULL_BACK;
        UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
    }
    else
    {
        UIM_ReqNextState();
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_NeedleSetCheckScreen( void )
{
    ERROR_T err = ERR_NONE;
    SCREEN_FUNC_T nextScreen;
    LINESET_INFO_T needle;
    
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;

    err = INF_GetLinesetInfo( &needle );
    UIM_Trace( err, __LINE__ );

    err = PM_GetProgramInfo ( &s_CurrProgram );
    UIM_Trace( err, __LINE__ );

    if( needle == s_CurrProgram.ProgNeedleSet )
    {
        nextScreen.Type = ID;
        nextScreen.ID = PROGRAM_INFO1;
    }
    else
    {
        if( ( needle == SINGLE_NEEDLE_LINESET ) && 
            ( s_CurrProgram.ProgNeedleSet == BIFURCATED_NEEDLE_LINESET ) )
        {
            nextScreen.Type = ID;
            nextScreen.ID = NOTE_SINGLE_NEEDLE_DETECTED;
        }
        else if( ( needle == BIFURCATED_NEEDLE_LINESET ) && 
               ( s_CurrProgram.ProgNeedleSet == SINGLE_NEEDLE_LINESET ) )
        {
            nextScreen.Type = ID;
            nextScreen.ID = NOTE_BI_NEEDLE_DETECTED;
        }
        else
        {
            // NOTE: Execution will never come here
            UIM_Trace( ERR_PARAMETER, __LINE__ );
        }
    }

    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_UpdateProgramBasedOnNeedle( void )
{
    ERROR_T err = ERR_NONE;
    SCREEN_FUNC_T nextScreen;
    LINESET_INFO_T needle;

    err = INF_GetLinesetInfo( &needle );
    UIM_Trace( err, __LINE__ );

    err = PM_GetProgramInfoBasedOnNeedle( needle, &s_CurrProgram );
    UIM_Trace( err, __LINE__ );

    nextScreen.Type = ID;
    nextScreen.ID = PROGRAM_INFO1;

    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_DetermineStep( void )
{
    SCREEN_FUNC_T nextScreen;
    
    if( s_CurrProgram.ProgIgSteps > NUM_STEP_ROWS_PER_SCR )
    {
        nextScreen.Type = ID;
        nextScreen.ID = STEP_INFO2; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
    }
    else
    {
        nextScreen.Type = FP;
        nextScreen.FP = UIM_ReqNextState; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
    }

    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_DetermineBloodPath( int selection_ )
{
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;
    
    switch( selection_ )
    {
        case NO_BLOOD_IN_TUBING:
            nextScreen.Type = FP;
            nextScreen.FP = UIM_ReqNextState;
          break;

        case YES_BLOOD_IN_TUBING:
            nextScreen.Type = ID;
            nextScreen.ID = REMOVE_NEEDLE_BLOOD;
            s_HasPrimedFlag = false;                //since the yes option is selected for there being blood, priming will reset.
          break;

        default:
          break;
    }
    
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_DetermineHYComplete( int selection_ )
{   
    ERROR_T err = ERR_NONE;
    
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;
    
    bool aaStatus = false;
    
    err = AAM_isActive( END_OF_HY, &aaStatus ); //In the case that we had to press back to come to this screen we need to remove the alarm popup
                                               //that has the End Of Hy screen but the alarm has already been cleared.
                                                //In that scenario we have to delete the popup outside of HandleAA
    UIM_Trace( err, __LINE__ );
      
    switch( selection_ )
    {
        case 0:
            nextScreen.Type = ID;
            nextScreen.ID = REMOVE_SYRINGE;
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
          break;

        case 1:
            // User Found HY is not complete so start HY infusion again
            nextScreen.Type = ID;
            nextScreen.ID = RESUME_INF_HY;
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
          break;

        default:
          break;
    }
    
    if( aaStatus ) 
    {
        AAM_Clear( END_OF_HY );
    }
    else
    {
        UIM_DeletePopUp( s_AlarmAlertPopUp );
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_DetermineIGComplete( int selection_ )
{    
    SCREEN_FUNC_T nextScreen;
    ERROR_T err = ERR_NONE;
 
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;
    
    bool aaStatus = false;
    
    err = AAM_isActive( END_OF_IG, &aaStatus ); //In the case that we had to press back to come to this screen we need to remove the alarm popup
                                               //that has the End Of Hy screen but the alarm has already been cleared.
                                                //In that scenario we have to delete the popup outside of HandleAA
    UIM_Trace( err, __LINE__ );
    
    if( aaStatus ) 
    {
        AAM_Clear( END_OF_IG );
    }
    else
    {
        UIM_DeletePopUp( s_AlarmAlertPopUp );
    }
    
    switch( selection_ )
    {
        case 0:
            nextScreen.Type = FP;
            nextScreen.FP = UIM_ReqNextState;
          break;

        case 1:
            nextScreen.Type = FP;    
            nextScreen.FP = UIM_CompleteInfDelivery; //lint !e641 Ignoring conversion warning due to the way the multi state table is setup
          break;

        default:
          break;
    } 
    
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_DetermineInfScreen( void )
{
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;
    
    if( s_CurrStep != 0 )
    {
        nextScreen.ID = IG_INF_PROG;
    }
    else
    {
        nextScreen.ID = INF_PROG;
    }
    
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SilenceAndNav( void )
{
    SCREEN_FUNC_T nextScreen;
    ERROR_T err;
    SCREEN_INFO_T screenInfo;
    WM_HWIN *win;
    AM_MESSAGE_T amMsg;
    
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;
    
    UIM_GetActiveWinAndScreenInfo( &screenInfo, &win );

    // Silence the audio
    amMsg.EventType = ALERT;
    amMsg.Alert = AUDIO_NONE;
    err = AM_CommandNotify( &amMsg );
    UIM_Trace( err, __LINE__ );

    // Navigate to next screen
    switch( screenInfo.ScreenNumber)
    {
        case SCREEN_32: //AIL_S
            nextScreen.Type = ID;
            nextScreen.ID = AIL_DISCONNECT_NEEDLE;
            break;
        case SCREEN_33: //OCC_S
            nextScreen.Type = ID;
            nextScreen.ID = OCC_CLEAR_S;
            break;
        default:
            // Unexpected screen state
            UIM_Trace( ERR_VERIFICATION, __LINE__ );
            break;
    }

    UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SilenceAndNavInfo( void )
{
    SCREEN_FUNC_T nextScreen;
    ERROR_T err;
    SCREEN_INFO_T screenInfo;
    WM_HWIN *win;
    AM_MESSAGE_T amMsg;

    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;
   
    UIM_GetActiveWinAndScreenInfo( &screenInfo, &win );

    // Silence the audio
    amMsg.EventType = ALERT;
    amMsg.Alert = AUDIO_NONE;
    err = AM_CommandNotify( &amMsg );
    UIM_Trace( err, __LINE__ );

    // Navigate to next screen
    switch( screenInfo.ScreenNumber)
    {
        case SCREEN_32: //AIL_S
            nextScreen.Type = ID;
            nextScreen.ID = AIL_S_INFO;

            break;
        case SCREEN_33: //OCC_S
            nextScreen.Type = ID;
            nextScreen.ID = OCC_INFO_1_S;
            break;
        default:
            // Unexpected screen state
            UIM_Trace( ERR_VERIFICATION, __LINE__ );
            break;
    }

    UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ResetLedAndNav ( void )
{
    WM_HWIN *win;
    SCREEN_FUNC_T nextScreen;
    SCREEN_INFO_T screenInfo;
    
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;
    
    UIM_GetActiveWinAndScreenInfo( &screenInfo, &win );

    // Navigate to next screen
    switch(screenInfo.ScreenNumber)
    {
        case SCREEN_32_7:
            nextScreen.Type = ID;
            nextScreen.ID = RESUME_INF_AIL;
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
            break;
        case SCREEN_33_1:
            nextScreen.Type = ID;
            nextScreen.ID = RESUME_INF_OCC;
            UIM_ChangeScreen( &nextScreen , ALARM_TYPE );
            break;
        default:
            // Unexpected screen state
            UIM_Trace( ERR_VERIFICATION, __LINE__ );
            break;
    }

    UIM_HandlePumpStatus( NUM_OF_PUMP_STATUS, false ); // Restore the status LED
}

////////////////////////////////////////////////////////////////////////////////
//This function is used to determine what screen to go to next when there are multiple options on screen.
static void UIM_DeterminePath( void )
{
    WM_HWIN *win;
    
    SCREEN_INFO_T screenInfo;
        
    unsigned int listSize;

    UIM_GetActiveWinAndScreenInfo( &screenInfo, &win );
    
    int selection;
    WM_HWIN hItemLB;
    hItemLB = WM_GetDialogItem( *win, PRI_LISTBOX );

    if( hItemLB )
    {
        selection = LISTBOX_GetSel( hItemLB );  
        listSize = LISTBOX_GetNumItems( hItemLB );
    }
    else
    {
        UIM_Trace( ERR_GUI, __LINE__ );
        return;
    }

    switch( screenInfo.ScreenNumber )
    {
        case SCREEN_9:          // Start Setup
            UIM_DeviceModeSelection( selection );
            break;  
      
        case SCREEN_21:         // BLOOD_IN_TUBING
            UIM_DetermineBloodPath( selection );
            break;
            
        case SCREEN_24:         //HY_COMPLETE
            UIM_DetermineHYComplete( selection );
            break;
            
        case SCREEN_31_1:       //IG_STEP_ENDING
            UIM_DetermineRate( selection );
            break;

        case SCREEN_35:         //IG_COMPLETE
            UIM_DetermineIGComplete( selection );
            break;

  		case SCREEN_37_3:       //  Flush Complete Selection
            UIM_FlushCompleteSelection( selection );
            break;

        case SCREEN_39:         // SETTINGS_MENU
            UIM_SettingsMenuSelection( selection, ( uint16_t )listSize );
            break;
            
        case SCREEN_39_5:       //INDICATION_MENU
            UIM_IndicationSelection( selection );
            break; 
            
        case SCREEN_39_8:       //LANGUAGE_MENU
            UIM_LanguageSelection( selection );
            break;
            
		case SCREEN_43_1:        //  EXIT_AUTHORIZED_SETUP_SELCTION
            UIM_ExitAuthorizedSetupSelection( selection );
            break;
            
	    case SCREEN_46:         // Program Type 
            UIM_ProgramTypeSelection( selection );
            break;
            
        case SCREEN_57:         // Authorized Menu 
            UIM_AuthorizedMenuSelection( selection );
            break;
            
		case SCREEN_60_1:        //ALLOW_RATE_CHANGES 
            UIM_SaveAllowRateChange( selection );
            break;
            
        case SCREEN_66_1:        // NEEDLE_CHECK
            UIM_SaveNeedleCheckStatus( selection );
            break;
            
        case SCREEN_67_1:        // FLUSH
            UIM_SaveFlushStatus( selection );
            break;
         
        case SCREEN_68:        //EDIT_PATIENT_WEIGHT
            UIM_SavePatientWeight( selection );
            break;

        case SCREEN_68_2:        //NEEDLE_SET
            UIM_NeedleTypeSelection( selection );
            break;
            
        case SCREEN_73_1:       // POWER_LOSS_RECOVERY
            UIM_ResumeInfusionSelect( selection );
            break;

        case SCREEN_74_1:        //INFUSION_RAMP_UP
            UIM_InfusionRampUpSelection( selection );
            break;
            
        case SCREEN_100:        //USB_ENABLE
            UIM_UsbEnableStatus( selection );
            break;
            
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
//This function is used to determine path when info button is pressed instead of SELECT
static void UIM_DetermineInfoPath( void )
{
    WM_HWIN hItem;
    SCREEN_FUNC_T nextScreen;
    SCREEN_INFO_T screenInfo;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );

    hItem = WM_GetDialogItem( s_BaseWindow, PRI_LISTBOX );  
    if( hItem == 0 )
    {
         UIM_Trace( ERR_GUI, __LINE__ );
    }

    s_ListBoxIndex = LISTBOX_GetSel( hItem ); 

    switch( screenInfo.ScreenNumber )
    {
        case SCREEN_9:          // Start Setup
        {
            switch( s_ListBoxIndex )
            {
                case 0:
                    nextScreen.Type = ID;
                    nextScreen.ID = START_SETUP_INFO1_MORE;
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                case 1:
                    nextScreen.Type = ID;
                    nextScreen.ID = START_SETUP_INFO2_MORE;
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                default:
                    break;
            }
        }
        break;
        
        case SCREEN_39_5:          // INDICATION_MENU
        {
            switch( s_ListBoxIndex )
            {
                case 0:
                    nextScreen.Type = ID;
                    nextScreen.ID = INDICATION_PI_INFO_MORE;    //SCREEN_39_5I
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                case 1:
                    nextScreen.Type = ID;
                    nextScreen.ID = INDICATION_CIDP_INFO_MORE;  //SCREEN_39_6I
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                default:
                    break;
            }
        }
        break;
        
        case SCREEN_46:          // PROGRAM_TYPE
        {
            switch( s_ListBoxIndex )
            {
                case 0:
                    nextScreen.Type = ID;
                    nextScreen.ID = CUSTOM_INFO_MORE;
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                case 1:
                    nextScreen.Type = ID;
                    nextScreen.ID = HYQVIA_INFO_MORE;
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                default:
                    break;
            }
        }
        break;
        
        case SCREEN_60_1:          // LOCK_RATE
        {
            switch( s_ListBoxIndex )
            {
                case YES_LISTBOX:
                    nextScreen.Type = ID;
                    nextScreen.ID = ALLOW_RATE_CHANGES_INFO1_MORE;
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                case NO_LISTBOX:
                    nextScreen.Type = ID;
                    nextScreen.ID = ALLOW_RATE_CHANGES_INFO2_MORE;
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                default:
                    break;
            }
        }
        break;
        
        case SCREEN_66_1:          // NEEDLE_CHECK
        {
            switch( s_ListBoxIndex )
            {
                case YES_LISTBOX:
                    nextScreen.Type = ID;
                    nextScreen.ID = NEEDLE_CHECK_MORE_ON_INFO1;
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                case NO_LISTBOX:
                    nextScreen.Type = ID;
                    nextScreen.ID = NEEDLE_CHECK_MORE_OFF_INFO2;
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                default:
                    break;
            }
        }
        break;
        
        case SCREEN_67_1:          // FLUSH
        {
            switch( s_ListBoxIndex )
            {
                case YES_LISTBOX:
                    nextScreen.Type = ID;
                    nextScreen.ID = FLUSH_INFO1_MORE;
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                case NO_LISTBOX:
                    nextScreen.Type = ID;
                    nextScreen.ID = FLUSH_INFO2_MORE;
                    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
                    break;
                    
                default:
                    break;
            }
        }
        break;
            
        default:
            break;
    } 
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_DetermineRate( int selection_ )
{
    ERROR_T err = ERR_NONE;
    INF_MESSAGE_T infMsg;
        
    //The default selection is to maintain the lowered rate of the user, which requires no action
    //The raised value is the value for the next step.
    
    if( selection_ == 0 )
    {
        infMsg.ManRate = s_CurrProgram.ProgStepFlowRate[s_CurrStep];
         
         infMsg.InfEventType = INF_MAN_RATE_ADJ;
         
         err = INF_SendMessage( &infMsg );
         UIM_Trace( err, __LINE__ );
        
    }
    else if( selection_ == 1 )
    {
        for( int i=(s_CurrStep + 1); i<=s_CurrProgram.ProgIgSteps; i++ )
        {
            err = PM_GetIgStepFlowrate( ( uint8_t )i, &s_CurrProgram.ProgStepFlowRate[i] );
            UIM_Trace( err, __LINE__ );
        }
         
        infMsg.ManRate = s_CurrProgram.ProgStepFlowRate[s_CurrStep + 1];

        infMsg.InfEventType = INF_MAN_RATE_ADJ;

        err = INF_SendMessage( &infMsg );
        UIM_Trace( err, __LINE__ );
    }
    
    AAM_Clear( STEP_ENDING );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_RevertBrightness( void )
{
    ERROR_T status;
    uint8_t brightness = 0;
    SCREEN_FUNC_T nextScreen;
    
    // Get Brightness which is saved in file system
    status = CFG_GetBrightness( &brightness );
    UIM_Trace( status, __LINE__ );

    UIM_SetDefaultBrightnessLevel( brightness );
    
    // Adjust Brightness
    status = LED_AdjustBrightnessPct( brightness, false );
    UIM_Trace( status, __LINE__ );
    
    nextScreen.Type = ID;
    nextScreen.ID = SETTINGS_MENU;
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_RevertVolume ( void )
{
    ERROR_T status = ERR_NONE; 
    
    SCREEN_FUNC_T nextScreen;
    uint8_t volume = DEFAULT_VOL_BRIGHT_LVLS;
    
    //Gather the volume level from the last stored value.
    status = CFG_GetVolume( (uint8_t *)&volume );
    UIM_Trace( status, __LINE__ );

    if( volume > MAX_PERCENT )  // if got invalid data from flash
    {
        volume = DEFAULT_VOL_BRIGHT_LVLS;     // set default volume value
    }
    
    UIM_SetDefaultVolumeLevel( volume );
       
    UIM_SetVolumeLevel( false );    //set current volume level and not save into flash
    
    nextScreen.Type = ID;
    nextScreen.ID = SETTINGS_MENU;
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );

}

///////////////////////////////////////////////////////////////////////////////
void UIM_SetLevel ( void )
{
    SCREEN_INFO_T screenInfo;
    SCREEN_FUNC_T nextScreen;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    switch( screenInfo.ScreenNumber )
    {
        case SCREEN_40:         //ADJUST_BRIGHTNESS
            UIM_SetBrightnessLevel( true );
            nextScreen.Type = ID;
            nextScreen.ID = SETTINGS_MENU;
            s_ListBoxIndex = BRIGHTNESS_LISTBOX;        //Update list box for the next screen
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            break;
              
        case SCREEN_41:         //ADJUST_VOLUME
            UIM_SetVolumeLevel( true );
            nextScreen.Type = ID;
            nextScreen.ID = SETTINGS_MENU;          
            s_ListBoxIndex = VOLUME_LISTBOX;            //Update list box for the next screen
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            break;
          
        case SCREEN_29_4I:      //PROG_BRIGHTNESS
            UIM_SetBrightnessLevel( true );
            nextScreen.Type = ID;
            nextScreen.ID = PROG_SOUND_VOL;
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            break;
		  
	    default:
		  break;
    }
}

////////////////////////////////////////////////////////////////////////////////
//Audio manager will save the settings to the configuration manager
void UIM_SetVolumeLevel ( bool saveFlag_ )
{
    ERROR_T err = ERR_NONE;
    AM_MESSAGE_T amMsg;
    
    uint8_t volumeLevel = 0;    
    UIM_GetVolumeLevel( &volumeLevel );
    amMsg.EventType = VOLUME;
    amMsg.Volume = ( uint8_t )( volumeLevel * NUM_VOL_BRIGHT_LVLS );
    amMsg.SaveVolFlag = saveFlag_;
    
    err = AM_CommandNotify( &amMsg );
    UIM_Trace( err, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
//LED manager will save the settings to the configuration manager
void UIM_SetBrightnessLevel ( bool saveFlag_ )
{
    ERROR_T status;
    uint8_t brightnessLevel = 0;
    
    status = UIM_GetBrightnessLevel( &brightnessLevel );
    UIM_Trace( status, __LINE__ );
    
    brightnessLevel = ( uint8_t )( brightnessLevel * NUM_VOL_BRIGHT_LVLS );
    status = LED_AdjustBrightnessPct( brightnessLevel, saveFlag_ );
    UIM_Trace( status, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_EndOfTherapyOnBack ( void )
{
    SCREEN_INFO_T screenInfo;
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    switch( screenInfo.ScreenNumber )
    {
        case SCREEN_25: //REMOVE_SYRINGE
        case SCREEN_32_3: //RESUME_INF_HY
            nextScreen.ID = HY_COMPLETE_S;
            break;
          
        case SCREEN_36_1: //CONNECT_FLUSH
            UIM_ReqAbortState();
            nextScreen.ID = IG_COMPLETE_S;
            break;
                     
        default:
            UIM_Trace( ERR_PARAMETER, __LINE__ );
            break;
    }

    UIM_ChangeScreen( &nextScreen, ALARM_TYPE );

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ProcessUpDownInInfusion ( void )
{
    ERROR_T err = ERR_NONE;
    SCREEN_FUNC_T nextScreen;
    bool rateLockFlag = false;

    err = PM_GetRateChangeAllowed( &rateLockFlag );
    UIM_Trace( err, __LINE__ );
    
    if( ( !err ) && ( rateLockFlag == false ) )
    {
        nextScreen.Type = ID;
        nextScreen.ID = INF_PAUSE_PROMPT;
        UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
        err = UIM_CreatePromptTimer( PAUSE_PROMPT_SCREEN_DURATION );
        if ( err != ERR_NONE )
        {
            UIM_Trace( err, __LINE__ );     ///there is issue with UIM
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_ProcessInfoOnPausePrompt( void )
{
    SCREEN_FUNC_T nextScreen;
    ERROR_T err;
    
    if( s_PromptTimer != NULL )
    {
        err = TF_DeleteTimer( &s_PromptTimer );
        UIM_Trace(err,__LINE__);
    }

    nextScreen.Type = ID;
    nextScreen.ID = PROG_INFO1;

    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

////////////////////////////////////////////////////////////////////////////////
void UIM_GetWindow( WM_HWIN *currentWindow_ )
{
    if( currentWindow_ != NULL )
    {
        if( s_PowerPopUp > 0 )
        {
            *currentWindow_ = s_PowerPopUp;
        }
        else if( s_AlarmAlertPopUp > 0 )
        {
            *currentWindow_ = s_AlarmAlertPopUp;
        }
        else
        {
            *currentWindow_ = s_BaseWindow;
        }
    }
    else
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_GetStep( uint16_t *step_ )
{
    if( step_ != NULL )
    {
        *step_ = s_CurrStep;
    }
    else
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_GetCurrProg( PROGRAM_INFO_T *prog_ )
{
    if( prog_ != NULL )
    {
        *prog_ = s_CurrProgram;
    }
    else
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_GetVolInfused( float *vol_ )
{
    if( vol_ != NULL )
    {
        *vol_ = s_VolumeInfused;
    }
    else
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_GetBattLeft( uint8_t *batt_ )
{
    ERROR_T err;
    uint8_t batteryPercentage;
        
    if( batt_ != NULL )
    {
        // Get Battery Percenatge
        err = BATT_GetBatteryPercentage( &batteryPercentage );
        UIM_Trace( err, __LINE__ );      
      
        *batt_ = batteryPercentage;
    }
    else
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_IncRate( void )
{
    ERROR_T err = ERR_NONE;
    bool rateLockFlag = false;
    RATE_VALUE_T origRate = ML_0;
    SCREEN_INFO_T screenInfo;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );

    err = PM_GetRateChangeAllowed( &rateLockFlag );
    UIM_Trace( err, __LINE__ );

    if( ( !err ) && ( rateLockFlag == false ) )
    {
        s_IsOffProgram = FALSE;
        if ( s_CurrStep == HY_STEP_INDEX )
        {
            err = PM_GetHyStepFlowrate ( &origRate );
            UIM_Trace( err, __LINE__ );
            
            if ( ( s_CurrProgram.ProgStepFlowRate[HY_STEP_INDEX] < PM_HY_FLOWRATE_MAX ) &&
                 ( s_CurrProgram.ProgStepFlowRate[HY_STEP_INDEX] < origRate ) ) //Can never increase the rate higher than the step
            {
                s_CurrProgram.ProgStepFlowRate[HY_STEP_INDEX] = (RATE_VALUE_T)(s_CurrProgram.ProgStepFlowRate[HY_STEP_INDEX] + PM_HY_STEP_INCREMENT);

                UIM_ChangeScreen( &screenInfo.SubStateScreen, screenInfo.ScreenType );
            }
        }
        else
        {
            err = PM_GetIgStepFlowrate ( ( uint8_t )s_CurrStep, &origRate );
            UIM_Trace( err, __LINE__ );
            
            if ( s_CurrProgram.ProgStepFlowRate[s_CurrStep] < origRate )  //Can never increase the rate higher than the step was originally set to.
            {
                s_IsOffProgram = TRUE;
                s_CurrProgram.ProgStepFlowRate[s_CurrStep]++;

                // Skip 320 as it is an invalid flowrate
                if( s_CurrProgram.ProgStepFlowRate[s_CurrStep] == ML_320 )
                {
                    s_CurrProgram.ProgStepFlowRate[s_CurrStep]++;
                }

                UIM_ChangeScreen( &screenInfo.SubStateScreen, screenInfo.ScreenType );
            }

            if( s_CurrProgram.ProgStepFlowRate[s_CurrStep] == origRate )
            {
                s_IsOffProgram = FALSE;
                for( int i=s_CurrStep; i<=s_CurrProgram.ProgIgSteps; i++ )
                {
                    err = PM_GetIgStepFlowrate ( ( uint8_t )i, &s_CurrProgram.ProgStepFlowRate[i] );
                    UIM_Trace( err, __LINE__ );
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_DecRate( void )
{
    ERROR_T err = ERR_NONE;
    bool rateLockFlag = false;
    SCREEN_INFO_T screenInfo;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    err = PM_GetRateChangeAllowed( &rateLockFlag );
    UIM_Trace( err, __LINE__ );
    
    if ( rateLockFlag == false )
    {
        s_IsOffProgram = FALSE;
        if ( s_CurrStep == HY_STEP_INDEX )
        {
            if( s_CurrProgram.ProgStepFlowRate[s_CurrStep] > PM_HY_FLOWRATE_MIN )
            {
                s_CurrProgram.ProgStepFlowRate[s_CurrStep] = (RATE_VALUE_T)(s_CurrProgram.ProgStepFlowRate[s_CurrStep] - (RATE_VALUE_T)PM_HY_STEP_INCREMENT);

                UIM_ChangeScreen( &screenInfo.SubStateScreen , screenInfo.ScreenType );
            }
        }
        else
        {
            if( s_CurrProgram.ProgStepFlowRate[s_CurrStep] > PM_IG_FLOWRATE_MIN )
            {
                s_IsOffProgram = TRUE;
                s_CurrProgram.ProgStepFlowRate[s_CurrStep]--;

                // Skip 320 as it is an invalid flowrate
                if( s_CurrProgram.ProgStepFlowRate[s_CurrStep] == ML_320 )
                {
                    s_CurrProgram.ProgStepFlowRate[s_CurrStep]--;
                }

                UIM_ChangeScreen( &screenInfo.SubStateScreen, screenInfo.ScreenType );
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_CommandNotify (  const UIM_EVENT_MESSAGE_T *event_ )
{
    ERROR_T status;
    status = TF_PostMsg(&s_UimTaskDesc, (const void *)event_);
    UIM_Trace(status, __LINE__);
    return status;
}

///////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetProgramType ( PROGRAM_TYPE_T *programType_ )
{
    if( programType_ != NULL )
    {
        *programType_ = s_ProgramType;
         return ERR_NONE;
    }
    else
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return ERR_PARAMETER;
    }
}
////////////////////////////////////////////////////////////////////////////////
static void UIM_Increment ( void )
{
    GUI_HWIN currWindow = 0;
    
    SCREEN_FUNC_T nextScreen;
    UIM_GetWindow( &currWindow );
    
    SCREEN_INFO_T screenInfo;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );

    switch( screenInfo.ScreenNumber )
    {
        case SCREEN_44:          //PASSCODE_SCREEN

            UIM_PasscodeAction( currWindow, true, false );

            break;
            
        case SCREEN_30_4I:         //PROG_PAUSE_BRIGHT
        case SCREEN_30_5I:         //PROG_PAUSE_SOUND
        case SCREEN_29_4I:         //PROG_BRIGHTNESS
        case SCREEN_29_5I:         //PROG_SOUND_VOL
        case SCREEN_40:            //ADJUST_BRIGHTNESS
        case SCREEN_41:            //ADJUST_VOLUME
            
            UIM_AdjustLevel( currWindow, true, false );
            
            break;
            
        case SCREEN_47:                 // Dosage Program

            UIM_DosageAction( currWindow, true, false );
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_DOSAGE_PROGRAM;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break;

        case SCREEN_48:                 //Hy Flow Rate

            UIM_HyFlowRateAction( currWindow, true, false );
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_HY_FLOW_RATE;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break;

        case SCREEN_49:                  //EDIT_IG_STEPS

            UIM_IgStepAction( currWindow, true, false );
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_IG_STEPS;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break;

        case SCREEN_50:                  //EDIT_IG_STEPS_FLOW_RATE

            UIM_IgFlowrateAction( currWindow, true, false );
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_IG_STEPS_FLOW_RATE;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break;

        case SCREEN_51:        			 //EDIT_IG_STEPS_DURATION

            UIM_IgDurationAction( currWindow, true, false ); 
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_IG_STEPS_DURATION;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break;

        case SCREEN_69:                 //HYQVIA_IG_TIME

            UIM_IgTimeIntervalAction( currWindow, true, false ); 
            nextScreen.Type = ID;
            nextScreen.ID = HYQVIA_IG_TIME;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break; 

        default:
          
           UIM_ListboxIncrementAction( currWindow );
        
        break;
    }
}

///////////////////////////////////////////////////////////////////////////////
static void UIM_Decrement ( void )
{
    SCREEN_FUNC_T nextScreen;
    
    GUI_HWIN currWindow = 0;

    UIM_GetWindow( &currWindow );
    
    SCREEN_INFO_T screenInfo;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    switch( screenInfo.ScreenNumber )
    {
        case SCREEN_44:                  //PASSCODE_SCREEN

            UIM_PasscodeAction( currWindow, false, true );

            break;
            
        case SCREEN_30_4I:         //PROG_PAUSE_BRIGHT
        case SCREEN_30_5I:         //PROG_PAUSE_SOUND
        case SCREEN_29_4I:         //PROG_BRIGHTNESS
        case SCREEN_29_5I:         //PROG_SOUND_VOL
        case SCREEN_40:            //ADJUST_BRIGHTNESS
        case SCREEN_41:            //ADJUST_VOLUME
            
            UIM_AdjustLevel( currWindow, false, true );

            break;   
            
        case SCREEN_47:                 // Dosage Program

            UIM_DosageAction( currWindow, false, true );
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_DOSAGE_PROGRAM;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break;

        case SCREEN_48:                 //Hy Flow Rate

            UIM_HyFlowRateAction( currWindow, false, true );
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_HY_FLOW_RATE;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break;

        case SCREEN_49:                  //EDIT_IG_STEPS  

            UIM_IgStepAction( currWindow, false, true );
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_IG_STEPS;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break;

        case SCREEN_50:                  //EDIT_IG_STEPS_FLOW_RATE

            UIM_IgFlowrateAction( currWindow, false, true );
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_IG_STEPS_FLOW_RATE;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break;

        case SCREEN_51:        			 //EDIT_IG_STEPS_DURATION

            UIM_IgDurationAction( currWindow, false, true );
            nextScreen.Type = ID;
            nextScreen.ID = EDIT_IG_STEPS_DURATION;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break;

        case SCREEN_69:                 //HYQVIA_IG_TIME

            UIM_IgTimeIntervalAction( currWindow, false, true ); 
            nextScreen.Type = ID;
            nextScreen.ID = HYQVIA_IG_TIME;          
            UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
            
            break; 
              
        default:
            
            UIM_ListboxDecrementAction( currWindow );
           
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetListboxIndex ( int *listBoxIndex_ )
{
    if( listBoxIndex_ != NULL )
    {
        *listBoxIndex_ = s_ListBoxIndex;
         return ERR_NONE;
    }
    else
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return ERR_PARAMETER;
    }
}

///////////////////////////////////////////////////////////////////////////////
static void UIM_ResetListBoxIndex( void )
{
    s_ListBoxIndex = 0; //Always Set 0 value for reset apart from it will generate issue
}

///////////////////////////////////////////////////////////////////////////////
//Each case is the screen that is currently displayed while the listboxIndex is what will be displayed on the next screen.
static void UIM_SetBackButtonListboxIndex ( void )
{
    SCREEN_INFO_T screenInfo;
    WM_HWIN *win;
    int selection;
    WM_HWIN hItemLB;

    UIM_GetActiveWinAndScreenInfo( &screenInfo, &win );
    
    switch( screenInfo.ScreenNumber )
    {
        case SCREEN_12:          // PROGRAM_INFO1
            switch ( screenInfo.SubStateScreen.ID )
            {  
                case REVIEW_EDIT_PROGRAM_INFO1: //Going to FLUSH (SCREEN_67_1)
                case HYQVIA_PROGRAM_INFO1:      //Going to FLUSH (SCREEN_67_1)
                     if ( s_CurrProgram.FlushEnabled )
                     {
                          s_ListBoxIndex = YES_LISTBOX;
                     }
                     else
                     {
                          s_ListBoxIndex = NO_LISTBOX;
                     }
                     break;
                     
                default:
                     break;
            }
            break;

       case SCREEN_22:
           s_ListBoxIndex = YES_BLOOD_IN_TUBING;
           break;

       case SCREEN_25:                    //REMOVE_SYRINGE
            s_ListBoxIndex = YES_LISTBOX;
            break;  

       case SCREEN_32_3:                    //Going from RESUME_INF_HY to HY_COMPLETE_S (SCREEN_24)
            s_ListBoxIndex = NO_LISTBOX;
            break;

       case SCREEN_36_1:                    //CONNECT_FLUSH
            s_ListBoxIndex   = YES_LISTBOX;
            break;   
            
        case SCREEN_39:         // Going from SETTINGS_MENU to AUTHORIZATION (SCREEN_57)
            s_ListBoxIndex = PUMP_SETTINGS_LISTBOX;
            break;

        case SCREEN_39_5:       // Going from INDICATION_MENU to SETTINGS_MENU (SCREEN_39)
            s_ListBoxIndex = INDICATION_LISTBOX;
            break;

        case SCREEN_39_8:        // Going from LANGUAGE_MENU to SETTINGS_MENU (SCREEN_39)
            s_ListBoxIndex = LANGUAGE_LISTBOX;
            break;

        case SCREEN_40:         // Going from ADJUST_BRIGHTNESS to SETTINGS_MENU (SCREEN_39)
            s_ListBoxIndex = BRIGHTNESS_LISTBOX;
            break;

        case SCREEN_41:         // Going from ADJUST_VOLUME to SETTINGS_MENU (SCREEN_39)
            s_ListBoxIndex = VOLUME_LISTBOX;
            break;
            
        case SCREEN_43_1:
            s_ListBoxIndex = s_AuthMenuLBIndex;
            break;

        case SCREEN_44:
        case SCREEN_44_1:
            s_ListBoxIndex = DEVICE_MODE_SETUP;
            break;
            
        case SCREEN_46:           //PROGRAM_TYPE
            s_ListBoxIndex = EDIT_PROGRAM_LISTBOX;
            break;
        
        case SCREEN_47:    // Going from EDIT_DOSAGE_PROGRAM to NEEDLE_SET (SCREEN_68_2)
        case SCREEN_68:    // Going from EDIT_PATIENT_WEIGHT to NEEDLE_SET (SCREEN_68_2)
            if( s_CurrProgram.ProgNeedleSet == BIFURCATED_NEEDLE_LINESET )
            {
                s_ListBoxIndex = BIFURCATED_NEEDLE_LISTBOX;
            }
            else
            {
                s_ListBoxIndex = SINGLE_NEEDLE_LISTBOX;
            }
           break;
                        
        case SCREEN_57:         // Authorized Menu
            hItemLB = WM_GetDialogItem( *win, PRI_LISTBOX );

            if( hItemLB != 0)
            {
                selection = LISTBOX_GetSel( hItemLB );
                s_AuthMenuLBIndex = selection;
            }

            s_ListBoxIndex = YES_LISTBOX;
            break;
            
        case SCREEN_66_1:        // NEEDLE_CHECK going to ALLOW_RATE_CHANGES
            if( s_CurrProgram.RateLocked )
            {
                s_ListBoxIndex = YES_LISTBOX;
            }
            else
            {
                s_ListBoxIndex = NO_LISTBOX;
            }
            break;
            
        case SCREEN_67_1:        // FLUSH going to NEEDLE_CHECK
            if( s_CurrProgram.NeedlePlacementCheckEnabled )
            {
                s_ListBoxIndex = YES_LISTBOX;
            }
            else
            {
                s_ListBoxIndex = NO_LISTBOX;
            }
            break;
                     
        case SCREEN_68_2:        //NEEDLE_SET
            if( s_ProgramType == HYQVIA_TEMPLATE )
            {
                s_ListBoxIndex = HYQVIA_TEMPLATE;
            }
            else
            {
                s_ListBoxIndex = CUSTOM;
            }
            break;
            
        case SCREEN_74_1:      //Going from INFUSION_RAMP_UP to Patient Screen
            if ( GREATER_88 == s_CurrProgram.PatientWeight )
            {
                s_ListBoxIndex = GREATER_88;
            }
            else if ( LESS_EQUAL_88 == s_CurrProgram.PatientWeight )
            {
                s_ListBoxIndex = LESS_EQUAL_88;
            }
            else
            {
                UIM_Trace(ERR_VERIFICATION, __LINE__ );
            }
            break;
            
        default:
            break;
    }

}

///////////////////////////////////////////////////////////////////////////////
static void UIM_DetermineInfInfoScreen( void )
{
    SCREEN_INFO_T screenInfo;
    SCREEN_FUNC_T nextScreen;
    
    nextScreen.Type = ID;
    nextScreen.ID = NO_CHANGE;
    
    UIM_GetActivePopUpScreenInfo( &screenInfo );
    
    switch( screenInfo.ScreenNumber )
    {
        case SCREEN_29_1I:   // PROG_INFO1 
        {
            if ( s_CurrStep == HY_STEP_INDEX )
            {
                nextScreen.Type = ID;
                nextScreen.ID = PROG_INFO3;
            }
            else
            {
                nextScreen.Type = ID;
                nextScreen.ID = PROG_INFO2;
            }
        }
        break;
        
        case SCREEN_29_3I:   // PROG_INFO3 
        {
            if ( s_CurrStep == HY_STEP_INDEX )
            {
                nextScreen.Type = ID;
                nextScreen.ID = PROG_INFO1;
            }
            else
            {
                nextScreen.Type = ID;
                nextScreen.ID = PROG_INFO2;
            }
        }
        break;
          
        case SCREEN_30_1I:   // PROG_PAUSE_INFO1
        {
            if ( s_CurrStep == HY_STEP_INDEX )
            {
                nextScreen.Type = ID;
                nextScreen.ID = PROG_PAUSE_INFO3;
            }
            else
            {
                nextScreen.Type = ID;
                nextScreen.ID = PROG_PAUSE_INFO2;
            }
        }
        break;
          
        case SCREEN_30_3I:   // PROG_PAUSE_INFO3
        {
            if ( s_CurrStep == HY_STEP_INDEX )
            {
                nextScreen.Type = ID;
                nextScreen.ID = PROG_PAUSE_INFO1;
            }
            else
            {
                nextScreen.Type = ID;
                nextScreen.ID = PROG_PAUSE_INFO2;
            }
        }
        break;
    }
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

///////////////////////////////////////////////////////////////////////////////
static void UIM_LowBatteryAck( void )
{
    ERROR_T err = ERR_NONE;
    err = AAM_Clear( LOW_BATT );
    UIM_Trace( err, __LINE__ );
}

///////////////////////////////////////////////////////////////////////////////
static void UIM_ReplaceBatteryAck ( void )
{
    ERROR_T err = ERR_NONE;
    err = AAM_Clear( REPLACE_BATT );
    UIM_Trace( err, __LINE__ );
}

///////////////////////////////////////////////////////////////////////////////
static void UIM_PrimingErrAck( void )
{
    ERROR_T err = ERR_NONE;
    err = AAM_Clear( PRIMING_ERR );
    UIM_Trace( err, __LINE__ );
}

///////////////////////////////////////////////////////////////////////////////
static void UIM_ExitReplaceNeedle( void )
{
    UIM_EVENT_MESSAGE_T msg;
    bool active = false;
    ERROR_T status = ERR_NONE;

    UIM_ReqAbortState();

    // The lineset alarm was suppressed on the previous screens
    // while the user opened the door. Display the alarm now if it is still
    // active.
    AAM_isActive( LINESET_ALARM, &active );

    if( active )
    {
        // Post a message. We need the base window screen to change first
        // otherwise the alarm/alert handler will still suppress this alarm.
        msg.EventType = UIM_ALERT_OR_ALARM;
        msg.AlarmAlert = LINESET_ALARM;

        status = UIM_CommandNotify( &msg );
        UIM_Trace( status, __LINE__ );
    }
}
////////////////////////////////////////////////////////////////////////////////
static void UIM_EditIgStepBackButton ( void )
{
    SCREEN_FUNC_T nextScreen;
    nextScreen.Type = ID;
    if( s_CurrProgram.ProgNeedleSet == BIFURCATED_NEEDLE_LINESET )
    {
        nextScreen.ID = BIFURCATED_NOTE_HY_CUSTOM; //SCREEN_71
    }
    else
    {
        nextScreen.ID = EDIT_HY_FLOW_RATE;    // SCREEN_48
    }
    UIM_ChangeScreen( &nextScreen, INFUSION_TYPE );
}

///////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetTaskControlBlock(TF_DESC_T **pDesc_)
{
    if( pDesc_ == NULL )
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return ERR_PARAMETER;
    }

    *pDesc_= &s_UimTaskDesc;
    return ERR_NONE;
}

///////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetPasscodeInfo( PASSCODE_T *passcodeInfo_ )
{
    if( passcodeInfo_ != NULL )
    {
        *passcodeInfo_ = s_Passcode;
         return ERR_NONE;
    }
    else
    {
        UIM_Trace( ERR_PARAMETER, __LINE__ );
        return ERR_PARAMETER;
    }
}
