////////////////////////////////////////////////////////////////////////////////
//   Copyright (c) 2016 Flextronics Medical
//   *** Confidential Company Proprietary ***
///
///  \file CLI.c
///  \brief CLI (CLI) provides the methods and properties to interpret user
///   and or manufacturing commands input via the UART and to call the proper
///   methods to implement those commands.
//
//
////////////////////////////////////////////////////////////////////////////////
#ifndef _CLI_C_
#define _CLI_C_

#include <string.h>

#include "Common.h"
#include "CLI.h"
#include "CLI_Commands.h"
#include "CLI_ProgramCreate.h"
#include "CommonTypes.h"
#include "SafetyMonitor.h"
#include "SerialManager.h"
#include "UART.h"

////////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND #DEFINES
////////////////////////////////////////////////////////////////////////////////
#define CLI_TIMEOUT_MS ( 8000 )

////////////////////////////////////////////////////////////////////////////////
// ENUMS, TYPEDEFS, AND STRUCTURES
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Global Variables
////////////////////////////////////////////////////////////////////////////////

char g_ParsedMsg[ COMMAND_PARMS_MAX ][ COMMAND_LENGTH_MAX ];

uint16_t g_LongestSyntaxLength = 0;
uint16_t g_LongestCmdLength    = 0;
uint8_t g_ParmCount;                   ///Count for the number of parameters parsed with command.

char g_PadCmd[ MAX_PADDING_CHARS ];
char g_PadSyntax[ MAX_PADDING_CHARS ];

// CLI task stack space
static OS_STK s_CLITaskStk[ CLI_TASK_STK_SIZE ];

static SERIAL_CMD_SOURCE_T s_Source = SERIAL_UART; //Default as UART but changes when latest command is different from current source.

