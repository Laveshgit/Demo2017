//////////////////////////////////////////////////////////////////////////////
///
///                Copyright(c) 2016 FlexMedical
///
///                   *** Confidential Company Proprietary ***
///
/// \file CLI_Commands.c
///
/// \brief CLI Commands Functions
///
/// This file carries the function implementations supporting the
/// Command Line Interface software unit.
///
///
///////////////////////////////////////////////////////////////////////////////

#include <fs_entry.h>
#include <fs_dir.h>
#include <stdlib.h>
#include <string.h>

// RTOS
#include "cpu.h"
#include <ucos_ii.h>
#include <clk.h>
#include  <fs_file.h>
#include  <fs_api.h>

// User
#include "Adc.h"
#include "AlarmAlertManager.h"
#include "AilDriver.h"
#include "AirInLineManager.h"
#include "AudioManager.h"
#include "BatteryMonitor.h"
#include "Common.h"
#include "CommonTypes.h"
#include "CLI.h"
#include "CLI_Commands.h"
#include "ConfigurationManager.h"
#include "DAC.h"
#include "EncoderDriver.h"
#include "Errors.h"
#include "GUI.h"
#include "FileSystemManager.h"
#include "InternalFlashManager.h"
#include "InfusionManagerFunctions.h"
#include "HallManager.h"
#include "HBridgeDriver.h"
#include "HighPrecisionTimer.h"
#include "InfusionManager.h"
#include "LanguageManager.h"
#include "LEDManager.h"
#include "MotorManager.h"
#include "MAX1454.h"
#include "OcclusionManager.h"
#include "ProgramManager.h"
#include "PwmDriver.h"
#include "RTC.h"
#include "SafetyMonitor.h"
#include "SerialManager.h"
#include "SpiDriver.h"
#include "SwitchManager.h"
#include "TaskMonitor.h"
#include "UIManager.h"
#include "UIM_ScreenFunctions.h"
#include "USBManager.h"
#include "Version.h"
#include "WatchdogManager.h"

#include <time.h>

////////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND #DEFINES
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ENUMS, TYPEDEFS, AND STRUCTURES
////////////////////////////////////////////////////////////////////////////////
//Structure which will convert ASCII value into screen exist enum value
typedef struct
{
    const char *NAME;
    SCREEN_ID_T Value;
}SCR_ATTR_VALUE_T;

///////////////////////////////////////////////////////////////////////////////
// Local Variables
///////////////////////////////////////////////////////////////////////////////
static const char* const S_ADC_INPUT_STRINGS[ ADC_CHANNEL_COUNT ] =
{
    "PDET0",
    "PDET1",
    "DOOR ",
    "IMOT ",
#ifndef PROTO_BOARD
    "MOTPOS",
    "MOT_PWR",
    "3V",
    "5V"
#endif
};

static SCR_ATTR_VALUE_T s_CliScreenTable[MAX_SCREENS] =
{
    { "N/A",    SCREEN_BLANK   },              //BLANK_SCREEN
    { "0",      SCREEN_0       },              //POWER_OFF_S,
    { "0-1",    SCREEN_0_1     },              //PAUSE_BEFORE_POWER_S,
    { "1",      SCREEN_1       },              //SHUT_DOWN_CONN_CHARGE
    { "2",      SCREEN_2       },              //LOW_BATT_CONN_CHARGE
    { "3",      SCREEN_3       },              //LOGO
    { "4",      SCREEN_4       },              //LOW_BATT_MINS_REMAIN
    { "5",      SCREEN_5       },              //REPLACE_BATT 
    { "5-i",    SCREEN_5_I     },              //REPLACE_BATT_INFO_S
    { "6",      SCREEN_6       },              //INSERT_INFUSION_SET
    { "6-1",    SCREEN_6_1     },              //ATTACH_HY
    { "6-2",    SCREEN_6_2     },              //NOTE_SINGLE_NEEDLE_DETECTED
    { "6-3",    SCREEN_6_3     },              //NOTE_BI_NEEDLE_DETECTED
    { "7",      SCREEN_7       },              //NO_SET
    { "7-1i",   SCREEN_7_1I    },              //NO_SET_INFO
    { "9-1",    SCREEN_9       },              //START_SETUP
    { "9-1i",   SCREEN_9_1I    },              //START_SETUP_INFO1_EXIT
    { "9-2i",   SCREEN_9_2I    },              //START_SETUP_INFO2_EXIT
    { "9-1io",  O_SCREEN_9_1I  },              //START_SETUP_INFO1_MORE
    { "9-2io",  O_SCREEN_9_2I  },              //START_SETUP_INFO2_MORE
    { "9-3",    SCREEN_9_3     },              //NO_ACTIVE_PROGRAM
    { "12",     SCREEN_12      },              //PROGRAM_INFO1
    { "13",     SCREEN_13      },              //PROGRAM_INFO2
    { "14-1",   SCREEN_14_1    },              //REVIEW_EDIT_STEP_INFO1
    { "14-2",   SCREEN_14_2    },              //REVIEW_EDIT_STEP_INFO2
	{ "14-3",   SCREEN_14_3    },              //REVIEW_EDIT_PROGRAM_INFO3	
    { "14-4",   SCREEN_14_4    },              //REVIEW_EDIT_PROGRAM_INFO
    { "15",     SCREEN_15      },              //PRIMING1
    { "15-i",   SCREEN_15_I    },              //PRIMING1_INFO
    { "15-1",   SCREEN_15_1    },              //PRIME_ERROR_S
    { "15-1i",  SCREEN_15_1I   },              //PRIME_ERROR_INFO_S
    { "16",     SCREEN_16      },              //PRIMING2
    { "16p",    P_SCREEN_16    },              //plural PRIMING2
    { "16-i",   SCREEN_16_I    },              //PRIMING2_INFO
    { "16-ip",  P_SCREEN_16_I  },              //plural PRIMING2_INFO
    { "17",     SCREEN_17,     },              //FILLING
    { "18",     SCREEN_18      },              //INSERT_NEEDLE,
    { "18p",    P_SCREEN_18    },              //plural INSERT_NEEDLE
    { "19",     SCREEN_19      },              //START_PULL_BACK,
    { "19-1i",  SCREEN_19_I    },              //NEEDLE_CHECK_INFO1,
    { "19-2i",  SCREEN_19_2I   },              //NEEDLE_CHECK_INFO2,
    { "19-3i",  SCREEN_19_3I   },              //NEEDLE_CHECK_INFO3,
    { "20",     SCREEN_20      },              //PULLING_BACK,
    { "21-1",   SCREEN_21      },              //BLOOD_IN_TUBING,
    { "22",     SCREEN_22      },              //REMOVE_NEEDLE_BLOOD,
    { "22p",    P_SCREEN_22    },              //plural REMOVE_NEEDLE_BLOOD
    { "22-1",   SCREEN_22_1    },              //REPLACE_NEEDLE,
    { "23",     SCREEN_23      },              //START_HY,
    { "23p",    P_SCREEN_23    },              //plural START_HY,
    { "24-1",   SCREEN_24      },              //HY_COMPLETE
    { "25",     SCREEN_25      },              //REMOVE_SYRINGE,
    { "28",     SCREEN_28      },              //RESUME_INF
    { "29-1",   SCREEN_29_1    },              //INF_PROG
    { "29-2",   SCREEN_29_2    },              //IG_INF_PROG
    { "29-3",   SCREEN_29_3    },              //PRESS_PAUSE_PROMPT
    { "29-1i",  SCREEN_29_1I   },              //PROG_INFO1,
    { "29-2i",  SCREEN_29_2I   },              //PROG_INFO2,
    { "29-3i",  SCREEN_29_3I   },              //PROG_INFO3,
    { "29-4i",  SCREEN_29_4I   },              //PROG_BRIGHTNESS,
    { "29-5i",  SCREEN_29_5I   },              //PROG_SOUND_VOL,
    { "30",     SCREEN_30      },              //PROG_PAUSE
    { "30-1i",  SCREEN_30_1I   },              //PROG_PAUSE_INFO1,
    { "30-2i",  SCREEN_30_2I   },              //PROG_PAUSE_INFO2,
    { "30-3i",  SCREEN_30_3I   },              //PROG_PAUSE_INFO3,
    { "30-4i",  SCREEN_30_4I   },              //PROG_PAUSE_BRIGHT,    
    { "30-5i",  SCREEN_30_5I   },              //PROG_PAUSE_SOUND,
    { "31-1",   SCREEN_31_1    },              //STEP_ENDING_S
    { "32",     SCREEN_32      },              //AIL
    { "32-1i",  SCREEN_32_1I   },              //AIL_INFO
    { "32-1",   SCREEN_32_6    },              //AIL_ACTION         //This goes to screen 32_6 for review purposes. So reviewer can see all widgets.
    { "32-2",   SCREEN_32_2    },              //CLEARING_AIR
    { "32-3",   SCREEN_32_3    },              //HY_RESUME_INF
    { "32-4",   SCREEN_32_4    },              //IG_RESUME_INF 
    { "32-5",   SCREEN_32_5    },              //AIL_DISCONNECT_NEEDLE
    { "32-6",   SCREEN_32_6    },              //AIL_ACTION_DONE
    { "32-7",   SCREEN_32_7    },              //AIL_RECONNECT_NEEDLE
    { "33",     SCREEN_33      },              //OCC_S
    { "33-1",   SCREEN_33_1    },              //OCC_CLEAR_S
    { "33-1i",  SCREEN_33_1I   },              //OCC_INFO
    { "34",     SCREEN_34      },              //NEAR_END_INF
    { "35-1",   SCREEN_35      },              //IG_COMPLETE
    { "36-1",   SCREEN_36_1    },              //CONNECT_FLUSH
    { "37",     SCREEN_37      },              //START_FLUSH
    { "37-1",   SCREEN_37_1    },              //FLUSHING
    { "37-2",   SCREEN_37_2    },              //FLUSHING_PAUSE
    { "37-3",   SCREEN_37_3    },              //FLUSH_COMPLETE
    { "38",     SCREEN_38      },              //REMOVE_NEEDLE_FLUSH
    { "38p",    P_SCREEN_38    },              //plural REMOVE_NEEDLE_FLUSH
    { "39-1",   SCREEN_39      },              //SETTINGS_MENU
    { "39-5",   SCREEN_39_5    },              //INDICATION_MENU
    { "39-5i",  SCREEN_39_5I  },               //INDICATION_PI_INFO_MORE    
    { "39-5io", O_SCREEN_39_5I},               //INDICATION_PI_INFO_EXIT
    { "39-6i",  SCREEN_39_6I  },               //INDICATION_CIDP_INFO_MORE
    { "39-6io", O_SCREEN_39_6I},               //INDICATION_CIDP_INFO_EXIT
    { "39-8",   SCREEN_39_8    },              //LANGUAGE_MENU
    { "40",     SCREEN_40      },              //ADJUST_BRIGHTNESS
    { "41",     SCREEN_41      },              //ADJUST_VOLUME
    //TODO:This Macro is added only for resolved Lint issue for manage screen attribute menu.
//     Remove it after maintain Screen enum.
    { "43-1",   SCREEN_43_1    },              //Exit Authorized Setup
    { "44",     SCREEN_44      },              //Setup Menu Password
    { "44-1",   SCREEN_44_1    },              //Incorrect  password
    { "46-1",   SCREEN_46      },              //PROGRAM_TYPE
    { "46-1i",  SCREEN_46_1I   },              //CUSTOM_INFO_EXIT
    { "46-2i",  SCREEN_46_2I   },              //HYQVIA_INFO_EXIT
    { "46-1io", O_SCREEN_46_1I },              //CUSTOM_INFO_MORE
    { "46-2io", O_SCREEN_46_2I },              //HYQVIA_INFO_MORE
    { "47",     SCREEN_47      },              //EDIT_DOSAGE_PROGRAM
    { "48",     SCREEN_48      },              //EDIT_HY_FLOW_RATE
    { "49",     SCREEN_49      },              //EDIT_IG_STEPS
    { "50",     SCREEN_50      },              //EDIT_IG_STEPS_FLOW_RATE
    { "51",     SCREEN_51      },              //EDIT_IG_STEPS_DURATION
    { "57-1",   SCREEN_57      },              //Authorization
    { "59-7",   SCREEN_59_7    },              //Program available
    { "60-1",   SCREEN_60_1    },              //Allow Rate Change
    { "60-1i",  SCREEN_60_1I   },              //ALLOW_RATE_CHANGES_INFO1_MORE,
    { "60-2i",  SCREEN_60_2I   },              //ALLOW_RATE_CHANGES_INFO2_MORE, 
    { "60-1io", O_SCREEN_60_1I },              //ALLOW_RATE_CHANGES_INFO1_EXIT, 
    { "60-2io", O_SCREEN_60_2I },              //ALLOW_RATE_CHANGES_INFO2_EXIT, 
    { "66-1",   SCREEN_66_1    },              //Needle Check
    { "66-1i",  SCREEN_66_1I   },              //NEEDLE_CHECK_MORE_ON_INFO1
    { "66-2i",  SCREEN_66_2I   },              //NEEDLE_CHECK_EXIT_OFF_INFO2
    { "66-1io", O_SCREEN_66_1I },              //NEEDLE_CHECK_EXIT_ON_INFO1
    { "66-2io", O_SCREEN_66_2I },              //NEEDLE_CHECK_MORE_OFF_INFO2
    { "67-1",   SCREEN_67_1    },              //FLUSH
    { "67-1i",  SCREEN_67_1I   },              //FLUSH_INFO1_MORE
    { "67-2i",  SCREEN_67_2I   },              //FLUSH_INFO2_EXIT
    { "67-1io", O_SCREEN_67_1I },              //FLUSH_INFO1_EXIT
    { "67-2io", O_SCREEN_67_2I },              //FLUSH_INFO2_MORE 
    { "68",     SCREEN_68      },              //EDIT_PATIENT_WEIGHT
    { "68-2",   SCREEN_68_2    },              //NEEDLE_SET
    { "69",     SCREEN_69      },              //HYQVIA_IG_TIME
    { "70",     SCREEN_70      },              //PUMP_ERROR,
    { "70-1i",  SCREEN_70_1I,  },              //PUMP_ERROR_INFO,
    { "71",     SCREEN_71      },              //BIFURCATED_NOTE_HY_CUSTOM,   //BIFURCATED_NOTE_IG //BIFURCATED_NOTE_IG_CUSTOM //BIFURCATED_NOTE_HY
    { "73",     SCREEN_73      },              //PROGRAM_ERROR 
    { "73-1",   SCREEN_73_1    },              //POWER_LOSS_RECOVERY
    { "74-1",   SCREEN_74_1    },              //INFUSION_RAMP_UP
    { "77",     SCREEN_77      },              //BLACK_SCREEN
    { "78",     SCREEN_78      },              //WHITE_SCREEN
    { "100",    SCREEN_100     },              //USB_ENABLE
    { "102",    SCREEN_102     },              //FONT_TEST
    { "103",    SCREEN_103     },              //LOADING_LANGUAGE
};