////////////////////////////////////////////////////////////////////////////////
// Command List
// Add new commands to this list then implement command handler. Don't forget
// to add the function prototype.
////////////////////////////////////////////////////////////////////////////////
const CLI_COMMANDS_T COMMANDS[] =
{
    {"aa", "alarm/alert#", CLI_AlarmAlertHandler,
     "Set, Clear, or Query system alarm or alert."},

    {"adc", "[Channel]", CLI_ReadAdcDataCmdHandler,
     "Read Mention channel's Voltage Data"},

    {"ail", "", CLI_Ail,
     "AIL command set. Enter \"ail\" for command set."},

    {"am", "event|vol event#|volumeLvl", CLI_SetToneSeriesHandler,
     "Set audio tone output based upon event in alarms/alerts/notification table. Also sets volume."},

    {"current", "", CLI_ReadPackCurrentCmdHandler,
     "Read Charger Pack Current"},

    {"dac", "amp freq", CLI_DAC_SetWaveHandler,
     "Sets the frequency and amplitude of the triangle wave emitted from the DAC. Setting 0 for either parameter will disable DAC."},

    {"button", "[button]", CLI_ButtonPressHandler,
     "Imitate a button press above the hardware level. No interrupt triggered."},

    {"battp", "", CLI_ReadBatteryPercentageHandler,
     "Read Battery Percentage"},

    {"battsim", "[on/off]", CLI_ControlBatterySimulation,
     "Enable/Disable Battery Percentage Simulation."},

    {"cfg", "[option][value]", CLI_CfgHandler,
     "Get/Set/Save/Restore system configuration settings. ? for options"},

    {"duty", "[percent]", CLI_DutyCycleHandler,
     "Sets the duty cycle of the PWM that goes to the motor. 0.0% - 100.0%"},

    {"edir", "", CLI_EncoderDirectionHandler,
     "Displays the current direction of the encoder."},

    {"endinf", "hy/ig", CLI_SimulateEndOfInfusionHandler,
     "Simulate end of infusion"},

    {"format", "", CLI_FsFormat,
     "Format NOR flash with FAT filesystem"},

    {"fscd", "[file name]", CLI_FileChangeDir,
     "Change directories within file system"},

    {"fsdel", "[file name]", CLI_FileDelete,
     "Delete mentioned file from file system"},

    {"fsdir", "[path]", CLI_FsDir,
     "List of files names from root directory"},

    {"fstest", "", CLI_FsTest,
     "Write chunk of data and read it back to check data integrity."},

    {"fsquery", "", CLI_FsQuery,
     "Returns the number of open files in the system"},

    {"fsread", "[file name]", CLI_FsRead,
     "Read File and ability to output file content to the CLI"},
     
    {"fwupdate", "", CLI_FwUpdate,
     "Retrieves firmware image from file system and prepares system for upgrade."},

    {"gpiod", "[on/off]", CLI_Toggle_GPIO_Debug,
     "Enable/Disable debug prints of GPIO driver."},

    {"gtime", "", CLI_GetDateTime,
     "Get Date Time From RTC"},

    {"hben", "t|f", CLI_HB_EnableHandler,
     "Enables or disables the H Bridge."},

    {"hdir", "[cw/ccw]", CLI_HB_DirHandler,
     "Displays the current direction of the H Bridge or sets the direction of the H Bridge."},

    {"hptest", "", CLI_HptTestHandler,
     "Test 100 uS spin loop, display actual time"},

    {"hptime", "", CLI_GetHPTimeHandler,
     "Get high precision timer time raw and formatted"},

    {"infmin", "sec/min", CLI_SetMinTimerHandler,
     "Changes how quickly an infusion runs. Every min will last the number of seconds input. The on screen time must change before the new time will take affect."},

    {"infstate", "", CLI_infstateHandler,
     "Display current state of Infusion Manager"},

    {"infevt", "event", CLI_InfEventHandler,
     "Send message to Infusion Manager task"},

#if defined( FDEBUG )
    {"langc", "[language #]", CLI_CreateLangFile,
     "Create language file and directory"},
#else
    {"langc", "", CLI_CreateLangFile,
     "Create language file and directory"},
#endif

    {"led", "number on/off", CLI_Led_Cmd_Handler,
     "Turn on/off an LED."},

    {"lcdclear", "", CLI_LCD_Clear_Cmd_Handler,
     "Clear the LCD of all pixels."},

    {"ledb", "number [brightness]", CLI_LedBrightnessHandler,
     "Get/Set LED brightness"},

    {"lcden", "[t|f]", CLI_EnableScreen_Cmd_Handler,
     "Enable/Disable the LCD screen or check the status of the LCD enable pin."},

    {"lcdline", "line# Data", CLI_LCD_Line_Cmd_Handler,
     "Input 8 bit data in decimal format to repeat across line # of LCD. Value must be between 0 and 255"},

    {"maxw", "reg value", CLI_MAX1454WriteMessageHandler,
     "Write Internal Configuration register of MAX1454"},

    {"maxwpg", "page", CLI_MAX1454WriteFlashPageHandler,
     "Write MAX1454 Flash memory"},

    {"mtrflow", "flow", CLI_MTR_FlowHandler,
     "Set a commanded flow rate. Data will be captured for the first .5 seconds of ramp up."},

    {"mtrflt", "", CLI_MTR_FaultHandler,
     "Displays the status of the motor fault pin."},

    {"mtrsd", "", CLI_MTR_ShutDownHandler,
     "Shutdown motor manager, related drivers and algorithms"},
// lint !e553 This variable define in the IAR option->c/c++->preprocessor setting,so ignore it.
#if defined( FDEBUG )
    {"mtrlog", "", CLI_MTR_DeltaLogHandler,
     "Print usec deltas"},
#endif

    {"mtrtime", "[t|f]", CLI_MTR_Timing,
     "Enables (t) or disables (f) GPIO toggle showing the motor loop frequency "},

    {"octave", "number", CLI_OctaveHandler,
     "Performs the Power On, Alert, Alarm, Button Tone, and Power Off sequence in the specified octave."},

    {"occ", "", CLI_Occ,
     "Occ command set. Enter \"occ\" for command set."},

    {"pos", "[number]", CLI_EncoderPosGetHandler,
     "Displays the current position of the encoder or sets the value of the encoder position variable"},

    {"progc", "", CLI_CreateProgramHandler,
     "Create Custom or HyQvia Template program"},

    {"progr", "", CLI_ReadProgramHandler,
     "Read Progam"},

    {"rpm", "", CLI_RetrieveRPMhandler,
     "Reads RPMs of the motor."},

    {"rtc", "", CLI_RTChandler,
     "Directly Reads the RTC."},

    {"simail", "", CLI_SimulateAirInLineDetection,
     "Simulate Air In Line Detection Event"},

    {"simbatt", "percentage", CLI_SimulateBattStateHandler,
     "Simulate Battery State"},

     {"simfail", "", CLI_SimulateFailSafeHandler,
      "Simulate Fail Safe Error" },

    {"simline", "[lineset] [door]", CLI_SimulateHallSensorStatus,
     "Simulate Lineset and Door Status"},

    {"simocc", "", CLI_SimulateOcclusionDetection,
     "Simulate Occlusion Detection Event"},

    {"screen", "[screen ID]", CLI_GUI_DispScreen_Cmd_Handler,
     "Display screen with provided screen ID. If the screen ID is all, then every screen will be displayed for 1 second."},

    {"stime", "[mm] [dd] [yyyy] [hh] [mm] [ss]", CLI_SetDateTime,
     "Set Date Time in RTC"},

    {"show", "[text]", CLI_GUI_DispString_Cmd_Handler,
     "Write text on LCD display"},

    {"sm", "[on/off]", CLI_Toggle_SM_Debug,
     "Enable/Disable debug prints for switch manger."},

#if defined(FDEBUG)
    {"smbuff", "", CLI_SmOverFlow_Count,
     "Get the number of times the serial manager buffered overflowed"},
#endif

    {"swtoana", "", CLI_SwitchToAnalog,
     "Switch MAX1454 to fixed analog mode"},

    {"tasks", "", CLI_TaskQuery,
     "Display task info"},

    {"tdc", "", CLI_GetTDCDataHandler,
     "Display TDC details in voltage, encoder position, and TDC state"},

    {"tdcmove", "", CLI_MoveToTDC_Handler,
     "Move the motor shaft to the TDC position"},

    {"temp", "", CLI_ReadTemperatureCmdHandler,
     "Read Fuel Guage Temperature in celsius"},

    {"usb", "[on/off]", CLI_ControlUSBInterface,
     "Enable/Disable USB interface"},

    { "ver", "", CLI_ver_Cmd_Handler,
      "Display firmware part number and version info" },      // PN and version info

    {"volt", "", CLI_ReadCellVoltageCmdHandler,
     "Read Battery Cell Voltage"},

    { "?", "", CLI_help_Cmd_Handler,                                    ///Displays list of Commands available
      "Display list of commands" },

    { NULL,     NULL,             NULL,               NULL }
};


////////////////////////////////////////////////////////////////////////////////
// Static Variables
////////////////////////////////////////////////////////////////////////////////

static char s_OutgoingMsg[SERIAL_MAX][MAX_SERIAL_MSG];    ///Buffer to hold text for responses to command messages.
static char s_IncomingMsg[SERIAL_MAX][MAX_SERIAL_MSG];    ///CLI internal buffer after grabbing command message from Serial Manager.

///////////////////////////////////////////////////////////////////////////////
// Function Prototypes
///////////////////////////////////////////////////////////////////////////////

///Takes incoming message and determines what command function it is associated with.
static void CLI_ProcessCommand( SERIAL_CMD_SOURCE_T cmdSource_ );

///Breaks incoming messages into the command and parameters.
///Parm1: In - pointer to current message to be parsed.
static uint8_t CLI_ParseCommand( char *msg_ );

static ERROR_T CLI_PostStart( void );

static void CLI_PostWdgMsg( void );

typedef struct
{
    uint32_t Source;
}CLI_MSG_T;

// CLI  Manager Queue
static OS_EVENT *s_CliQueue;
// CLI  Manager Queue
static void *s_CliQMsgs[ CLI_Q_SIZE ];
// CLI Pool
static CLI_MSG_T *s_CliMsgPool[ CLI_Q_SIZE ];