static AM_MESSAGE_T s_AmMessage;

///////////////////////////////////////////////////////////////////////////////
// Prototypes
///////////////////////////////////////////////////////////////////////////////

static void CLI_ShowAllScreenCommands( SERIAL_CMD_SOURCE_T source_ );
static ERROR_T CLI_ScreenLookUp( const char *name_, SCREEN_ID_T *screenId_ );
static void CLI_Trace( SERIAL_CMD_SOURCE_T source_, ERROR_T errMsg_, int line_ );
static void CLI_ShowAilCmds( SERIAL_CMD_SOURCE_T source_ );
static void CLI_ShowOccCmds( SERIAL_CMD_SOURCE_T source_ );
static void CLI_HandleTwoAilArgs( SERIAL_CMD_SOURCE_T source_ );
static void CLI_HandleThreeAilArgs( SERIAL_CMD_SOURCE_T source_ );
static void CLI_HandleFourAilArgs( SERIAL_CMD_SOURCE_T source_ );
static void CLI_HandleFiveAilArgs( SERIAL_CMD_SOURCE_T source_ );
static void CLI_HandleTwoOccArgs( SERIAL_CMD_SOURCE_T source_ );
static void CLI_HandleThreeOccArgs( SERIAL_CMD_SOURCE_T source_ );
static void CLI_HandleFiveOccArgs( SERIAL_CMD_SOURCE_T source_ );
static void CLI_HandleSixOccArgs( SERIAL_CMD_SOURCE_T source_ );

///////////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////////

static void CLI_HandleFiveAilArgs( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    INF_STATE_T state;
    AILM_MSG_T msg;

    ( void )memset( ( void * )&msg, 0, sizeof(msg) );

    if( strcmp( g_ParsedMsg[1], "start" ) == 0 )
    {
        status = INF_GetState( &state );
        CLI_Trace( source_, status, __LINE__ );

        if( state == INF_STATE_INFUSION_DELIVERY )
        {
            ( void ) SM_SendF( "Not supported during infusion\r\n" );
        }
        else
        {
            // Start, Fluid-Air, Flowrate, Sample Limit,
            msg.Id = AILM_START;
            msg.Data.Cfg.Type = (AIL_SAMPLE_T)atoi( g_ParsedMsg[2] );
            msg.Data.Cfg.FlowRate = (float)atof( g_ParsedMsg[3] );
            msg.Data.Cfg.SampleLimitML = (uint8_t)atoi( g_ParsedMsg[4] );

            if( AILM_ValidateCfg( &msg.Data.Cfg ) == ERR_NONE )
            {
                status = AILM_Notify( &msg );
                CLI_Trace( source_, status, __LINE__ );
            }
            else
            {
                ( void ) SM_SendF( "Invalid Parameter\r\n" );
            }
        }
    }
    else if( strcmp( g_ParsedMsg[1], "write" ) == 0 )
    {
        AIL_WRITE_T cfg;
        cfg.Id = (AIL_SENSOR_T) atoi( g_ParsedMsg[2] );
        cfg.OutType = (AIL_OUT_T) atoi( g_ParsedMsg[3] );
        cfg.Threshold = (uint8_t) atoi( g_ParsedMsg[4] );

        status = AIL_SendCfg( &cfg );

        if( status == ERR_NONE )
        {
            ( void ) SM_SendF( "Configuration Sent\r\n" );
        }
        else
        {
            ( void ) SM_SendF( "Invalid Parameter\r\n" );
        }
    }
    else
    {
        ( void ) SM_SendF( "Command not recognized\r\n" );
        CLI_ShowAilCmds( source_ );
    }
}

///////////////////////////////////////////////////////////////////////////////
static void CLI_HandleFourAilArgs( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;

    if( strcmp( g_ParsedMsg[1], "set" ) == 0 )
    {
        if( strcmp( g_ParsedMsg[2], "thresh" ) == 0 )
        {
            status = AILM_SetThresholdML( ( uint8_t )atoi( g_ParsedMsg[3] ) );

            if( status != ERR_NONE )
            {
                ( void ) SM_SendF( "Threshold out of range\r\n" );
            }
            else
            {
                ( void ) SM_SendF( "Threshold set\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[2], "win" ) == 0 )
        {
            status = AILM_SetSampleWinSeconds( ( uint32_t )atoi( g_ParsedMsg[3] ) );

            if( status != ERR_NONE )
            {
                ( void ) SM_SendF( "Window out of range\r\n" );
            }
            else
            {
                ( void ) SM_SendF( "Window set\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[2], "bubble" ) == 0 )
        {
            status = AIL_SetBubbleVolume( (float) atof( g_ParsedMsg[3] ) );

            if( status != ERR_NONE )
            {
                ( void ) SM_SendF( "Bubble size out of range\r\n" );
            }
            else
            {
                ( void ) SM_SendF( "Bubble size set\r\n" );
            }

        }
        else
        {
            ( void ) SM_SendF( "Command not recognized\r\n" );
            CLI_ShowAilCmds( source_ );
        }
    }
    else if( strcmp( g_ParsedMsg[2], "array" ) == 0 )
    {
        status = AILM_ArrayDump( (AIL_SENSOR_T)atoi( g_ParsedMsg[3] ) );
        CLI_Trace( source_, status, __LINE__ );
    }
    else if( strcmp( g_ParsedMsg[1], "disable" ) == 0 )
    {
        int id = atoi( g_ParsedMsg[2] );
        int disable = atoi( g_ParsedMsg[3] );

        if( id < (int)NUM_OF_AIL_SENSORS )
        {
            status = AILM_DisableSensor( (AIL_SENSOR_T)id, (disable != 0) );

            if( status == ERR_NONE )
            {
                if( disable != 0 )
                {
                    ( void ) SM_SendF( "Sensor Disabled\r\n" );
                }
                else
                {
                    ( void ) SM_SendF( "Sensor Enabled\r\n" );
                }
            }
            else
            {
                ( void ) SM_SendF( "Command Failed\r\n" );
            }
        }
        else
        {
            ( void ) SM_SendF( "Sensor ID out of range\r\n" );
        }
    }
    else
    {
        ( void ) SM_SendF( "Command not recognized\r\n" );
        CLI_ShowAilCmds( source_ );
    }
}

///////////////////////////////////////////////////////////////////////////////
static void CLI_HandleThreeAilArgs( SERIAL_CMD_SOURCE_T source_ )
{
    bool flag;
    ERROR_T status;
    AIL_READ_DATA_T data;

    if( strcmp( g_ParsedMsg[1], "get" ) == 0 )
    {
        if( strcmp( g_ParsedMsg[2], "thresh" ) == 0 )
        {
            uint16_t threshold;
            status = AILM_GetThresholdML( &threshold );

            if( status == ERR_NONE )
            {
                (void)SM_SendF( "Threshold: %i ml\r\n", threshold );
            }
            else
            {
                CLI_Trace( source_, status, __LINE__ );
            }

        }
        else if( strcmp( g_ParsedMsg[2], "win" ) == 0 )
        {
            uint32_t win;
            status = AILM_GetSampleWindow( &win );

            if( status == ERR_NONE )
            {
                (void)SM_SendF( "Sampling Window: %i seconds\r\n", win );
            }
            else
            {
                CLI_Trace( source_, status, __LINE__ );
            }

        }
        else if( strcmp( g_ParsedMsg[2], "vol" ) == 0 )
        {
            float vol;
            status = AILM_GetDetectedVolume( &vol );

            if( status == ERR_NONE )
            {
                (void)SM_SendF( "Volume: %f ml\r\n", vol );
            }
            else
            {
                CLI_Trace( source_, status, __LINE__ );
            }
        }
    }
    else if( strcmp( g_ParsedMsg[1], "poll" ) == 0 )
    {
        status = AIL_PollOuput( ( AIL_SENSOR_T )atoi( g_ParsedMsg[2] ), &flag );

        if( status == ERR_NONE )
        {
            ( void )SM_SendF( "%s\r\n", ( flag == AIL_AIR_DETECT ) ? "AIR" : "FLUID" ); //lint !e731 Bool comparison is intentional in case AIR_DETECT changes value during development
        }
        else
        {
            CLI_Trace( source_, status, __LINE__ );
        }
    }
    else if( strcmp( g_ParsedMsg[1], "read" ) == 0 )
    {
        int id = atoi( g_ParsedMsg[2] );

        if( id < (int)NUM_OF_AIL_SENSORS )
        {
            status = AIL_ReadSensorData( (AIL_SENSOR_T)id, &data );

            if( status != ERR_NONE )
            {
                ( void ) SM_SendF( "Command Failed\r\n" );
            }
            else
            {
                // Display the output in readable form
                (void)SM_SendF( "Error Flag: %i\r\n", data.ErrFlag );
                (void)SM_SendF( "CA bit: %i\r\n", data.CA );
                (void)SM_SendF( "AGC: %i\r\n", data.AGC );
                (void)SM_SendF( "LED: %i\r\n", data.I );
                (void)SM_SendF( "Reference: %i\r\n", data.Ref );
                (void)SM_SendF( "DS bit: %i\r\n", data.DS );
                (void)SM_SendF( "OP bit: %i\r\n", data.OP );
            }
        }
        else
        {
            ( void ) SM_SendF( "Sensor ID out of range\r\n" );
        }
    }
    else if( strcmp( g_ParsedMsg[1], "cal" ) == 0 )
    {
        int id = atoi( g_ParsedMsg[2] );

        if( id < (int)NUM_OF_AIL_SENSORS )
        {
            status = AIL_SendCal( (AIL_SENSOR_T)id );

            if( status != ERR_NONE )
            {
                ( void ) SM_SendF( "Command Failed\r\n" );
            }
        }
        else
        {
            ( void ) SM_SendF( "Sensor ID out of range\r\n" );
        }
    }
    else
    {
        ( void ) SM_SendF( "Command not recognized\r\n" );
        CLI_ShowAilCmds( source_ );
    }
}

///////////////////////////////////////////////////////////////////////////////
static void CLI_HandleTwoAilArgs( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    INF_STATE_T state;
    AILM_MSG_T msg;

    ( void )memset( ( void * )&msg, 0, sizeof(msg) );

    if( strcmp( g_ParsedMsg[1], "stop" ) == 0 )
    {
        status = INF_GetState( &state );
        CLI_Trace( source_, status, __LINE__ );

        if( state == INF_STATE_INFUSION_DELIVERY )
        {
            ( void ) SM_SendF( "Not supported during infusion\r\n" );
        }
        else
        {
            msg.Id = AILM_STOP;
            status = AILM_Notify( &msg );

            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "AIL stopped\r\n" );
            }
            else
            {
                ( void ) SM_SendF( "Command Failed\r\n" );
            }
        }
    }
    else if( strcmp( g_ParsedMsg[1], "pause" ) == 0 )
    {
        status = INF_GetState( &state );
        CLI_Trace( source_, status, __LINE__ );

        if( state == INF_STATE_INFUSION_DELIVERY )
        {
            ( void ) SM_SendF( "Not supported during infusion\r\n" );
        }
        else
        {
            msg.Id = AILM_PAUSE;
            status = AILM_Notify( &msg );

            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "AIL paused\r\n" );
            }
            else
            {
                ( void ) SM_SendF( "Command Failed\r\n" );
            }
        }
    }
    else if( strcmp( g_ParsedMsg[1], "clear" ) == 0 )
    {
        msg.Id = AILM_CLR_ARRAY;
        status = AILM_Notify( &msg );

        if( status == ERR_NONE )
        {
            ( void ) SM_SendF( "Array Cleared\r\n" );
        }
        else
        {
            ( void ) SM_SendF( "Command Failed\r\n" );
        }
    }
    else
    {
        ( void ) SM_SendF( "Command not recognized\r\n" );
        CLI_ShowAilCmds( source_ );
    }
}

///////////////////////////////////////////////////////////////////////////////
void CLI_Ail ( SERIAL_CMD_SOURCE_T source_ )
{
    switch( g_ParmCount )
    {
        case 1:
            CLI_ShowAilCmds( source_ );
            break;

        case 2:
            CLI_HandleTwoAilArgs( source_ );
            break;

        case 3:
            CLI_HandleThreeAilArgs( source_ );
            break;

        case 4:
            CLI_HandleFourAilArgs( source_ );
            break;

        case 5:
            CLI_HandleFiveAilArgs( source_ );
            break;

        default:
            ( void ) SM_SendF( "Command not recognized\r\n" );
            CLI_ShowAilCmds( source_ );
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////
static void CLI_HandleSixOccArgs( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    OCC_MSG_T msg;
    INF_STATE_T state;
    float threshold = 0;
    uint16_t flowrate = 0;
    LINESET_INFO_T needle;

    ( void )memset( ( void * )&msg, 0, sizeof(msg) );

    if( strcmp( g_ParsedMsg[1], "start" ) == 0 )
    {
        status = INF_GetState( &state );
        CLI_Trace( source_, status, __LINE__ );

        if( state == INF_STATE_INFUSION_DELIVERY )
        {
            ( void ) SM_SendF( "Not supported during infusion\r\n" );
        }
        else
        {
            status = MTR_SetFlow( ( float )atof( g_ParsedMsg[2] ) );
            CLI_Trace( source_, status, __LINE__ );

            // Start, Fluid-Air, Flowrate, Sample Limit,
            msg.Id = OCC_START;
            msg.Data.Cfg.Flowrate   = (uint16_t)atoi( g_ParsedMsg[2] );
            msg.Data.Cfg.Needle     = (LINESET_INFO_T)atoi( g_ParsedMsg[3] );
            msg.Data.Cfg.SamplingTime = (uint16_t)atoi( g_ParsedMsg[4] );
            msg.Data.Cfg.ThresholdFilterCnt = (uint8_t)atoi( g_ParsedMsg[5] );

            if( OCC_ValidatePrerequisite( &msg.Data.Cfg ) == ERR_NONE )
            {
                status = OCC_Notify( &msg );
                CLI_Trace( source_, status, __LINE__ );
            }
            else
            {
                ( void ) SM_SendF( "Invalid Parameter\r\n" );
            }
        }
    }
    else if( strcmp( g_ParsedMsg[1], "set" ) == 0 )
    {
        if( strcmp( g_ParsedMsg[2], "upstream" ) == 0 )
        {
            flowrate = (uint16_t)atoi( g_ParsedMsg[3] );
            needle = (LINESET_INFO_T)atoi( g_ParsedMsg[4] );
            threshold = ( float )atof( g_ParsedMsg[5] );

            status = OCC_SetThreshold( OCC_UPSTREAM, flowrate, needle, threshold );
            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "Threshold limit set successfully\r\n" );
            }
            else
            {
                ( void ) SM_SendF( "Failed to set threshold limit\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[2], "downstream" ) == 0 )
        {
            flowrate = (uint16_t)atoi( g_ParsedMsg[3] );
            needle = (LINESET_INFO_T)atoi( g_ParsedMsg[4] );
            threshold = ( float )atof( g_ParsedMsg[5] );
            status = OCC_SetThreshold( OCC_DOWNSTRAM, flowrate, needle, threshold );
            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "Threshold limit set successfully\r\n" );
            }
            else
            {
                ( void ) SM_SendF( "Failed to set threshold limit\r\n" );
            }
        }
        else
        {
            ( void ) SM_SendF( "Command not recognized\r\n" );
            CLI_ShowOccCmds( source_ );
        }
    }
    else
    {
        ( void ) SM_SendF( "Command not recognized\r\n" );
        CLI_ShowOccCmds( source_ );
    }
}

////////////////////////////////////////////////////////////////////////////////
static void CLI_HandleFiveOccArgs( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    float threshold = 0;
    uint16_t flowrate = 0;
    LINESET_INFO_T needle;

    if( strcmp( g_ParsedMsg[1], "get" ) == 0 )
    {
        if( strcmp( g_ParsedMsg[2], "upstream" ) == 0 )
        {
            flowrate = (uint16_t)atoi( g_ParsedMsg[3] );
            needle = (LINESET_INFO_T)atoi( g_ParsedMsg[4] );

            status = OCC_GetThreshold( OCC_UPSTREAM, flowrate, needle, &threshold );
            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "Threshold limit set as %0.2f PSI\r\n", threshold );
            }
            else
            {
                ( void ) SM_SendF( "Failed to get threshold limit\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[2], "downstream" ) == 0 )
        {
            flowrate = (uint16_t)atoi( g_ParsedMsg[3] );
            needle = (LINESET_INFO_T)atoi( g_ParsedMsg[4] );

            status = OCC_GetThreshold( OCC_DOWNSTRAM, flowrate, needle, &threshold );
            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "Threshold limit set as %0.2f PSI\r\n", threshold );
            }
            else
            {
                ( void ) SM_SendF( "Failed to get threshold limit\r\n" );
            }
        }
        else
        {
            ( void ) SM_SendF( "Command not recognized\r\n" );
            CLI_ShowOccCmds( source_ );
        }
    }
    else
    {
        ( void ) SM_SendF( "Command not recognized\r\n" );
        CLI_ShowOccCmds( source_ );
    }
}

////////////////////////////////////////////////////////////////////////////////
static void CLI_HandleTwoOccArgs( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    INF_STATE_T state;
    OCC_MSG_T msg;

    ( void )memset( ( void * )&msg, 0, sizeof(msg) );

    if( strcmp( g_ParsedMsg[1], "swtoana" ) == 0 )
    {
        MAX1454_MODE_T deviceMode;
        status = MAX1454_GetDeviceMode( &deviceMode );
        if( deviceMode == MAX1454_DIGITAL_MODE )
        {
            status = MAX1454_SwithchToFixedAnalog();
            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "MAX1454 now in Analog mode \r\n" );
            }
            else
            {
                ( void ) SM_SendF( "Failed to switch into Analog Mode \r\n" );
            }
        }
        else if( deviceMode == MAX1454_ANALOG_MODE )
        {
            ( void ) SM_SendF( "Device already in Analog mode\r\n" );
        }
        else
        {
            ( void ) SM_SendF( "MAX1454 is not properly initialized\r\n" );
        }
    }
    else if( strcmp( g_ParsedMsg[1], "stop" ) == 0 )
    {
        status = INF_GetState( &state );
        CLI_Trace( source_, status, __LINE__ );

        if( state == INF_STATE_INFUSION_DELIVERY )
        {
            ( void ) SM_SendF( "Not supported during infusion\r\n" );
        }
        else
        {
            status = MTR_ShutDown( true );
            CLI_Trace( source_, status, __LINE__ );

            msg.Id = OCC_STOP;
            status = OCC_Notify( &msg );

            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "OCC stopped\r\n" );
            }
            else
            {
                ( void ) SM_SendF( "Command Failed\r\n" );
            }
        }
    }
    else if( strcmp( g_ParsedMsg[1], "pause" ) == 0 )
    {
        status = INF_GetState( &state );
        CLI_Trace( source_, status, __LINE__ );

        if( state == INF_STATE_INFUSION_DELIVERY )
        {
            ( void ) SM_SendF( "Not supported during infusion\r\n" );
        }
        else
        {
            status = MTR_ShutDown( false );
            CLI_Trace( source_, status, __LINE__ );

            msg.Id = OCC_PAUSE;
            status = OCC_Notify( &msg );

            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "OCC paused\r\n" );
            }
            else
            {
                ( void ) SM_SendF( "Command Failed\r\n" );
            }
        }
    }
    else
    {
        ( void ) SM_SendF( "Command not recognized\r\n" );
        CLI_ShowOccCmds( source_ );
    }
}