static TF_DESC_T s_CliTaskDesc =
{
    .NAME = "CLI_Task",      // task name
    .ptos = &s_CLITaskStk[ CLI_TASK_STK_SIZE - 1],      // Stack top
    .prio = (int)CLI_TASK_PRIO,       // Task priority
    .pbos = s_CLITaskStk,      // Stack bottom
    .stk_size = CLI_TASK_STK_SIZE,   // Stack size
    .taskId = CLI,
    .queue = &s_CliQueue,
    .queueBuf = s_CliQMsgs,
    .msgPool = (uint8_t *)s_CliMsgPool,
    .msgCount = (uint16_t)CLI_Q_SIZE,
    .msgSize = (uint16_t)sizeof(CLI_MSG_T),
    .PreStart = CLI_Initialize,
    .PostStart = CLI_PostStart,
    .MsgHandler = CLI_Task,
    .TimeoutHandler = CLI_PostWdgMsg,
    .msgTimeoutMS = CLI_TIMEOUT_MS,
    .MemPtr = NULL,
    .maxQdepth = 0,
    .taskRunning = FALSE,
    .WdgId = WD_CLI,
};

////////////////////////////////////////////////////////////////////////////////
static void mainCLI_Trace ( ERROR_T errMsg_, int line_ )
{
    if( errMsg_ != ERR_NONE)
    {
        SAFE_ReportError( errMsg_, __FILENAME__, line_ ); //lint !e613 Filename is not NULL ptr
    }
}

////////////////////////////////////////////////////////////////////////////////
static void CLI_PostWdgMsg( void )
{
    (void)CLI_CommandNotify( SERIAL_WDOG );
}

////////////////////////////////////////////////////////////////////////////////
static ERROR_T CLI_PostStart ( void )
{
    return SM_SendF( "\r\n" );
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T CLI_Initialize ( void )
{
    (void)memset( s_IncomingMsg, '\0', sizeof(s_IncomingMsg) );
    (void)memset( s_OutgoingMsg, '\0', sizeof(s_OutgoingMsg) );

    uint8_t i = 0;
    uint8_t l;
    while( COMMANDS[i].CMD != NULL )
    {
        if( ( l = (uint8_t)strlen( COMMANDS[i].CMD ) ) > g_LongestCmdLength )
        {
            g_LongestCmdLength = l;
        }

        if( ( l = (uint8_t)strlen( COMMANDS[i].SYNTAX ) ) > g_LongestSyntaxLength )
        {
            g_LongestSyntaxLength = l;
        }

        i++;

    }


    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
void CLI_Task ( void *arg_ )
{
    if ( arg_ == NULL )
    {
        mainCLI_Trace( ERR_PARAMETER, __LINE__ );
        return;
    }

    CLI_MSG_T *message = (CLI_MSG_T *) arg_;

    if( message->Source != SERIAL_WDOG )
    {
        ///Do not care about data received arg_
        CLI_ProcessCommand( (SERIAL_CMD_SOURCE_T)message->Source );
    }

    // Do nothing with watchdog message
}

////////////////////////////////////////////////////////////////////////////////
static void CLI_ProcessCommand ( SERIAL_CMD_SOURCE_T cmdSource_ )
{
    ERROR_T errorReturn; ///Used to capture error reported from Serial Manager.
    uint8_t i;
    bool knownCmd = false;
    static char lastCmd[SERIAL_MAX][COMMAND_LINE_LENGTH_MAX] = {0};
    
    s_Source = cmdSource_;

    if( cmdSource_ >= SERIAL_MAX )
    {
        return;
    }

    memset( s_IncomingMsg, '\0', sizeof( s_IncomingMsg ) );
    memset( s_OutgoingMsg, '\0', sizeof( s_OutgoingMsg ) );

    errorReturn = SM_RetrieveMessage( cmdSource_, &s_IncomingMsg[cmdSource_][0] );
    mainCLI_Trace( errorReturn, __LINE__ );
    

    // Use last command entered if user just hits CRLF
    if( !strcmp( s_IncomingMsg[cmdSource_], "" ) )
    {
        strcpy( &s_IncomingMsg[cmdSource_][0], &lastCmd[cmdSource_][0] );
        errorReturn = SM_SendF( &s_IncomingMsg[cmdSource_][0] );
        mainCLI_Trace( errorReturn, __LINE__ );
        
        errorReturn = SM_SendF( "\r\n" );
        mainCLI_Trace( errorReturn, __LINE__ );
        
    }
    else
    {
        memset( &lastCmd[cmdSource_][0], '\0', sizeof( COMMAND_LINE_LENGTH_MAX ) );
        strncpy( &lastCmd[cmdSource_][0], &s_IncomingMsg[cmdSource_][0], COMMAND_LINE_LENGTH_MAX );
        
        errorReturn = SM_SendF( "\r\n" );
        mainCLI_Trace( errorReturn, __LINE__ );
    }


    /// Split message into command and parameters.
    /// Command and params are written to g_ParsedMsg.
    /// Command is always first string in first array
    /// parmCount will always be minimum of 1, command is first parm

    g_ParmCount = CLI_ParseCommand( &s_IncomingMsg[cmdSource_][0] );
    g_ParmCount = g_ParmCount;              

    /// Process command unless it is for help in which case we just call each command handler
    /// to print it's own help information
    i = 0;
    while( COMMANDS[i].CMD != NULL )
    {
        if( !strcmp( g_ParsedMsg[0], COMMANDS[i].CMD ) )
        {
            COMMANDS[i].Handler( cmdSource_ );

            knownCmd = true;
        }

        i++;
    }

    if( !knownCmd )
    {
        strncpy( &s_OutgoingMsg[cmdSource_][0], "Invalid Command\r\n", 17 );

        errorReturn = SM_SendF( "%s", &s_OutgoingMsg[cmdSource_][0] );
        mainCLI_Trace( errorReturn, __LINE__ );
    }
}

///////////////////////////////////////////////////////////////////////////////
/// Parse command line into command and parameters.  Strings left in g_ParsedMsg array
/// of strings.
/// Returns number of parameters found.
static uint8_t CLI_ParseCommand ( char *msg_ )
{
    char *c;
    uint8_t parmIdx, charIdx;
    uint8_t parmCount = 1;  ///lint !e578
    bool lastCharIsWhiteSpace = false;

    if( msg_ == NULL )
    {
        mainCLI_Trace( ERR_PARAMETER, __LINE__ );
        return 0;
    }

    memset( g_ParsedMsg, 0, sizeof( g_ParsedMsg ) );

    c = msg_;
    parmIdx = 0;
    charIdx   = 0;

    while( *c != 0x00 )
    {
        if( isspace( (int) *c ) )
        {
            /// Skip over multiple whitespace chars
            if( !lastCharIsWhiteSpace )
            {
                parmIdx++;
                charIdx = 0;

                /// Don't increment parameter count if final chars whitespace only
                if( ( *(c + 1) != 0x00 ) && ( !lastCharIsWhiteSpace ) )  //lint !e774
                {

                    parmCount++;
                }
                lastCharIsWhiteSpace = true;
            }
        }
        else
        {
            g_ParsedMsg[parmIdx][charIdx] = *c;
            charIdx++;
            lastCharIsWhiteSpace = false;
        }

        c++;
    }

    return parmCount;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T CLI_CommandNotify ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    CLI_MSG_T data;

    if( source_ >= SERIAL_MAX )
    {
        return ERR_PARAMETER;
    }

    data.Source = (uint32_t)source_;
    status = TF_PostMsg( &s_CliTaskDesc, (void *)&data );
    return status;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T CLI_GetTaskControlBlock ( TF_DESC_T * *pDesc_ )
{
    if( pDesc_ == NULL )
    {
        return ERR_PARAMETER;
    }

    *pDesc_ = &s_CliTaskDesc;
    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
SERIAL_CMD_SOURCE_T CLI_GetSource( void )
{
    return s_Source;
}

#endif // _CLI_C_