////////////////////////////////////////////////////////////////////////////////
static void CLI_HandleThreeOccArgs( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    OCC_MSG_T msg;

    ( void )memset( ( void * )&msg, 0, sizeof(msg) );

    if( strcmp( g_ParsedMsg[1], "read" ) == 0 )
    {
        if( strcmp( g_ParsedMsg[2], "pressure" ) == 0 )
        {
            float volt, psi;
            status = OCC_GetPressureInfo( &volt, &psi );

            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "Pressure [%0.2f V] [%0.2f PSI]\r\n", volt, psi );
            }
            else
            {
                ( void ) SM_SendF( "Failed to read pressure\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[2], "calpara" ) == 0 )
        {
            float offset, sensitivity;
            status = OCC_GetCalibrationParameter( &offset, &sensitivity );
            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "Offset [%0.2f V] Sensitivity [%0.2f V/V/N]\r\n", offset, sensitivity );
            }
            else
            {
                ( void ) SM_SendF( "Failed to calibration parameter\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[2], "upstream" ) == 0 )
        {
            float buffer[NUM_OF_RATE_VALUES][LINESET_INFO_MAX];
            status = OCC_ReadAlgorithmThresholdFromFile( OCC_UPSTREAM, &buffer[0][0] );
            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "Upstream Threshold Limits\r\n" );
                for( RATE_VALUE_T i = ML_0; i < NUM_OF_RATE_VALUES; i++ )
                {
                    ( void ) SM_SendF( "Flowrate [%3d] Single [ %0.2f PSI ] Bifurcated [ %0.2f PSI ]\r\n",
                                      RATE_VALUE_ARR[i], buffer[i][SINGLE_NEEDLE_LINESET],
                                      buffer[i][BIFURCATED_NEEDLE_LINESET] );
                }
            }
            else
            {
                ( void ) SM_SendF( "Failed to read upstream threshold file\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[2], "downstream" ) == 0 )
        {
            float buffer[NUM_OF_RATE_VALUES][LINESET_INFO_MAX];
            status = OCC_ReadAlgorithmThresholdFromFile( OCC_DOWNSTRAM, &buffer[0][0] );
            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "Downstream Threshold Limits\r\n" );
                for( RATE_VALUE_T i = ML_0; i < NUM_OF_RATE_VALUES; i++ )
                {
                    ( void ) SM_SendF( "Flowrate [%3d] Single [ %0.2f PSI ] Bifurcated [ %0.2f PSI ]\r\n",
                                      RATE_VALUE_ARR[i], buffer[i][SINGLE_NEEDLE_LINESET],
                                      buffer[i][BIFURCATED_NEEDLE_LINESET] );
                }
            }
            else
            {
                ( void ) SM_SendF( "Failed to read downstream threshold file\r\n" );
            }
        }
        else
        {
            ( void ) SM_SendF( "Command not recognized\r\n" );
            CLI_ShowOccCmds( source_ );
        }
    }
    else if( strcmp( g_ParsedMsg[1], "sample" ) == 0 )
    {
        // Send Sample Data
        msg.Id = OCC_SAMPLE;
        msg.Data.AveragePressure = ( float )atof( g_ParsedMsg[2] );

        status = OCC_Notify( &msg );
        CLI_Trace( source_, status, __LINE__ );
    }
    else if( strcmp( g_ParsedMsg[1], "upstream" ) == 0 )
    {
        if( strcmp( g_ParsedMsg[2], "on" ) == 0 )
        {
            status = OCC_EnableUpStreamDetect( true );
            CLI_Trace( source_, status, __LINE__ );
        }
        else if( strcmp( g_ParsedMsg[2], "off" ) == 0 )
        {
            status = OCC_EnableUpStreamDetect( false );
            CLI_Trace( source_, status, __LINE__ );
        }
        else
        {
            ( void ) SM_SendF( "Command not recognized\r\n" );
            CLI_ShowOccCmds( source_ );
        }
    }
    else if( strcmp( g_ParsedMsg[1], "detection" ) == 0 )
    {
        if( strcmp( g_ParsedMsg[2], "on" ) == 0 )
        {
            status = OCC_EnableDetection( true );
            CLI_Trace( source_, status, __LINE__ );
        }
        else if( strcmp( g_ParsedMsg[2], "off" ) == 0 )
        {
            status = OCC_EnableDetection( false );
            CLI_Trace( source_, status, __LINE__ );
        }
        else
        {
            ( void ) SM_SendF( "Command not recognized\r\n" );
            CLI_ShowOccCmds( source_ );
        }
    }
    else if( strcmp( g_ParsedMsg[1], "cal" ) == 0 )
    {
        status = OCC_Cal( ( float )atof( g_ParsedMsg[2] ) );
        CLI_Trace( source_, status, __LINE__ );
    }
    else
    {
        ( void ) SM_SendF( "Command not recognized\r\n" );
        CLI_ShowOccCmds( source_ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_Occ ( SERIAL_CMD_SOURCE_T source_ )
{
    switch( g_ParmCount )
    {
    case 1:
        CLI_ShowOccCmds( source_ );
        break;

    case 2:
        CLI_HandleTwoOccArgs( source_ );
        break;

    case 3:
        CLI_HandleThreeOccArgs( source_ );
        break;

    case 5:
        CLI_HandleFiveOccArgs( source_ );
        break;

    case 6:
        CLI_HandleSixOccArgs( source_ );
        break;

    default:
        ( void ) SM_SendF( "Command not recognized\r\n" );
        CLI_ShowOccCmds( source_ );
        break;
    }
}

///////////////////////////////////////////////////////////////////////////////
static void CLI_ShowAilCmds ( SERIAL_CMD_SOURCE_T source_ )
{
    ( void ) SM_SendF( "Supported AIL commands:\r\n" );
    ( void ) SM_SendF(
                      "ail get thresh\\win\\vol \r\n\tPrints the AIL threshold, sampling window duration, or detected volume\r\n" );
    ( void ) SM_SendF(
                      "ail get array sensor_id\r\n\tPrints the AIL history array for the specified sensor\r\n" );
    ( void ) SM_SendF(
                      "ail set thresh\\win\\bubble # \r\n\tSets the AIL threshold (1-5ml) or sampling window is history duration(seconds) or single bubble volume(ul, effects sampling rate)\r\n" );
    ( void ) SM_SendF(
                      "ail start type rpm threshold - type 0 = fluid, 1 air, rpm is motor speed, and threshold is 1-5ml\r\n" );
    ( void ) SM_SendF( "ail stop \r\n\tStops detection and motor and resets history.\r\n" );
    ( void ) SM_SendF( "ail pause \r\n\tPauses the AIL detection without resetting history.\r\n" );
    ( void ) SM_SendF( "ail read sensor_id \r\n\tRead sensor memory.\r\n" );
    ( void ) SM_SendF(
                      "ail write sensor_id pin_state ref \r\n\tWrite sensor config where \r\n\t\tpin_state 0=OD \r\n\t\tpin_state 1=OD inverted \r\n\t\tpin_state 2=PP \r\n\t\tpin_state 3=PP inverted \r\n\t\treference level is 0 - 15 \r\n" );
    ( void ) SM_SendF( "ail cal sensor_id \r\n\tPerforms sensor calibration\r\n" );
    ( void ) SM_SendF(
                      "ail disable sensor_id [OPTION] \r\n\tOPTION == 1 disables sensor such that samples do not count toward alarm.\r\n\tOPTION == 0 enables detection\r\n\tsensor_id can be 0 or 1\r\n" );
    ( void ) SM_SendF( "ail poll sensor_id \r\n\tIndicates whether the specified sensor detects air or fluid\r\n" );

}

///////////////////////////////////////////////////////////////////////////////
static void CLI_ShowOccCmds ( SERIAL_CMD_SOURCE_T source_ )
{
    ( void ) SM_SendF( "Supported OCC commands:\r\n" );
    ( void ) SM_SendF(
                      "occ start rpm needle samplingtime filtercnt, rpm is motor speed, needle 1- single 2- bifurcated, samplingtime is 100-300ms, filtercnt is M and N filer for threshold check\r\n" );
    ( void ) SM_SendF(
                      "occ read [OPTION] \r\n\tRead OPTION, OPTION -- pressure, calpara \r\n" );
    ( void ) SM_SendF( "occ stop \r\n\tStops Occlusion detection and motor and resets algorithm paramters.\r\n" );
    ( void ) SM_SendF( "occ pause \r\n\tPauses the Occlusion detection without resetting algorithm paramters.\r\n" );
    ( void ) SM_SendF( "occ swtoana \r\n\tSwitch MAX1454 part in fixed analog mode\r\n" );
    ( void ) SM_SendF(
                      "occ cal force \r\n\tforce is Newtons applied to sensor. Software expects Zero Newton cal first followed by non-zero Newton cal\r\n" );
    ( void ) SM_SendF(
                      "occ upstream on\\off\r\n\tForce upstream occlusion detection on or off.\r\n" );
    ( void ) SM_SendF( "occ detection on\\off\r\n\tForce occlusion detection on or off\r\n" );
    ( void ) SM_SendF(
                      "occ set [OPTION] rpm needle threshold, rpm is motor speed, needle 1- single 2- bifurcated, threshold is occ detection threshold limit in PSI\r\n\tOPTION == upstream for Upstream OCC Limit\r\n\tOPTION == downstream for Downstream OCC Limit\r\n" );
    ( void ) SM_SendF(
                      "occ get [OPTION] rpm needle, rpm is motor speed, needle 1- single 2- bifurcated\r\n\tOPTION == upstream for Upstream OCC Limit\r\n\tOPTION == downstream for Downstream OCC Limit\r\n" );
}

///////////////////////////////////////////////////////////////////////////////
void CLI_ReadAdcDataCmdHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    float fval;
    uint32_t channel;

    if( g_ParmCount == 2 )
    {
        if( sscanf( g_ParsedMsg[1], "%u", &channel ) == 1 )
        {
            if( channel < ( uint32_t )ADC_CHANNEL_COUNT )
            {
                status = ADC_ReadChannel( (ADC_CHANNEL_T)channel, &fval );
                if( status != ERR_NONE )
                {
                    ( void ) SM_SendF( "Error reading raw sample, Error: %d\r\n", status );
                    return;
                }

                ( void ) SM_SendF( "  %d-(%s): %0.3f  V\r\n",
                                  channel,
                                  S_ADC_INPUT_STRINGS[ channel ],
                                  fval );
            }
            else
            {
                (void)SM_SendF( "Error: Channel number out of range\r\n" );
            }
        }
    }
    else if( g_ParmCount == 1 )
    {
        for( channel = 0; channel < (uint32_t)ADC_CHANNEL_COUNT; channel++ )
        {
            status = ADC_ReadChannel( (ADC_CHANNEL_T)channel, &fval );
            if( status != ERR_NONE )
            {
                ( void ) SM_SendF( "Error reading raw sample, Error: %d\r\n", status );
                return;
            }
            ( void ) SM_SendF( "  %d-(%s): %0.3f  V\r\n",
                              channel,
                              S_ADC_INPUT_STRINGS[ channel ],
                              fval );
        }
    }
    else
    {
        ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
    }
}

//////////////////////////////////////////////////////////////////////////////////
void CLI_ButtonPressHandler ( SERIAL_CMD_SOURCE_T source_ )  //TODO: This will no longer be needed once we have buttons on the prototype board.
{                                     //or we can edit this function to make calls through the switch manager.
    if( strcmp( g_ParsedMsg[1], "down" ) == 0 )
    {
        SwM_PostEvent( GPIO_ID_USER_DOWN, GPIO_INPUT_STATE_HIGH );
    }
    else if( strcmp( g_ParsedMsg[1], "up" ) == 0 )
    {
        SwM_PostEvent( GPIO_ID_USER_UP, GPIO_INPUT_STATE_HIGH );
    }
    else if( strcmp( g_ParsedMsg[1], "start" ) == 0 )
    {
        SwM_PostEvent( GPIO_ID_USER_START, GPIO_INPUT_STATE_HIGH );
    }
    else if( strcmp( g_ParsedMsg[1], "!start" ) == 0 )
    {
        SwM_PostEvent( GPIO_ID_USER_START, GPIO_INPUT_STATE_LOW );
    }
    else if( strcmp( g_ParsedMsg[1], "pause" ) == 0 )
    {
        SwM_PostEvent( GPIO_ID_USER_PAUSE, GPIO_INPUT_STATE_HIGH );
    }
    else if( strcmp( g_ParsedMsg[1], "power" ) == 0 )
    {
        SwM_PostEvent( GPIO_ID_USER_POWER, GPIO_INPUT_STATE_HIGH );
    }
    else if( strcmp( g_ParsedMsg[1], "info" ) == 0 )
    {
        SwM_PostEvent( GPIO_ID_USER_INFO, GPIO_INPUT_STATE_HIGH );
    }
    else if( strcmp( g_ParsedMsg[1], "back" ) == 0 )
    {
        SwM_PostEvent( GPIO_ID_USER_BACK, GPIO_INPUT_STATE_HIGH );
    }
    else if( strcmp( g_ParsedMsg[1], "ok" ) == 0 )
    {
        SwM_PostEvent( GPIO_ID_USER_OK, GPIO_INPUT_STATE_HIGH );
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
        return;
    }

}

////////////////////////////////////////////////////////////////////////////////
void CLI_CfgUsage ( SERIAL_CMD_SOURCE_T source_ )
{
    ( void )SM_SendF( "  lang       name (file must exist in language folder)\r\n");
    ( void )SM_SendF( "  vol        user controlled audio volume (0%%-100%%)\r\n" );
    ( void )SM_SendF( "  bri        LED/LCD backlight brightness (0%%-100%%)\r\n" );
    ( void )SM_SendF( "  hyunit     HY Rate Unit (0=ml/min/site, 1=ml/hr/site)\r\n" );
    ( void )SM_SendF( "  ver        Configuration structure version\r\n" );
    ( void )SM_SendF( "  rest       restore current settings from flash \r\n" );
    ( void )SM_SendF( "  alarmvol   alarm audio volume (0%%-100%%)\r\n" );
    ( void )SM_SendF( "  cidp       enable/disable for cidp setting\r\n");
    ( void )SM_SendF( "  srno       set/get for serial number \r\n");
    ( void )SM_SendF( "  ?          display cfg options\r\n" );
}

////////////////////////////////////////////////////////////////////////////////
void CLI_CfgHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    int val = 0;
    bool readAll = false;
    char msg[20] = "";
    bool optiongood = false;
    AM_MESSAGE_T amMsg;
    char serialNumber[ SERIAL_NUMBER_STRING_LENGTH ] = {0};                // Serial Number
    char tempString[MAX_FILE_NAME_CHAR_SIZE];

    if( g_ParmCount == 1 )
    {
        // Read and display all config values
        readAll = true;
    }
    ///////////////////////////////////////////////////////////////
    // Read and/or write single value, save, or restore
    if( strcmp( g_ParsedMsg[1], "?" ) == 0 )
    {
        // Display usage
        CLI_CfgUsage( source_ );
    }
    ///////////////////////////////////////////////////////////////
    if( readAll || ( strcmp( g_ParsedMsg[1], "lang" ) == 0 ) )
    {
        optiongood = true;
        if( g_ParmCount == 3 )  // set language
        {
            // Initialize temp string
            memset( tempString, 0, sizeof( tempString ) );

            // Get current language for restoration if needed.
            status = CFG_GetLanguage( tempString );
            CLI_Trace( source_, status, __LINE__ );

            // Set cfg to new desired language, used in filename by LM
            status = CFG_SetLanguage( g_ParsedMsg[2] );
            CLI_Trace( source_, status, __LINE__ );

            // Attempt to load new language
            status = LM_SetLanguageFile( true );
            if( status != ERR_NONE )
            {
                ( void )SM_SendF( "Error language file not found\r\n" );
                CLI_Trace( source_, status, __LINE__ );
            }
        }

        status = CFG_GetLanguage( tempString );
        if( status != ERR_NONE )
        {
            ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
        }
        else
        {
            ( void )SM_SendF( "                language : %s\r\n", tempString );
        }
    }
    /////////////////////////////////////////////////////////////////
    if( readAll || ( strcmp( g_ParsedMsg[1], "vol" ) == 0 ) )
    {
        optiongood = true;
        if( g_ParmCount == 3 )  // set
        {
            if( sscanf( g_ParsedMsg[2], "%d", &val ) == 1 )
            {
                //Set Cfg volume here for correct read back value
                UIM_SetDefaultVolumeLevel( (uint8_t)val );
                UIM_SetVolumeLevel( true );
                // Update audio manager with new volume
                amMsg.EventType = VOLUME;
                amMsg.SaveVolFlag = false;
                amMsg.Volume = ( uint8_t )val;
                status = AM_CommandNotify( &amMsg );
                if( status != ERR_NONE )
                {
                    ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
                }
            }
            else
            {
                ( void )SM_SendF( "Error reading setting\r\n" );
            }
        }

        // Read setting
        val = 0;
        status = CFG_GetVolume( (uint8_t *)&val );
        if( status != ERR_NONE )
        {
            ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
        }
        else
        {
            ( void )SM_SendF( "                  volume : %d%%\r\n", val );
        }

    }
    /////////////////////////////////////////////////////////////////
    if( readAll || ( strcmp( g_ParsedMsg[1], "alarmvol" ) == 0 ) )
    {
        optiongood = true;
        if( g_ParmCount == 3 )  // set
        {
            if( sscanf( g_ParsedMsg[2], "%d", &val ) == 1 )
            {
                
                //Set Cfg volume here for correct read back value
                status = CFG_SetAlarmVolume( ( uint8_t )val );
                if( status != ERR_NONE )
                {
                    ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
                }

                // Update the audio manager with the new volume
                amMsg.EventType = AM_ALARM_VOL;
                amMsg.SaveVolFlag = false;
                amMsg.Volume = ( uint8_t )val;
                status = AM_CommandNotify( &amMsg );
                if( status != ERR_NONE )
                {
                    ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
                }
            }
            else
            {
                ( void )SM_SendF( "Error reading setting\r\n" );
            }
        }

        // Read setting
        val = 0;
        status = CFG_GetAlarmVolume( (uint8_t *)&val );
        if( status != ERR_NONE )
        {
            ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
        }
        else
        {
            ( void )SM_SendF( "            alarm volume : %d%%\r\n", val );
        }

    }
    //////////////////////////////////////////////////////////////
    if( readAll || ( strcmp( g_ParsedMsg[1], "bri" ) == 0 ) )
    {
        optiongood = true;
        if( g_ParmCount == 3 )  // set
        {
            if( sscanf( g_ParsedMsg[2], "%d", &val ) == 1 )
            {
                UIM_SetDefaultBrightnessLevel( (uint8_t)val );
                UIM_SetBrightnessLevel( true );
            }
            else
            {
                ( void )SM_SendF( "Error reading setting\r\n" );
            }
        }

        // Read setting
        val = 0;
        status = CFG_GetBrightness( (uint8_t *)&val );
        if( status != ERR_NONE )
        {
            ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
        }
        else
        {
            ( void )SM_SendF( "              brightness : %d%%\r\n", (int)val );
        }
    }
    /////////////////////////////////////////////////////////////////
    if( readAll || ( strcmp( g_ParsedMsg[1], "ver" ) == 0 ) )
    {
        optiongood = true;

        // Read setting
        val = 0;
        status = CFG_GetVer( (uint8_t *)&val );
        if( status != ERR_NONE )
        {
            ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
        }
        else
        {
            ( void )SM_SendF( "                 version : %d\r\n", val );
        }
    }

    ////////////////////////////////////////////////
    if( strcmp( g_ParsedMsg[1], "rest" ) == 0 )
    {
        optiongood = true;
        status = CFG_RestoreFromFlash();
        if( status != ERR_NONE )
        {
            ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
        }
        else
        {
            ( void )SM_SendF( "Config settings restored from flash\r\n", msg );
        }
    }
    /////////////////////////////////////////////////////////////////
    if( readAll || ( strcmp( g_ParsedMsg[1], "cidp" ) == 0 ) )
    {
        optiongood = true;
        if( g_ParmCount == 3 )  // set
        {
            if( sscanf( g_ParsedMsg[2], "%d", &val ) == 1 )
            {
                //Set CIDP here
                status = CFG_SetCIDP( ( bool )val );
                if( status != ERR_NONE )
                {
                    ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
                }
            }
            else
            {
                ( void )SM_SendF( "Error reading setting\r\n" );
            }
        }

        // Read setting
        val = 0;
        status = CFG_GetCIDP( (bool *)&val );
        if( status != ERR_NONE )
        {
            ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
        }
        else
        {
            ( void )SM_SendF( "                    CIDP : %d\r\n", val );
        }
    }
    
    /////////////////////////////////////////////////////////////////
    if( readAll || ( strcmp( g_ParsedMsg[1], "srno" ) == 0 ) )
    {
        optiongood = true;
        if( g_ParmCount == 3 )  // set 
        {
            if( sscanf( g_ParsedMsg[2], "%s", &serialNumber[0] ) == 1 )
            {
                //Set Serial Number here for correct read back value
                status = CFG_SetSerialNumber( &serialNumber[0] );
                if( status != ERR_NONE )
                {
                    ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
                }
            }
            else
            {
                ( void )SM_SendF( "Error reading setting\r\n" );
            }
        }
        
        memset( serialNumber, 0, sizeof(serialNumber) );

        // Read Serial Number setting
        status = CFG_GetSerialNumber( &serialNumber[0] );
        if( status != ERR_NONE )
        {
            ( void )SM_SendF( "Error: %s\r\n", S_ERR_STRINGS[status] );
        }
        else
        {
            ( void )SM_SendF( "                  SerialNumber : %s\r\n", serialNumber );
        }

    }
    
    //////////////////////////////////////////////////////
    if( !optiongood )
    {
        ( void )SM_SendF( "option not recognized, use " "cfg ?" " for valid options\r\n" );
    }

}

////////////////////////////////////////////////////////////////////////////////
void CLI_FsTest ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;

    status = FS_Test( NOR_DEVICE );
    if( status != ERR_NONE )
    {
        ( void ) SM_SendF( "Error: %d\r\n", status );
        return;
    }

    ( void ) SM_SendF( "File System Test Passed" );
    ( void ) SM_SendF( "\r\n" );

}

////////////////////////////////////////////////////////////////////////////////
void CLI_FsQuery ( SERIAL_CMD_SOURCE_T source_ )
{
    FS_QTY count = FSFile_GetFileCnt();
    ( void ) SM_SendF( "Number of Open Files: %d\r\n", count );
}

////////////////////////////////////////////////////////////////////////////////
void CLI_FsRead ( SERIAL_CMD_SOURCE_T source_ )
{
    FS_ERR err;
    FS_FILE     *fPtr;
    CPU_CHAR readData[ COMMAND_LINE_LENGTH_MAX ];
    CPU_CHAR pathName[ COMMAND_PARMS_MAX ];
    FS_ENTRY_INFO info;
    CPU_SIZE_T size;

    // Get FILE PATH with Working Dir
    FS_WorkingDirGet( (CPU_CHAR   *)pathName, (CPU_SIZE_T  )COMMAND_PARMS_MAX, &err );
    if( err != FS_ERR_NONE )
    {
        CLI_Trace( source_, ERR_FS, __LINE__ );
        return;
    }
    strncat( pathName, g_ParsedMsg[1], COMMAND_PARMS_MAX );

    // Get info about file
    FSEntry_Query( pathName, &info, &err );
    if( err != FS_ERR_NONE )
    {
        // return if file does not exist or get other error
        CLI_Trace( source_, ERR_FS, __LINE__ );
        return;
    }

    fPtr = FSFile_Open( pathName, FS_FILE_ACCESS_MODE_RD, &err );
    if( err != FS_ERR_NONE )
    {
        CLI_Trace( source_, ERR_FS, __LINE__ );
        return;
    }

    ( void ) SM_SendF( "File Contain: \r\n" );
    while( !fs_feof( fPtr ) )
    {
        // FILE READ
        size = FSFile_Rd( fPtr, &readData[0], (COMMAND_LINE_LENGTH_MAX - 1), &err );
        if( ( err != FS_ERR_NONE ) || ( size == 0 ) )
        {
            FSFile_Close( fPtr, &err );
            CLI_Trace( source_, ERR_FS, __LINE__ );
            return;
        }
        //Print data into terminal
        ( void ) SM_SendF( readData );
        ( void ) SM_SendF( "\r\n" );
        memset( readData, 0, sizeof(readData) );
    }

    FSFile_Close( fPtr, &err );
    if( err != FS_ERR_NONE )
    {
        CLI_Trace( source_, ERR_FS, __LINE__ );
        return;
    }
    ( void ) SM_SendF( "File System Read successfully" );
    ( void ) SM_SendF( "\r\n" );
}

////////////////////////////////////////////////////////////////////////////////
void CLI_FsFormat ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    char format[sizeof(NOR_DEVICE)];
    strncpy( format, NOR_DEVICE, sizeof(format) );

    status = FS_NorFormat( format );
    if( status != ERR_NONE )
    {
        ( void ) SM_SendF( "Flash format Error: %d\r\n", status );
        return;
    }

    ( void ) SM_SendF( "Flash formatted successfully" );
    ( void ) SM_SendF( "\r\n" );

}

////////////////////////////////////////////////////////////////////////////////
void CLI_FileDelete ( SERIAL_CMD_SOURCE_T source_ )
{

    CPU_CHAR pathName[ COMMAND_PARMS_MAX ];
    FS_ERR err;

    // ----------------- FILE WRITE TEST ------------------
    strncpy( pathName, NOR_DEVICE, COMMAND_PARMS_MAX );
    strcat( pathName, FS_ROOT_DIR );
    strncat( pathName, g_ParsedMsg[1], COMMAND_PARMS_MAX );

    FSEntry_Del( pathName, FS_ENTRY_TYPE_ANY, &err );

    if( err != FS_ERR_NONE )
    {
        ( void ) SM_SendF( "Error: %d\r\n", err );
    }
    else
    {
        ( void ) SM_SendF( "File Deleted Successfully" );
        ( void ) SM_SendF( "\r\n" );
    }

}

////////////////////////////////////////////////////////////////////////////////
void CLI_FileChangeDir ( SERIAL_CMD_SOURCE_T source_ )
{
    FS_ERR err;
    CPU_CHAR pathName[ COMMAND_PARMS_MAX ] = {0};

    // ----------------- FILE Change Directory  ------------------
    strncpy( pathName, NOR_DEVICE, COMMAND_PARMS_MAX );
    strncat( pathName, g_ParsedMsg[1], COMMAND_PARMS_MAX - strlen( pathName ) );

    FS_WorkingDirSet( (CPU_CHAR *) pathName, &err );
    if( err != FS_ERR_NONE )
    {
        ( void ) SM_SendF( "Error: %d\r\n", err );
    }
    else
    {
        ( void )SM_SendF( " Changed Directory %s ", pathName );
        ( void ) SM_SendF( "\r\n" );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_FsDir ( SERIAL_CMD_SOURCE_T source_ )
{
    FS_DIR          *dirPtr;
    FS_DIR_ENTRY dirEntry;
    FS_ERR err = FS_ERR_NONE;
    ERROR_T status = ERR_NONE;

    struct tm     *timeStruct;

    char *buf;

    CPU_CHAR pathName[ COMMAND_PARMS_MAX ] = {0};

    //  FILE  Directory PATH
    strncpy( pathName, g_ParsedMsg[1], COMMAND_PARMS_MAX );

    // Open Directory
    status = FS_OpenDir(&dirPtr,( CPU_CHAR * )pathName );

    if( status != ERR_NONE )
    {
        CLI_Trace( source_, status, __LINE__ );
        return;
    }

    ( void )SM_SendF( "%s     %s     %s\r\n", "Name", "Size", "Time" );

    while( err == FS_ERR_NONE )
    {
        // Read directory entry.
        FSDir_Rd( ( FS_DIR * ) dirPtr, (FS_DIR_ENTRY  *)&dirEntry, ( FS_ERR * )&err );
        if( err == FS_ERR_NONE )
        {
            timeStruct = localtime( &dirEntry.Info.DateTimeCreate );

            buf = asctime( timeStruct );

            ( void )SM_SendF( "%s     %d     %s\r\n", dirEntry.Name, dirEntry.Info.Size, buf );
        }
    }

    // Close Directory
    status = FS_CloseDir( dirPtr );
    CLI_Trace( source_, status, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
void CLI_FwUpdate ( SERIAL_CMD_SOURCE_T source_ )
{
    
#define FW_DIR_PATH  "\\firmware_update\\"
    
#define FW_FILE_NAME "HOPP-AppOnly-Release.bin"
         
    ERROR_T err = ERR_NONE;
    int n = 0;
    char outputString[ COMMAND_LINE_LENGTH_MAX ];
    bool usbEnabled = FALSE;
    
    FS_FILE *fPtr;
    OPEN_FILE_T fileData;    
    
    CPU_CHAR pathName[ COMMAND_PARMS_MAX ] = {0};
   
    ( void )SM_SendF( "Attempting to access file system and copy over FW image. May take several minutes.\r\n" );

    n = snprintf(pathName, sizeof(pathName), "%s%s", FW_DIR_PATH, FW_FILE_NAME) ;
    if( n <= 0)
    {
        snprintf( outputString, COMMAND_LINE_LENGTH_MAX, "FW Dir Path or Filename too long \r\n" );
        ( void )SM_SendF( "%s", outputString );
        return; // invalid path, err and return immediately
    }

    err = USB_State( &usbEnabled );
    CLI_Trace( source_, err, __LINE__ );

    if( ( err == ERR_NONE ) && usbEnabled )
    {
        err = FS_ReleaseAccess();
        if( err == ERR_NONE )
        {
            err = USB_InterfaceDisable();
            CLI_Trace( source_, err, __LINE__ );
        }
    }
            
    fileData.Name = pathName;
    fileData.File = &fPtr;
    fileData.Mode = FS_FILE_ACCESS_MODE_RD;
    
    err = FS_OpenFile( &fileData );
    
    if( err == ERR_DOES_NOT_EXIST )
    {
        snprintf( outputString, COMMAND_LINE_LENGTH_MAX, "%s  Does not exist. Please confirm firmware image is in firmware directory\r\n", fileData.Name );
    }
    else if( err != ERR_NONE )
    {
         snprintf( outputString, COMMAND_LINE_LENGTH_MAX, "Unknown error while attempting to access firmware image\r\n" );
    }
    else 
    {
        err = WD_DisableTaskWdog( WD_CLI );
        CLI_Trace( source_, err, __LINE__ );

        err = IFM_ProgramFwStagingArea( fPtr );
        if( err )
        {
            snprintf( outputString, COMMAND_LINE_LENGTH_MAX, "Issue with programming new image. Please retry. \r\n" );
        }
        else
        {
            snprintf( outputString, COMMAND_LINE_LENGTH_MAX, "Programming image successful. Restart device. \r\n" );
        }

        err = WD_EnableTaskWdog( WD_CLI );
        CLI_Trace( source_, err, __LINE__ );

    }
    
    err = FS_CloseFile( fPtr );
    if( err != ERR_NONE )
    {
        CLI_Trace( source_, err, __LINE__ );
    }
    
    err = FS_AcquireAccess( FS_DEFAULT_TIMEOUT );    
    
    if(( err == ERR_NONE ) && ( CLI_GetSource() == SERIAL_USB ))
    {
        err = USB_InterfaceEnable();
        CLI_Trace( source_, err, __LINE__ );
    }
    
    ( void )SM_SendF( "%s", outputString );
}

////////////////////////////////////////////////////////////////////////////////
static void CLI_ShowAllScreenCommands ( SERIAL_CMD_SOURCE_T source_ )
{
    ( void )SM_SendF( "%s", "\r\nList of Supported Screen Commands:\r\n\r\n" );

    for( SCREEN_ID_T i=SCREEN_BLANK; i<MAX_SCREENS; i++ )
    {
        ( void )SM_SendF( "%s \r\n", s_CliScreenTable[i].NAME );
    }
}

////////////////////////////////////////////////////////////////////////////////
static ERROR_T CLI_ScreenLookUp ( const char *name_, SCREEN_ID_T *screenId_ )
{
    if( ( name_ != NULL ) && ( screenId_ != NULL ) )
    {
        for( SCR_ATTR_VALUE_T *screenData = s_CliScreenTable; screenData->NAME != NULL; screenData++ )
        {
            if( strcmp( screenData->NAME, name_ ) == 0 )
            {
                *screenId_ = screenData->Value;
                return ERR_NONE;
            }
        }
    }
    return ERR_PARAMETER;
}

////////////////////////////////////////////////////////////////////////////////
void CLI_GUI_DispScreen_Cmd_Handler ( SERIAL_CMD_SOURCE_T source_ )
{
    SCREEN_ID_T screenId = SCREEN_0;
    ERROR_T reportedError = ERR_NONE;

    if( g_ParmCount == 1 )
    {
        CLI_ShowAllScreenCommands( source_ );
    }
    else
    {
        if( strcmp( g_ParsedMsg[1], "all" ) == 0 )
        {
            reportedError = WD_DisableTaskWdog( WD_CLI );
            CLI_Trace( source_, reportedError, __LINE__ );
            
            for( SCREEN_ID_T i=SCREEN_BLANK; i<MAX_SCREENS; i++ )
            {
                ( void )SM_SendF( "%s \r\n", s_CliScreenTable[i].NAME );
                UI_SetScreen( s_CliScreenTable[i].Value );
                ( void )OSTimeDlyHMSM( TIME_BTWN_SCR_VIEW );
            }
            
            reportedError = WD_EnableTaskWdog( WD_CLI );
            CLI_Trace( source_, reportedError, __LINE__ );
        }
        else
        {
            reportedError = CLI_ScreenLookUp( g_ParsedMsg[1], &(screenId) );
            if( reportedError != ERR_NONE )
            {
                ( void )SM_SendF( "%s", "Screen not found\r\n" );
                CLI_Trace( source_, reportedError, __LINE__ );
            }
            else
            {
                UI_SetScreen( screenId );
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_GUI_DispString_Cmd_Handler ( SERIAL_CMD_SOURCE_T source_ )
{
    UNUSED( source_ );
    ///TODO: Should return an error code, but its part of the GUI package.
    GUI_DispString( g_ParsedMsg[1] );

}

////////////////////////////////////////////////////////////////////////////////
void CLI_Toggle_SM_Debug ( SERIAL_CMD_SOURCE_T source_ )
{
    UNUSED( source_ );
    if( strcmp( g_ParsedMsg[1], "on" ) == 0 )
    {
        SwM_SetDebugPrints( true );
    }
    else if( strcmp( g_ParsedMsg[1], "off" ) == 0 )
    {
        SwM_SetDebugPrints( false );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_ControlUSBInterface ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T reportedError = ERR_NONE;
    bool usbEnabled = FALSE;

    if( strcmp( g_ParsedMsg[1], "on" ) == 0 )
    {
        reportedError = FS_AcquireAccess( FS_DEFAULT_TIMEOUT );

        if( reportedError == ERR_NONE )
        {
            reportedError = USB_InterfaceEnable();
        }

        CLI_Trace( source_, reportedError, __LINE__ );
    }
    else if( strcmp( g_ParsedMsg[1], "off" ) == 0 )
    {
        reportedError = USB_State( &usbEnabled );
        CLI_Trace( source_, reportedError, __LINE__ );

        if( ( reportedError == ERR_NONE ) && usbEnabled )
        {
            reportedError = FS_ReleaseAccess();

            if( reportedError == ERR_NONE )
            {
                reportedError = USB_InterfaceDisable();
            }

            CLI_Trace( source_, reportedError, __LINE__ );
        }
    }
    else
    {
        ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_ControlBatterySimulation ( SERIAL_CMD_SOURCE_T source_ )
{
    UNUSED( source_ );
    if( strcmp( g_ParsedMsg[1], "on" ) == 0 )
    {
        ( void )BATT_StartStopSimulation( true );
    }
    else if( strcmp( g_ParsedMsg[1], "off" ) == 0 )
    {
        ( void )BATT_StartStopSimulation( false );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_Toggle_GPIO_Debug ( SERIAL_CMD_SOURCE_T source_ )
{
    UNUSED( source_ );
    if( strcmp( g_ParsedMsg[1], "on" ) == 0 )
    {
        GPIO_SetDebugPrints( true );
    }
    else if( strcmp( g_ParsedMsg[1], "off" ) == 0 )
    {
        GPIO_SetDebugPrints( false );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_GetDateTime ( SERIAL_CMD_SOURCE_T source_ )
{
    CLK_DATE_TIME dateTime;

    if( Clk_GetDateTime( &dateTime ) == TRUE )
    {
        ( void ) SM_SendF( "%02u/%02u/%04d %02u:%02u:%02u\r\n",
                          dateTime.Month, dateTime.Day, dateTime.Yr, dateTime.Hr, dateTime.Min, dateTime.Sec );
    }
    else
    {
        ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_HARDWARE] );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_SetDateTime ( SERIAL_CMD_SOURCE_T source_ )
{
    CLK_DATE_TIME dateTime;
    CLK_YR year;
    CLK_MONTH month;
    CLK_DAY day;
    CLK_HR hour;
    CLK_MIN min;
    CLK_SEC sec;

    if( g_ParmCount == 7 )
    {
        month = ( CLK_MONTH )atoi( g_ParsedMsg[1] );
        day = ( CLK_DAY )atoi( g_ParsedMsg[2] );
        year = ( CLK_YR )atoi( g_ParsedMsg[3] );
        hour = ( CLK_HR )atoi( g_ParsedMsg[4] );
        min = ( CLK_MIN )atoi( g_ParsedMsg[5] );
        sec = ( CLK_SEC )atoi( g_ParsedMsg[6] );

        if( Clk_DateTimeMake( &dateTime, year, month, day, hour, min, sec, 0 ) == TRUE )
        {
            if( Clk_SetDateTime( &dateTime ) == TRUE )
            {
                ( void ) SM_SendF( "%02u/%02u/%04d %02u:%02u:%02u Set Successfully\r\n", month, day, year, hour, min,
                                  sec );
            }
            else
            {
                ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
            }
        }
        else
        {
            ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
        }
    }
    else
    {
        ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_GetHPTimeHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    uint64_t rawTime = 0ull;
    TIME_T fmtTime = {0, 0, 0, 0, 0 };


    rawTime =  HPT_GetTime();

    fmtTime = HPT_ToTIME_T( rawTime );

    (void)SM_SendF( "raw          : %llu ticks\r\n", rawTime );

    (void)SM_SendF( "formatted    : %u days, %02u:%02u:%02u.%03u\r\n",
                   fmtTime.Days, fmtTime.Hours, fmtTime.Minutes, fmtTime.Seconds,
                   fmtTime.Milliseconds );
}

////////////////////////////////////////////////////////////////////////////////
void CLI_HptTestHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    uint64_t start, stop;
    double delta;

    start = HPT_GetTime();

    HPT_Spin( 100 );

    stop = HPT_GetTime();

    delta = HPT_RawTimeToMicroseconds( stop - start );

    (void)SM_SendF( "Spin time: %2.3f\r\n", delta );
}

////////////////////////////////////////////////////////////////////////////////
void CLI_infstateHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    char msg[INF_MAX_STATE_NAME_LEN];

    ERROR_T status = INF_GetCurrentStateName( msg );
    if( status != ERR_NONE )
    {
        CLI_Trace( source_, status, __LINE__ ); 
        return;
    }

    (void)SM_SendF( "INF state: %s\r\n", msg );
}

////////////////////////////////////////////////////////////////////////////////
void CLI_InfEventHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    INF_MESSAGE_T msg;
    INF_EVENT_TYPE_T infEvent;

    if( g_ParmCount == 2 )
    {
        infEvent = ( INF_EVENT_TYPE_T )atoi( g_ParsedMsg[1] );
        if( infEvent < INF_NUM_INPUT_MESSAGE )
        {
            msg.InfEventType = infEvent;
            ERROR_T status = INF_SendMessage( &msg );
            if( status != ERR_NONE )
            {
                CLI_Trace( source_, status, __LINE__ );
            }
        }
        else
        {
            ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
        }
    }
    else
    {
        ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_TaskQuery ( SERIAL_CMD_SOURCE_T source_ )
{
    TASK_QUERY_DATA_T tData[NUM_TASKS];
    ERROR_T status;

    status = TM_TaskQuery( tData );
    if( status != ERR_NONE )
    {
        CLI_Trace( source_, status, __LINE__ );

        return;
    }

    ( void ) SM_SendF( "\r\n" );
    for( int i = 0; i < (int)NUM_TASKS; i++ )
    {
        ( void ) SM_SendF( "  %20s, Pri: %3d, SwCtr: %5d, StkSize: %3d, StkUsed: %3d\r\n",
                          tData[i].Name, tData[i].Priority,
                          tData[i].SwCtr, tData[i].StkSize, tData[i].StkUsed );
    }
    ( void ) SM_SendF( "\r\n" );

}

////////////////////////////////////////////////////////////////////////////////
void CLI_ver_Cmd_Handler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;
    
    char version[MAX_NUM_LINE_CHARS];
    
    memset( version, 0, MAX_NUM_LINE_CHARS );
    
    err = VER_GetBootVersionString( version );
    if( err )
    {
        CLI_Trace( source_, err, __LINE__ );
    }
    else
    {
        ( void ) SM_SendF( version );
    }
    
    memset( version, 0, MAX_NUM_LINE_CHARS );
    
    err = VER_GetVersionString( version );
    if( err )
    {
        CLI_Trace( source_, err, __LINE__ );
    }
    else
    {
        ( void ) SM_SendF( version );
    }
    
}

////////////////////////////////////////////////////////////////////////////////
void CLI_help_Cmd_Handler ( SERIAL_CMD_SOURCE_T source_ )
{
    uint16_t i, j, k;

    ERROR_T returnError = ERR_NONE;

    i = 0;
    while( COMMANDS[i].CMD != NULL )
    {

        memset( g_PadCmd,    0, sizeof( g_PadCmd ) );
        memset( g_PadSyntax, 0, sizeof( g_PadSyntax ) );

        /// Pad command string to longest cmd length
        k = (uint16_t)( g_LongestCmdLength - strlen( COMMANDS[i].CMD ) );
        for( j = 0; j < k; j++ )
        {
            g_PadCmd[j] = ' ';
        }

        if( COMMANDS[i].SYNTAX != NULL )
        {
            k = (uint16_t)( g_LongestSyntaxLength - strlen( COMMANDS[i].SYNTAX ) );
        }
        else
        {
            k = (uint16_t)( g_LongestSyntaxLength );
        }
        for( j = 0; j < k; j++ )
        {
            g_PadSyntax[j] = ' ';
        }

        returnError = SM_SendF( "%s%s %s%s : %s\r\n", COMMANDS[i].CMD, g_PadCmd,
                               COMMANDS[i].SYNTAX, g_PadSyntax,
                               COMMANDS[i].HELPMSG );

        CLI_Trace( source_, returnError, __LINE__ );

        i++;
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_Led_Cmd_Handler ( SERIAL_CMD_SOURCE_T source_ )
{
    uint8_t brightness;
    ERROR_T error = ERR_NONE;

    if( g_ParmCount == 3 )
    {
        int32_t ledNumber = atoi( g_ParsedMsg[1] );

        if( ( ledNumber < (int32_t)LED_TLC1_START ) || ( ledNumber >= (int32_t)LED_MAX ) )
        {
            ( void )SM_SendF( "Invalid LED number: Allowed LEDs are 0 to 31\r\n" );
            return;
        }

        if( strcmp( g_ParsedMsg[2], "on" ) == 0 )
        {
            if( LED_GetBrightnessPct( (LED_NUMBER_T)ledNumber, &brightness ) == ERR_NONE )
            {
                error = LED_On( (LED_NUMBER_T)ledNumber, LED_FULL_BRIGHTNESS_COUNTS );
                if( error != ERR_NONE )
                {
                    ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[error] );
                    return;
                }
            }
            else
            {
                ( void )SM_SendF( "Failed to get LED brightness\r\n" );
                return;
            }
        }
        else if( strcmp( g_ParsedMsg[2], "off" ) == 0 )
        {
            error = LED_Off( (LED_NUMBER_T)ledNumber );
            if( error != ERR_NONE )
            {
                ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[error] );
                return;
            }
        }
        else
        {
            ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
            return;
        }
    }
    else
    {
        ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_EncoderPosGetHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T error = ERR_NONE;

    int64_t posValue = 0;

    if( g_ParmCount == 1 )      ///if there is a position value intended to be set, then the parm count would be 2
    {

        error = ENC_PositionGet( &posValue );

        CLI_Trace( source_, error, __LINE__ );

        ( void )SM_SendF(  "%s %lli \r\n", "Encoder Pos: ", posValue );
    }
    else
    {
        posValue = strtoull( g_ParsedMsg[1], NULL, 10 ); //lint !e732 !e746 !e718 Lose of sign lint info.

        error = ENC_PositionSet( posValue );

        CLI_Trace( source_, error, __LINE__ );
    }

}

////////////////////////////////////////////////////////////////////////////////
void CLI_EncoderDirectionHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T error = ERR_NONE;

    ENCODER_DIR_T dir = ENCODER_DIR_CW;

    error = ENC_DirectionGet( &dir );

    CLI_Trace( source_, error, __LINE__ );

    if( dir == ENCODER_DIR_CW )
    {
        ( void )SM_SendF( "%s \r\n", "Encoder Direction: CW" );
    }
    else if( dir == ENCODER_DIR_CCW )
    {
        ( void )SM_SendF( "%s \r\n", "Encoder Direction: CCW" );
    }



}

////////////////////////////////////////////////////////////////////////////////
void CLI_LedBrightnessHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    int32_t ledNumber;
    int32_t brightnessPct;
    ERROR_T error;

    if( g_ParmCount == 3 )
    {
        ledNumber = atoi( g_ParsedMsg[1] );

        if( ( ledNumber < (int32_t)LED_TLC1_START ) || ( ledNumber >= (int32_t)LED_MAX ) )
        {
            ( void )SM_SendF( "Invalid LED number: Allowed LEDs are 0 to 31\r\n" );
            return;
        }

        brightnessPct = atoi( g_ParsedMsg[2] );

        if( ( brightnessPct > MAX_BRIGHTNESS_PCT ) || ( brightnessPct < LED_OFF_BRIGHTNESS ) )
        {
            ( void )SM_SendF( "Invalid Brightness: Allowed brightness between 0 to 100%\r\n" );
            return;
        }

        error = LED_SetBrightnessPct( ( LED_NUMBER_T )ledNumber, (uint8_t) brightnessPct );
        if( error != ERR_NONE )
        {
            ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[error] );
            return;
        }
    }
    else if( g_ParmCount == 2 )
    {
        ledNumber = atoi( g_ParsedMsg[1] );

        if( ( ledNumber < (int32_t)LED_TLC1_START ) || ( ledNumber >= (int32_t)LED_MAX ) )
        {
            ( void )SM_SendF( "Invalid LED number: Allowed LEDs are 0 to 31\r\n" );
            return;
        }

        error = LED_GetBrightnessPct( ( LED_NUMBER_T )ledNumber, (uint8_t *)&brightnessPct );
        if( error != ERR_NONE )
        {
            ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[error] );
            return;
        }
        ( void )SM_SendF( "LED %d Brightness : %u \r\n", ledNumber, brightnessPct );
    }
    else
    {
        ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_SimulateBattStateHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    int32_t battPercentage;

    if( g_ParmCount == 2 )
    {
        battPercentage = atoi( g_ParsedMsg[1] );

        if( ( battPercentage >= 0 ) && ( battPercentage <= 100 ) )
        {
            // Enable battery simulation
            ( void )BATT_StartStopSimulation( true );
            ( void )BATT_SetState( (uint8_t) battPercentage );
        }
        else
        {
            ( void )SM_SendF( "Invalid Battery Percentage: Allowed 0 to 100\r\n" );
            return;
        }
    }
    else
    {
        ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_SimulateFailSafeHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ( void )SM_SendF( "Generating Fail Safe Error\r\n" );

    // Reporting ERR_OS to force device into fail safe state.
    SAFE_ReportError( ERR_OS, __FILENAME__, __LINE__ ); //lint !e613 Filename is not NULL ptr
}

////////////////////////////////////////////////////////////////////////////////
void CLI_SimulateHallSensorStatus ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    LINESET_INFO_T needleStatus;
    DOOR_INFO_T doorStatus;

    if( g_ParmCount == 3 )
    {
        needleStatus = (LINESET_INFO_T)atoi( g_ParsedMsg[1] );
        doorStatus = (DOOR_INFO_T)atoi( g_ParsedMsg[2] );

        if( needleStatus < LINESET_INFO_MAX )
        {
            ( void )HALL_SetNeedleInfo( needleStatus );
        }
        else
        {
            ( void )SM_SendF( "Invalid Needle Info: Allowed 0 to 3\r\n" );
            return;
        }

        if( doorStatus < DOOR_INFO_MAX )
        {
            ( void )HALL_SetDoorInfo( doorStatus );
        }
        else
        {
            ( void )SM_SendF( "Invalid Door Info: Allowed 0 to 1\r\n" );
            return;
        }

        // Disable AIL detection during line simulation
        status = AILM_DisableSensor( AIL1, true );
        CLI_Trace( source_, status, __LINE__ );
        status = AILM_DisableSensor( AIL2, true );
        CLI_Trace( source_, status, __LINE__ );
    }
    else
    {
        ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_GetTDCDataHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err;

    TDC_INFO_T tdcStatus;
    float fval = 0.0f;
    int64_t posValue = 0;


    if( g_ParmCount == 1 )
    {
        err = HALL_GetTDCInfo( &tdcStatus );
        CLI_Trace( source_, err, __LINE__ );

        err = ADC_ReadChannel( MOTOR_TDC, &fval );
        CLI_Trace( source_, err, __LINE__ );

        err = ENC_PositionGet( &posValue );
        CLI_Trace( source_, err, __LINE__ );

        ( void )SM_SendF( "TDC: %s ADC: %0.3f EncPos: %lli \r\n", ( tdcStatus ? "TDC_NOT_READY" : "TDC_READY" ), fval,
                         posValue );

    }

}

////////////////////////////////////////////////////////////////////////////////
void CLI_SimulateAirInLineDetection ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    AILM_MSG_T msg;

    if( g_ParmCount == 1 )
    {
        ( void )memset( ( void *)&msg, 0, sizeof( msg ) );

        msg.Id = AILM_SIM_AIL;

        status = AILM_Notify( &msg );

        if( status != ERR_NONE )
        {
            ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
        }
    }
    else
    {
        ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_SimulateOcclusionDetection ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status;
    OCC_MSG_T msg;

    if( g_ParmCount == 1 )
    {
        ( void )memset( ( void *)&msg, 0, sizeof( msg ) );

        msg.Id = OCC_SIM_DETECTION;
        status = OCC_Notify( &msg );
        if( status != ERR_NONE )
        {
            ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
        }
    }
    else
    {
        ( void )SM_SendF( "%s\r\n", S_ERR_STRINGS[ERR_PARAMETER] );
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_DutyCycleHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T error = ERR_NONE;
    float pulseWidth;

    if( g_ParmCount == 2 )
    {
        pulseWidth = (float)atof( g_ParsedMsg[1] );

        error = PWM_PulseWidthSet( pulseWidth );
        if( error != ERR_NONE )
        {
            CLI_Trace( source_, error, __LINE__ );
        }
    }

    error = PWM_PulseWidthGet( &pulseWidth );
    if( error != ERR_NONE )
    {
        CLI_Trace( source_, error, __LINE__ );
    }
    (void)SM_SendF( "PWM percentage: %3.1f%%\r\n", pulseWidth );
}

////////////////////////////////////////////////////////////////////////////////
void CLI_HB_DirHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    HB_DIR_T dir = HB_DIR_CLOCKWISE;

    if( g_ParmCount == 2 )
    {
        if( strcmp( g_ParsedMsg[1], "cw" ) == 0 )
        {
            err = HB_DirectionSet( HB_DIR_CLOCKWISE );

            CLI_Trace( source_, err, __LINE__ );
        }
        else if( strcmp( g_ParsedMsg[1], "ccw" ) == 0 )
        {
            err = HB_DirectionSet( HB_DIR_COUNTERCLOCKWISE );

            CLI_Trace( source_, err, __LINE__ );
        }
    }
    else
    {
        err = HB_DirectionGet( &dir );

        if( err != ERR_NONE )
        {
            CLI_Trace( source_, err, __LINE__ );
        }
        else
        {
            ( void ) SM_SendF( "HB_Dir: %s \r\n", ( dir ? "CCW" : "CW" ) );
        }

    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_HB_EnableHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    if( strcmp( g_ParsedMsg[1], "t" ) == 0 )
    {
        err = HB_MotorEnable( true );
        CLI_Trace( source_, err, __LINE__ );
    }
    else if( strcmp( g_ParsedMsg[1], "f" ) == 0 )
    {
        err = HB_MotorEnable( false );
        CLI_Trace( source_, err, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_MTR_ShutDownHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T error = ERR_NONE;

    float dutyPercent = 0.0;

    error = MTR_ShutDown( true );

    CLI_Trace( source_, error, __LINE__ );

    error = PWM_PulseWidthGet( &dutyPercent );

    CLI_Trace( source_, error, __LINE__ );

    if( dutyPercent == 0.0 )
    {
        ( void )SM_SendF( "%s", "Motor shutdown successfully \r\n" );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_MTR_Timing ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T error = ERR_NONE;
    bool flag = false;

    if( strcmp( g_ParsedMsg[1], "t" ) == 0 )
    {
        flag = true;
    }
    else if( strcmp( g_ParsedMsg[1], "f" ) == 0 )
    {
        flag = false;
    }
    else
    {
        ( void )SM_SendF( "Command error. Enter t or f only\r\n"  );
        return;
    }

    error = MTR_EnableTiming( flag );
    CLI_Trace( source_, error, __LINE__ );

    if( error == ERR_NONE )
    {
        ( void )SM_SendF( "Timing output %s\r\n", flag ? "on" : "off"  );
    }
}

#if ( 0 ) ///TODO: This implementation to be completed in C1
////////////////////////////////////////////////////////////////////////////////
void CLI_MTR_P_BIT_Handler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T error = ERR_NONE;

    error = MTR_P_BIT(); //TODO: Will need to add Safety manager status type

    if( error != ERR_NONE )
    {
        TRACE( S_ERR_STRINGS[error] );
    }
}

#endif

////////////////////////////////////////////////////////////////////////////////
void CLI_MTR_FlowHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    if( g_ParmCount >= 2 )
    {

        float flowRate = ( float )atof( g_ParsedMsg[1] );

        err = MTR_SetFlow( flowRate );

        CLI_Trace( source_, err, __LINE__ );
    }
    else
    {
        ( void )SM_SendF( "%s", "Invalid Command \r\n" );
    }


}

////////////////////////////////////////////////////////////////////////////////

void CLI_MAX1454WriteMessageHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    MAX1454_REG_T reg;
    uint16_t value;

    if( g_ParmCount == 3 )
    {
        reg = ( MAX1454_REG_T )atoi( g_ParsedMsg[1] );
        value = ( uint16_t )atoi( g_ParsedMsg[2] );

        if( reg < MAX_REG )
        {
            if( MAX1454_WriteConfigReg( reg, value ) == ERR_NONE )
            {
                ( void )SM_SendF( "MAX1454 Write/Read Successfully \r\n" );
            }
            else
            {
                ( void )SM_SendF( "MAX1454 Write/Read Failed \r\n" );
            }
        }
        else
        {
            ( void ) SM_SendF( "Invalid Register: Allowed Range 0 to 6\r\n" );
        }
    }
    else
    {
        ( void )SM_SendF( "Err Parameter: Expected Command --> maxw [reg] [value]\r\n" );
    }
}

////////////////////////////////////////////////////////////////////////////////

void CLI_MAX1454WriteFlashPageHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    MAX1454_FLASH_PAGE_T page;

    if( g_ParmCount == 2 )
    {
        page = ( MAX1454_FLASH_PAGE_T )atoi( g_ParsedMsg[1] );

        if( page <= PAGE_1 )
        {
            if( MAX1454_WriteFlashPage( page ) == ERR_NONE )
            {
                ( void )SM_SendF( "MAX1454 Page Write Successfully \r\n" );
            }
            else
            {
                ( void )SM_SendF( "MAX1454 Page Write Failed \r\n" );
            }
        }
        else
        {
            ( void )SM_SendF( "MAX1454 Page0 and Page1 are accessible to write\r\n" );
        }
    }
    else
    {
        ( void )SM_SendF( "Err Parameter: Expected Command --> maxwpg [0/1]\r\n" );
    }
}

////////////////////////////////////////////////////////////////////////////////

void CLI_SwitchToAnalog ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T returnStatus;

    if( g_ParmCount == 1 )
    {
        returnStatus = MAX1454_SwithchToFixedAnalog();
        if( returnStatus == ERR_NONE )
        {
            ( void )SM_SendF( "MAX1454 successfully switched in fixed analog mode \r\n" );
        }
        else
        {
            ( void )SM_SendF( "MAX1454 failed to switch in fixed analog mode \r\n" );
        }
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_OctaveHandler ( SERIAL_CMD_SOURCE_T source_ )
{
#define ONE_SECOND    0, 0, 1, 0   // Format: H,M,S,Ms--> 1s

    enum
    {
        A_NOTE,
        C_NOTE,
        D_NOTE,
        E_NOTE,
        NUM_OF_NOTES
    };

    typedef enum
    {
        OCTAVE_THIRD,
        OCTAVE_FOURTH,
        OCTAVE_FIFTH,
        OCTAVE_SIXTH,
        OCTAVE_SEVENTH,
        NUM_OF_OCTAVES
    }OCTAVE_T;

    //lint -e26 '[' is part of the expression
    const uint16_t freqArray[ NUM_OF_OCTAVES ][ NUM_OF_NOTES ] =
    {
        [ OCTAVE_THIRD ] =
        {
            220, //A3
            131, //C3
            147, //D3
            165, //E3
        },
        [ OCTAVE_FOURTH ] =
        {
            440, //A4
            262, //C4
            294, //D4
            330, //E4
        },
        [ OCTAVE_FIFTH ] =
        {
            880, //A5
            523, //C5
            587, //D5
            659, //E5
        },
        [ OCTAVE_SIXTH ] =
        {
            1760, //A6
            1047, //C6
            1175, //D6
            1319, //E6
        },
        [ OCTAVE_SEVENTH ] =
        {
            3520, //A7
            2093, //C7
            2349, //D7
            2637, //E7
        },
    };
    //lint +e26

    int octave = 0;
    OCTAVE_T demoOctave = OCTAVE_SIXTH;
    ERROR_T err = ERR_NONE;

    if( g_ParmCount >= 2 )
    {
        octave = atoi( g_ParsedMsg[1] );

        switch( octave )
        {
            case 3:
                demoOctave = OCTAVE_THIRD;
                break;
            case 4:
                demoOctave = OCTAVE_FOURTH;
                break;
            case 5:
                demoOctave = OCTAVE_FIFTH;
                break;
            case 6:
                demoOctave = OCTAVE_SIXTH;
                break;
            case 7:
                demoOctave = OCTAVE_SEVENTH;
                break;
            default:
                ( void )SM_SendF( "%s", "Invalid Parameter. Must be 3 through 7.\r\n" );
                err = ERR_PARAMETER;
                break;
        }

        if( err == ERR_NONE )
        {
            // Power On
            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][C_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            err = DAC_Enable( true );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( AM_TASK_TONE_TIME );

            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][D_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( AM_TASK_TONE_TIME );

            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][E_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( AM_TASK_TONE_TIME );

            err = DAC_Enable( false );
            CLI_Trace( source_, err, __LINE__ );

            ( void )OSTimeDlyHMSM( ONE_SECOND );

            // Alert
            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][E_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            err = DAC_Enable( true );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( AM_TASK_TONE_TIME );

            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][E_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( AM_TASK_TONE_TIME );

            err = DAC_Enable( false );
            CLI_Trace( source_, err, __LINE__ );

            ( void )OSTimeDlyHMSM( ONE_SECOND );

            // Alarm
            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][C_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            err = DAC_Enable( true );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( AM_TASK_TONE_TIME );

            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][C_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( AM_TASK_TONE_TIME );

            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][C_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( AM_TASK_TONE_TIME );

            err = DAC_Enable( false );
            CLI_Trace( source_, err, __LINE__ );

            ( void )OSTimeDlyHMSM( ONE_SECOND );

            // Button Tone
            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][A_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            err = DAC_Enable( true );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( BUTTON_TONE_TIME );

            err = DAC_Enable( false );
            CLI_Trace( source_, err, __LINE__ );

            ( void )OSTimeDlyHMSM( ONE_SECOND );

            // Power Off
            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][E_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            err = DAC_Enable( true );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( AM_TASK_TONE_TIME );
            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][D_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( AM_TASK_TONE_TIME );
            err = DAC_SetFrequency( ( uint16_t ) freqArray[demoOctave][C_NOTE] );
            CLI_Trace( source_, err, __LINE__ );
            ( void )OSTimeDlyHMSM( AM_TASK_TONE_TIME );
            err = DAC_Enable( false );
            CLI_Trace( source_, err, __LINE__ );
        }
    }
    else
    {
        ( void )SM_SendF( "%s", "Invalid Command \r\n" );
    }

#undef ONE_SECOND
}

////////////////////////////////////////////////////////////////////////////////
void CLI_MTR_FaultHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    bool fault = false;

    err = MTR_GetFaultStatus( &fault );

    if( err != ERR_NONE )
    {
        CLI_Trace( source_, err, __LINE__ );
    }
    else
    {
        ( void )SM_SendF( "%s %s \r\n", "Motor Fault: ", fault ? "true" : "false" );
    }
}

#if defined( FDEBUG )
////////////////////////////////////////////////////////////////////////////////
void CLI_MTR_DeltaLogHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    err = MTR_OutputLogs();

    CLI_Trace( source_, err, __LINE__ );

}

#endif

////////////////////////////////////////////////////////////////////////////////
void CLI_LCD_Line_Cmd_Handler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    int line;
    int data;
    uint8_t data8;
    uint8_t data8arr[DATA_BYTES_LENGTH];

    if( g_ParmCount == 3 )
    {
        err =  SPI_EnableLCD( true );
        CLI_Trace( source_, err, __LINE__ );

        line = atoi( g_ParsedMsg[1] );
        data = atoi( g_ParsedMsg[2] );

        if( line > NUM_OF_LCD_LINES )
        {
            ( void ) SM_SendF( "/tMaximum line value is %u", (uint32_t)NUM_OF_LCD_LINES );
        }
        else
        {
            data8 = (uint8_t)data; ///The LCD screen writes data for low bits. This is not taken care of in LCD, but rather in GUI.
                                   ///TODO: Move the bit wise not function to the LCD driver.
            memset( data8arr, data8, sizeof(data8arr) );

            err = SPI_SLineDisplayUpdate( (uint8_t)line, data8arr );
            CLI_Trace( source_, err, __LINE__ );

            err = SPI_EndSend();
            CLI_Trace( source_, err, __LINE__ );
        }
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_LCD_Clear_Cmd_Handler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    err = SPI_DisplayClear();

    CLI_Trace( source_, err, __LINE__ );

}

//////////////////////////////////////////////////////////////////////////////////
void CLI_EnableScreen_Cmd_Handler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    bool statusFlag = false;

    if( g_ParmCount == 2 )
    {
        if( strcmp( g_ParsedMsg[1], "t" ) == 0 )
        {
            err = SPI_EnableLCD( true );
            CLI_Trace( source_, err, __LINE__ );

            err = SPI_GetLCD_EnableStatus( &statusFlag );
            CLI_Trace( source_, err, __LINE__ );

            if( err == ERR_NONE )
            {
                ( void ) SM_SendF( "%s %s %s", "The LCD is currently ", ( statusFlag ? "ON" : "OFF" ), "\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "f" ) == 0 )
        {
            err = SPI_EnableLCD( false );
            CLI_Trace( source_, err, __LINE__ );

            err = SPI_GetLCD_EnableStatus( &statusFlag );
            CLI_Trace( source_, err, __LINE__ );

            if( err == ERR_NONE )
            {
                ( void ) SM_SendF( "%s %s %s", "The LCD is currently ", ( statusFlag ? "ON" : "OFF" ), "\r\n" );
            }
        }
    }
    else
    {
        err = SPI_GetLCD_EnableStatus( &statusFlag );
        CLI_Trace( source_, err, __LINE__ );
        ( void ) SM_SendF( "%s %s %s", "The LCD is currently ", ( statusFlag ? "ON" : "OFF" ), " \r\n" );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_DAC_SetWaveHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    int amplitude = 0;
    int frequency = 0;

    if( g_ParmCount == 3 )
    {

        amplitude = atoi( g_ParsedMsg[1] );
        frequency = atoi( g_ParsedMsg[2] );

        if( ( amplitude != 0 ) &&
            ( frequency != 0 ) )
        {
            err = DAC_SetAmplitude( ( uint32_t )amplitude );
            CLI_Trace( source_, err, __LINE__ );

            err = DAC_SetFrequency( ( uint16_t )frequency );
            CLI_Trace( source_, err, __LINE__ );

            err = DAC_Enable( true );
            CLI_Trace( source_, err, __LINE__ );
        }
        else
        {
            err = DAC_Enable( false );
            CLI_Trace( source_, err, __LINE__ );
        }


    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }

}

////////////////////////////////////////////////////////////////////////////////
void CLI_SimulateEndOfInfusionHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    if( g_ParmCount == 2 )
    {
        // TODO: Handle HY and IG seperately. As of now its just a place holder
        if( ( strcmp( g_ParsedMsg[1], "hy" ) == 0 ) || ( strcmp( g_ParsedMsg[1], "ig" ) ) )
        {
            ( void )INF_StimulateEndOfInfusion();
        }
        else
        {
            CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
        }
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_ReadPackCurrentCmdHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    float current;
    ERROR_T status = ERR_NONE;

    if( g_ParmCount == 1 )
    {
        status = BATT_ReadPackCurrent( &current );
        if( status == ERR_NONE )
        {
            ( void ) SM_SendF( "Pack Current -- > %0.2f  mA\r\n", current );
        }
        else
        {
            CLI_Trace( source_, status, __LINE__ );
        }
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_ReadCellVoltageCmdHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    float voltage;
    ERROR_T status = ERR_NONE;

    if( g_ParmCount == 1 )
    {
        status = BATT_ReadCellVoltage( &voltage );
        if( status == ERR_NONE )
        {
            ( void ) SM_SendF( "Cell Voltage -- > %0.2f  V\r\n", voltage );
        }
        else
        {
            CLI_Trace( source_, status, __LINE__ );
        }
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_ReadTemperatureCmdHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    float temperature;
    ERROR_T status = ERR_NONE;

    if( g_ParmCount == 1 )
    {
        status = BATT_ReadTemperature( &temperature );
        if( status == ERR_NONE )
        {
            ( void ) SM_SendF( "Temperature -- > %0.2f  C\r\n", temperature );
        }
        else
        {
            CLI_Trace( source_, status, __LINE__ );
        }
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_ReadBatteryPercentageHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    uint8_t percentage;
    ERROR_T status = ERR_NONE;

    if( g_ParmCount == 1 )
    {
        status = BATT_GetBatteryPercentage( &percentage );
        if( status == ERR_NONE )
        {
            ( void ) SM_SendF( "Battery -- > %d %\r\n", percentage );
        }
        else
        {
            CLI_Trace( source_, status, __LINE__ );
        }
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_SetToneSeriesHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status = ERR_NONE;
    unsigned int eventParm = 0;
    uint8_t volume = 0;

    if( g_ParmCount == 3 )
    {
        if( strcmp( g_ParsedMsg[1], "event" ) == 0 )
        {
            eventParm = ( unsigned int )atoi( g_ParsedMsg[2] );

            s_AmMessage.EventType = ALERT;
            s_AmMessage.Alert = ( AUDIO_NOTIFICATION_T )eventParm;

            status = AM_CommandNotify( &s_AmMessage );

            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "Tone Played\r\n" );
            }
            else
            {
                CLI_Trace( source_, status, __LINE__ );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "vol" ) == 0 )
        {
            volume = ( uint8_t )atoi( g_ParsedMsg[2] );

            s_AmMessage.EventType = VOLUME;
            s_AmMessage.Volume = volume;
            s_AmMessage.SaveVolFlag = true;

            status = AM_CommandNotify( &s_AmMessage );

            if( status == ERR_NONE )
            {
                ( void ) SM_SendF( "Volume changed\r\n" );
            }
            else
            {
                CLI_Trace( source_, status, __LINE__ );
            }
        }
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_AlarmAlertHandler ( SERIAL_CMD_SOURCE_T source_ )
{
#define DISP_USAGE ( void ) SM_SendF( \
                                     "Usage- aa 0: Output queue contents (alarm id) in priority order.\r\n\taa 1 #: Set alarm id #.\r\n\taa 2 #: Clear alarm id #\r\n" )
    ERROR_T status = ERR_NONE;
    ALRM_ALRT_NOTI_T eventParm = ALL_CLEAR;
    unsigned int cmdParm = 0;
    char sBuf[100];

    if( g_ParmCount == 3 )
    {
        cmdParm = ( unsigned int )atoi( g_ParsedMsg[1] );
        eventParm = ( ALRM_ALRT_NOTI_T )atoi( g_ParsedMsg[2] );

        switch( cmdParm )
        {
            case 1: // Set
                status = AAM_Set( eventParm );
                break;
            case 2: // Clear
                status = AAM_Clear( eventParm );
                break;
            case 0: // Query
            default:
                DISP_USAGE;
                break;
        }

        if( status == ERR_NONE )
        {
            ( void ) SM_SendF( "Alarm Message Set\r\n" );
        }
        else
        {
            CLI_Trace( source_, status, __LINE__ );
        }

    }
    else if( g_ParmCount == 2 )
    {
        cmdParm = ( unsigned int )atoi( g_ParsedMsg[1] );

        if( cmdParm == 0 )
        {
            // Query
            ( void )AAM_Query( sBuf, sizeof(sBuf) );
            ( void ) SM_SendF( sBuf );
        }
        else
        {
            DISP_USAGE;
        }
    }
    else
    {
        DISP_USAGE;
    }
#undef DISP_USAGE
}

////////////////////////////////////////////////////////////////////////////////
void CLI_ReadProgramHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status = ERR_NONE;
    PROGRAM_INFO_T progInfo;

    if( g_ParmCount == 1 )
    {
        memset( &progInfo, 0, sizeof(progInfo) );
        status = PM_GetProgramInfo( &progInfo );
        CLI_Trace( source_, status, __LINE__ );

        if( status == ERR_NONE ) 
        {
            ( void ) SM_SendF( "Dosage             : %0.1f\r\n", progInfo.ProgDosage );
            ( void ) SM_SendF( "Needle             : %s\r\n", progInfo.ProgNeedleSet == SINGLE_NEEDLE_LINESET ? "Single Needle" : "Bifurcated Needle" );
            ( void ) SM_SendF( "IG Interval        : %d\r\n", progInfo.ProgIgTimeInterval );
            ( void ) SM_SendF( "Total IG Steps     : %d\r\n", progInfo.ProgIgSteps );
            for( uint8_t step = 0; step <= progInfo.ProgIgSteps; step++ )
            {
                if( step == HY_STEP_INDEX )
                {
                    ( void ) SM_SendF( "HY Step            : %02d:%02d @ %3d ml/hr/site\r\n", progInfo.ProgStepTime[step].Hour, progInfo.ProgStepTime[step].Min, RATE_VALUE_ARR[progInfo.ProgStepFlowRate[step]]);
                }
                else
                {
                    ( void ) SM_SendF( "IG Step%d           : %02d:%02d @ %3d ml/hr/site\r\n", step,progInfo.ProgStepTime[step].Hour, progInfo.ProgStepTime[step].Min, RATE_VALUE_ARR[progInfo.ProgStepFlowRate[step]]);
                }
            }
            ( void ) SM_SendF( "Rate               : %s\r\n", progInfo.RateLocked ? "Locked" : "Not Locked" );
            ( void ) SM_SendF( "NeedleCheck        : %s\r\n", progInfo.NeedlePlacementCheckEnabled ? "Enabled" : "Disabled" );
            ( void ) SM_SendF( "Flush              : %s\r\n", progInfo.FlushEnabled ? "Enabled" : "Disabled" );
        }
        else
        {
            ( void ) SM_SendF( "Failed to retrieve program from file system\r\n" );
        }
    }
    else
    {
        ( void ) SM_SendF( "Invalid Command Parameter\r\n" );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_MoveToTDC_Handler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    err = MTR_MoveToTDC();

    CLI_Trace( source_, err, __LINE__ );
}

////////////////////////////////////////////////////////////////////////////////
void CLI_SetMinTimerHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    int sec = 0;

    if( g_ParmCount == 2 )
    {
        sec = atoi( g_ParsedMsg[1] );

        err = INF_ChangeMinTimer( sec );
        CLI_Trace( source_, err, __LINE__ );
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }

}

////////////////////////////////////////////////////////////////////////////////
void CLI_RetrieveRPMhandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;
    float rpm = 0.0;

    if( g_ParmCount == 1 )
    {
        err = MTR_GetRPM( &rpm );
        CLI_Trace( source_, err, __LINE__ );

        ( void ) SM_SendF( "RPM: %2.2f\r\n", rpm );
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_RTChandler ( SERIAL_CMD_SOURCE_T source_  )
{
    ERROR_T err = ERR_NONE;
    RTC_TIMESTAMP_T dateTime;

    if( g_ParmCount == 1 )
    {
        err = RTC_GetDateTime( &dateTime );
        CLI_Trace( source_, err, __LINE__ );
        ( void ) SM_SendF( "\tYear : %04d\r\n", dateTime.Yr    );
        ( void ) SM_SendF( "\tMonth: %02d\r\n", dateTime.Month );
        ( void ) SM_SendF( "\tDate : %02d\r\n", dateTime.Date  );
        ( void ) SM_SendF( "\tHour : %02d\r\n", dateTime.Hr    );
        ( void ) SM_SendF( "\tMin  : %02d\r\n", dateTime.Min   );
        ( void ) SM_SendF( "\tSec  : %02d\r\n", dateTime.Sec   );
        ( void ) SM_SendF( "\tMsec : %03d\r\n", dateTime.Msec  );
    }
    else
    {
        CLI_Trace( source_, ERR_PARAMETER, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void CLI_CreateLangFile ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T err = ERR_NONE;

    if( source_ == SERIAL_USB )
    {
        // Disable USB so the MSC no longer monopolizes the file system.
        err = USB_InterfaceDisable();
        CLI_Trace( source_, err, __LINE__ );
    }

    if( g_ParmCount == 1 )
    {
        err = LM_CreateEnglishFile();
        CLI_Trace( source_, err, __LINE__ );
    }
    else
    {
        err = ERR_PARAMETER;
    }

    if( source_ == SERIAL_USB )
    {
        // Restore USB now that the file system is available.
        err = USB_InterfaceEnable();
        CLI_Trace( source_, err, __LINE__ );
    }

    if( err == ERR_NONE )
    {
        ( void ) SM_SendF( "Language file created successfully\r\n" );
    }
    else
    {
        CLI_Trace( source_, err, __LINE__ );
    }
}

#if defined( FDEBUG )
////////////////////////////////////////////////////////////////////////////////
void CLI_SmOverFlow_Count ( SERIAL_CMD_SOURCE_T source_ )
{
    int cnt;

    SM_PrintBuffCnt( &cnt );
    (void)SM_SendF( "Buffer OverFlow Count: %d \r\n", cnt );
}

#endif

////////////////////////////////////////////////////////////////////////////////
static void CLI_Trace ( SERIAL_CMD_SOURCE_T source_, ERROR_T errMsg_, int line_ )
{
#if defined( FDEBUG )
    if( ( errMsg_ < NUM_ERROR_CODES ) &&
        ( errMsg_ > ERR_NONE ) )
    {
        ( void )SM_SendF( "%s %s %s %s %d \r\n", S_ERR_STRINGS[errMsg_], "In File", __FILENAME__, "Line: ", line_ );
        //used as debugging tool, we can ignore it.
    }
    else if( errMsg_ != ERR_NONE )
    {
        ( void )SM_SendF( "Trace failed. Error out of bounds \r\n" );
    }
#endif
}

