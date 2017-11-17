//////////////////////////////////////////////////////////////////////////////
///
///                Copyright(c) 2017 FlexMedical
///
///                   *** Confidential Company Proprietary ***
///
/// \file CLI_ProgramCreate.c
///
/// \brief CLI to create program
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

// User
#include "CLI.h"
#include "CLI_Commands.h"
#include "CLI_ProgramCreate.h"
#include "ConfigurationManager.h"
#include "CommonTypes.h"
#include "OcclusionManager.h"
#include "ProgramManager.h"

////////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND #DEFINES
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ENUMS, TYPEDEFS, AND STRUCTURES
////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Local Variables
///////////////////////////////////////////////////////////////////////////////

static PROGRAM_INFO_T progInfo;
static bool isProgTypeSet = false;

//lint -e785 Missing parameters for initialization. For better readibility, do not want to show them
//lint -e26 Lint is confused by the explicit indexing. '[' is part of the expression.
//lint -e485 There is no duplicate initialization here.
//lint -e651 There is no confusion intialization
// Default Parameters for Custom Program
static const PROGRAM_INFO_T s_DefaultCustomProgram =
{
    .ProgType = CUSTOM,
    .ProgNeedleSet = SINGLE_NEEDLE_LINESET,
    .ProgAvailableF = true,
    .ProgDosage =50.0,
    .ProgIgSteps = 5,
    .ProgStepFlowRate[HY_STEP_INDEX] = ML_60,
    .ProgStepFlowRate[1] = ML_10,
    .ProgStepFlowRate[2] = ML_30,
    .ProgStepFlowRate[3] = ML_120,
    .ProgStepFlowRate[4] = ML_240,
    .ProgStepFlowRate[5] = ML_300,
    .ProgStepTime[0] = { 0,  0},    // HY Step Time Needs to be calculated
    .ProgStepTime[1] = { 0,  5},    // IG Step 1 Time
    .ProgStepTime[2] = { 0, 10},    // IG Step 2 Time
    .ProgStepTime[3] = { 0, 15},    // IG Step 3 Time
    .ProgStepTime[4] = { 0, 20},    // IG Step 4 Time
    .ProgStepTime[5] = { 0,  0},    // IG Step 5 Needs to be calculated
    .RateLocked = NO,
    .FlushEnabled = YES,
    .NeedlePlacementCheckEnabled = YES,
};

// Default Parameters for HyQvia Template Program
// Default Weight and RampUp Parameters
static PATIENT_WEIGHT_T s_weight = GREATER_88;
static INFUSION_RAMPUP_T s_Rampup = FIRST_TWO_INFUSIONS;
static const PROGRAM_INFO_T s_DefaultTemplateProgram =
{
    .ProgType = HYQVIA_TEMPLATE,
    .ProgNeedleSet = SINGLE_NEEDLE_LINESET,
    .ProgAvailableF = true,
    .ProgDosage = 50.0,
    .ProgStepFlowRate[HY_STEP_INDEX] = ML_60,
    .ProgIgTimeInterval = 15,
    .RateLocked = NO,
    .FlushEnabled = YES,
    .NeedlePlacementCheckEnabled = YES,
};
//lint +e485
//lint +e26
//lint +e785
//lint +e651

///////////////////////////////////////////////////////////////////////////////
// Prototypes
///////////////////////////////////////////////////////////////////////////////

static void CLI_ResetData( void );
static void CLI_ProgramCreatHelp( SERIAL_CMD_SOURCE_T source_ );
static ERROR_T CLI_ProgramReview( SERIAL_CMD_SOURCE_T source_, PROGRAM_INFO_T *programInfo_, bool isDebugF_, bool adjustProgram_ );

///////////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
void CLI_CreateProgramHandler ( SERIAL_CMD_SOURCE_T source_ )
{
    ERROR_T status = ERR_NONE;

    if( strcmp( g_ParsedMsg[1], "igflow" ) == 0 )
    {
        if( isProgTypeSet == true )
        {
            if( progInfo.ProgType == CUSTOM )
            {
                if( g_ParmCount == 2 + progInfo.ProgIgSteps )
                {
                    for( int i = 0; i < progInfo.ProgIgSteps; i++ )
                    {
                        RATE_VALUE_T flowRate;
                        status = PM_ConvertFlowrateIntToEnum( (uint16_t)atoi( g_ParsedMsg[2 + i] ), &flowRate );
                        if( status == ERR_NONE )
                        {
                            status = PM_ValidateIgFlowrate( flowRate, SINGLE_NEEDLE_LINESET );
                            if( status == ERR_NONE )
                            {
                                progInfo.ProgStepFlowRate[IG_STEP_START_INDEX + i] = flowRate;
                            }
                            else
                            {
                                ( void ) SM_SendF( "!!!!!!!! Invalid Step %d Flowrate value !!!!!!!!\r\n", i + 1 );
                                break;
                            }
                        }
                        else
                        {
                            ( void ) SM_SendF( "!!!!!!!! Invalid Step %d Flowrate value !!!!!!!!\r\n", i + 1 );
                            break;
                        }
                    }
                    if( status == ERR_NONE )
                    {
                        if( progInfo.ProgIgSteps == 1 )
                        {
                            ( void ) SM_SendF( "!!!!!!! Program has only one step !!!!!!!! \r\n" );
                        }
                        else
                        {
                            ( void ) SM_SendF( "!!!!!!! IG Flowrates Updated Successfully !!!!!!!\r\n" );
                            ( void ) SM_SendF(
                                              "**** Now set IG Duration by : progc set igtime [step1] [step2] ... [stepN-1] ****\r\n" );
                        }
                    }
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Invalid Parameter !!!!!!!\r\n" );
                    ( void ) SM_SendF( "!!!!!!! Required flowrate for %d step but entered command is for %d step !!!!!!!\r\n", progInfo.ProgIgSteps, ( g_ParmCount - 2 ) );
                }
            }
            else
            {
                ( void ) SM_SendF( "!!!!!!!! Not required To Set In Template Program !!!!!!!!\r\n" );
            }
        }
        else
        {
            ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
        }
    }
    else if( strcmp( g_ParsedMsg[1], "igtime" ) == 0 )
    {
        if( isProgTypeSet == true )
        {
            if( progInfo.ProgType == CUSTOM )
            {
                if( g_ParmCount == 2 + ( progInfo.ProgIgSteps - 1 )  )
                {
                    for( int i = 0; i < ( progInfo.ProgIgSteps - 1 ); i++ )
                    {
                        INFUSION_STEP_TIME_T stepTime;
                        stepTime.Hour = 0;
                        stepTime.Min = (uint8_t)atoi( g_ParsedMsg[2 + i] );
                        status = PM_ValidateIgStepDuration( &stepTime );
                        if( status == ERR_NONE )
                        {
                            progInfo.ProgStepTime[IG_STEP_START_INDEX + i].Hour = stepTime.Hour;
                            progInfo.ProgStepTime[IG_STEP_START_INDEX + i].Min = stepTime.Min;
                        }
                        else
                        {
                            ( void ) SM_SendF( "!!!!!!!! Invalid IG Step %d Time value !!!!!!!!\r\n", i + 1 );
                            break;
                        }
                    }
                    if( status == ERR_NONE )
                    {
                        ( void ) SM_SendF( "!!!!!!! IG Step Duration Updated Successfully !!!!!!!\r\n" );
                    }
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Invalid Parameter !!!!!!!\r\n" );
                    ( void ) SM_SendF( "!!!!!!! Make sure to not enter time for last IG step !!!!!!!\r\n" );
                    ( void ) SM_SendF( "!!!!!!! Required IG stpe duration for %d step but entered command is for %d step !!!!!!!\r\n", ( progInfo.ProgIgSteps - 1), ( g_ParmCount - 2 ) );
                }
            }
            else
            {
                ( void ) SM_SendF( "!!!!!!!! Not required To Set In Template Program !!!!!!!!\r\n" );
            }
        }
        else
        {
            ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
        }
    }
    else if( g_ParmCount == 3 )
    {
        if( strcmp( g_ParsedMsg[1], "type" ) == 0 )
        {
            if( strcmp( g_ParsedMsg[2], "custom" ) == 0 )
            {
                // Reset Local Structure first
                CLI_ResetData();
                isProgTypeSet = true;

                ( void ) SM_SendF( "**** Applied Default Parameters ****\r\n" );
                // Create Default Program
                memcpy( &progInfo, &s_DefaultCustomProgram, sizeof( PROGRAM_INFO_T ) );
                status = CLI_ProgramReview( source_, &progInfo, true, true );
                if( status != ERR_NONE )
                {
                    ( void ) SM_SendF( "!!!!!!! Error in Default Program Definition !!!!!!! \r\n" );
                }
                ( void ) SM_SendF( "**** Save these settings or change individual parameter if needed ****\r\n" );
                ( void ) SM_SendF( "**** Save the program by : progc save ****\r\n" );
            }
            else if( strcmp( g_ParsedMsg[2], "template" ) == 0 )
            {
                // Reset Local Structure first
                CLI_ResetData();
                isProgTypeSet = true;

                ( void ) SM_SendF( "**** Applied Default Parameters ****\r\n" );
                // Create Default Program
                memcpy( &progInfo, &s_DefaultTemplateProgram, sizeof( PROGRAM_INFO_T ) );
                status = CLI_ProgramReview( source_, &progInfo, true, true );
                if( status != ERR_NONE )
                {
                    ( void ) SM_SendF( "!!!!!!! Error in Default Program Definition !!!!!!! \r\n" );
                }
                ( void ) SM_SendF( "**** Save this settings or change individual parameter if needed ****\r\n" );
                ( void ) SM_SendF( "**** Save the program by : progc save ****\r\n" );
            }
            else
            {
                ( void ) SM_SendF( "!!!!!!! Invalid Parameter !!!!!!!\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "needle" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                if( strcmp( g_ParsedMsg[2], "single" ) == 0 )
                {
                    progInfo.ProgNeedleSet = SINGLE_NEEDLE_LINESET;
                    ( void ) SM_SendF( "!!!!!!! Needle Updated Successfully !!!!!!!\r\n" );
                }
                else if( strcmp( g_ParsedMsg[2], "bifurcated" ) == 0 )
                {
                    progInfo.ProgNeedleSet = BIFURCATED_NEEDLE_LINESET;
                    ( void ) SM_SendF( "!!!!!!! Needle Updated Successfully !!!!!!!\r\n" );
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Invalid Parameter !!!!!!!\r\n" );
                }
            }
            else
            {
                ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "weight" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                if( progInfo.ProgType == HYQVIA_TEMPLATE )
                {
                    if( strcmp( g_ParsedMsg[2], "above88" ) == 0 )
                    {
                        s_weight = GREATER_88;
                        ( void ) PM_PopulateAndCalculateHyQviaProgInfo( s_weight, s_Rampup, &progInfo );
                        ( void ) SM_SendF( "!!!!!!! Patient Weight Updated Successfully !!!!!!!\r\n" );
                    }
                    else if( strcmp( g_ParsedMsg[2], "below88" ) == 0 )
                    {
                        s_weight = LESS_EQUAL_88;
                        ( void ) PM_PopulateAndCalculateHyQviaProgInfo( s_weight, s_Rampup, &progInfo );
                        ( void ) SM_SendF( "!!!!!!! Patient Weight Updated Successfully !!!!!!!\r\n" );
                    }
                    else
                    {
                        ( void ) SM_SendF( "!!!!!!! Invalid Parameter !!!!!!!\r\n" );
                    }
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Not required to set in custom program !!!!!!! \r\n" );
                }
            }
            else
            {
                ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "rampup" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                if( progInfo.ProgType == HYQVIA_TEMPLATE )
                {
                    if( strcmp( g_ParsedMsg[2], "first2" ) == 0 )
                    {
                        s_Rampup = FIRST_TWO_INFUSIONS;
                        ( void ) PM_PopulateAndCalculateHyQviaProgInfo( s_weight, s_Rampup, &progInfo );
                        ( void ) SM_SendF( "!!!!!!! Infusion Rampup Updated Successfully !!!!!!!\r\n" );
                    }
                    else if( strcmp( g_ParsedMsg[2], "ongoing" ) == 0 )
                    {
                        s_Rampup = ONGOING_INFUSIONS;
                        ( void ) PM_PopulateAndCalculateHyQviaProgInfo( s_weight, s_Rampup, &progInfo );
                        ( void ) SM_SendF( "!!!!!!! Infusion Rampup Updated Successfully !!!!!!!\r\n" );
                    }
                    else
                    {
                        ( void ) SM_SendF( "!!!!!!! Invalid Parameter !!!!!!!\r\n" );
                    }
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Not required to set in custom program !!!!!!! \r\n" );
                }
            }
            else
            {
                ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "dosage" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                float doasage;
                doasage = (float)atof( g_ParsedMsg[2] );
                status =  PM_ValidateDosage( doasage );
                if( status == ERR_NONE )
                {
                    progInfo.ProgDosage = doasage;
                    ( void ) SM_SendF( "!!!!!!! Dosage Updated Successfully !!!!!!!\r\n" );
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Invalid Dosage Value !!!!!!!!!\r\n" );
                }
            }
            else
            {
                ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "hyflow" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                RATE_VALUE_T flowRate;
                status = PM_ConvertFlowrateIntToEnum( (uint16_t)atoi( g_ParsedMsg[2] ), &flowRate );
                if( status == ERR_NONE )
                {
                    if( PM_ValidateHyFlowrate( flowRate, SINGLE_NEEDLE_LINESET ) == ERR_NONE )
                    {
                        progInfo.ProgStepFlowRate[0] = flowRate;
                        ( void ) SM_SendF( "!!!!!!! HY Flowrate Updated Successfully !!!!!!!\r\n" );
                    }
                    else
                    {
                        ( void ) SM_SendF( "!!!!!!! Invalid HY FlowRate Value !!!!!!!!!\r\n" );
                    }
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Invalid HY FlowRate Value !!!!!!!!!\r\n" );
                }
            }
            else
            {
                ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "interval" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                if( progInfo.ProgType == HYQVIA_TEMPLATE )
                {
                    uint8_t interval = (uint8_t)atoi( g_ParsedMsg[2] );
                    if( PM_ValidateIgTimeInterval( interval ) == ERR_NONE )
                    {
                        progInfo.ProgIgTimeInterval = interval;
                        ( void ) SM_SendF( "!!!!!!! IG Interval Updated Successfully !!!!!!!\r\n" );
                    }
                    else
                    {
                        ( void ) SM_SendF( "!!!!!!! Invalid IG Interval Value !!!!!!!!!\r\n" );
                    }
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Not required to set in custom program !!!!!!! \r\n" );
                }
            }
            else
            {
                ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "igstep" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                if( progInfo.ProgType == CUSTOM )
                {
                    uint8_t step;
                    step = (uint8_t)atoi( g_ParsedMsg[2] );
                    // Validate Step
                    status = PM_ValidateIgSteps( step );
                    if( status == ERR_NONE )
                    {
                        if( step != progInfo.ProgIgSteps )
                        {
                            // If User Increased the steps
                            if( step > progInfo.ProgIgSteps )
                            {
                                //If User decreased the steps
                                for( uint8_t i = progInfo.ProgIgSteps+1; i<= step; i++ )
                                {
                                    progInfo.ProgStepFlowRate[i] = ML_0;
                                    progInfo.ProgStepTime[i].Hour = 0;
                                    progInfo.ProgStepTime[i].Min = 0;
                                }
                                ( void ) SM_SendF(
                                                      "**** Now set IG Flowrate : progc set igflow [step1] [step2] ... [stepN] ****\r\n" );
                            }
                            else if( step < progInfo.ProgIgSteps )
                            {
                                //If User decreased the steps
                                for( uint8_t i = step+1; i<= progInfo.ProgIgSteps; i++ )
                                {
                                    progInfo.ProgStepFlowRate[i] = ML_0;
                                    progInfo.ProgStepTime[i].Hour = 0;
                                    progInfo.ProgStepTime[i].Min = 0;
                                }
                            }
                        }
                        progInfo.ProgIgSteps = step;
                        ( void ) SM_SendF( "!!!!!!! IG Steps Updated Successfully !!!!!!!\r\n" );
                    }
                    else
                    {
                        ( void ) SM_SendF( "!!!!!!! Invalid IG Step Value !!!!!!!!!\r\n" );
                    }
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Not required to set in Template Program!!!!!!! \r\n" );
                }
            }
            else
            {
                ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "lockrate" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                if( strcmp( g_ParsedMsg[2], "yes" ) == 0 )
                {
                    progInfo.RateLocked = true;
                    ( void ) SM_SendF( "!!!!!!! Rates are Locked Successfully !!!!!!!\r\n" );
                }
                else if( strcmp( g_ParsedMsg[2], "no" ) == 0 )
                {
                    progInfo.RateLocked = false;
                    ( void ) SM_SendF( "!!!!!!! Rates are UnLocked Successfully !!!!!!!\r\n" );
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Invalid Parameter !!!!!!!\r\n" );
                }
            }
            else
            {
                ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "needlecheck" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                if( strcmp( g_ParsedMsg[2], "enable" ) == 0 )
                {
                    progInfo.NeedlePlacementCheckEnabled = true;
                    ( void ) SM_SendF( "!!!!!!! Needle Check Parameter Updated Successfully !!!!!!!\r\n" );
                }
                else if( strcmp( g_ParsedMsg[2], "disable" ) == 0 )
                {
                    progInfo.NeedlePlacementCheckEnabled = false;
                    ( void ) SM_SendF( "!!!!!!! Needle Check Parameter Updated Successfully !!!!!!!\r\n" );
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Invalid Parameter !!!!!!!\r\n" );
                }
            }
            else
            {
                ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "flush" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                if( strcmp( g_ParsedMsg[2], "enable" ) == 0 )
                {
                    progInfo.FlushEnabled = true;
                    ( void ) SM_SendF( "!!!!!!! Flush Parameter Updated Successfully !!!!!!!\r\n" );
                }
                else if( strcmp( g_ParsedMsg[2], "disable" ) == 0 )
                {
                    progInfo.FlushEnabled = false;
                    ( void ) SM_SendF( "!!!!!!! Flush Parameter Updated Successfully !!!!!!!\r\n" );
                }
                else
                {
                    ( void ) SM_SendF( "!!!!!!! Invalid Parameter !!!!!!!\r\n" );
                }
            }
            else
            {
                ( void ) SM_SendF( "**** First Set Program Type by: progc set type [custom/template] ****\r\n" );
            }
        }
        else
        {
            ( void ) SM_SendF( "!!!!!!!! Invalid command parameter !!!!!!! \r\n" );
            ( void ) SM_SendF( " **** Get list of supported commands by: progc help **** \r\n" );
        }
    }
    else if( g_ParmCount == 2 )
    {
        if( strcmp( g_ParsedMsg[1], "review" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                ( void ) CLI_ProgramReview( source_, &progInfo, true, false );
            }
            else
            {
                ( void ) SM_SendF( "First Create Program by: progc set type [custom/template]\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "save" ) == 0 )
        {
            if( isProgTypeSet == true )
            {
                status = CLI_ProgramReview( source_, &progInfo, false, true );
                if( status != ERR_NONE )
                {
                    ( void ) SM_SendF( "!!!!!!! Error in Program Definition !!!!!!! \r\n" );
                    // Display Current Program Settings For Better Understanding
                    ( void ) CLI_ProgramReview( source_, &progInfo, true, false );
                }
                if( status == ERR_NONE )
                {
                    if( PM_SetProgramInfo( &progInfo, progInfo.ProgType ) == ERR_NONE )
                    {
                        ( void ) SM_SendF( "\r\n !!!!!!!!  Program is ready to use from flash !!!!!!!!! \r\n" );
                    }
                    else
                    {
                        ( void ) SM_SendF( "!!!!!!!! Error in saving program to flash !!!!!!!\r\n" );
                    }
                }
            }
            else
            {
                ( void ) SM_SendF( "First Create Program by: progc set type [custom/template]\r\n" );
            }
        }
        else if( strcmp( g_ParsedMsg[1], "help" ) == 0 )
        {
            CLI_ProgramCreatHelp( source_ );
        }
        else if( strcmp( g_ParsedMsg[1], "?" ) == 0 )
        {
            CLI_ProgramCreatHelp( source_ );
        }
        else
        {
            ( void ) SM_SendF( "!!!!!!!! Invalid command parameter !!!!!!! \r\n" );
            ( void ) SM_SendF( " **** Get list of supported commands by: progc help **** \r\n" );
        }
    }
    else if( g_ParmCount == 1 )
    {
        CLI_ProgramCreatHelp( source_ );
    }
    else
    {
        ( void ) SM_SendF( "!!!!!!!! Invalid command parameter !!!!!!! \r\n" );
        ( void ) SM_SendF( " **** Get list of supported commands by: progc help **** \r\n" );
    }
}

////////////////////////////////////////////////////////////////////////////////

static void CLI_ResetData ( void )
{
    // Fill with default data
    memset( &progInfo, 0, sizeof(progInfo) );
    isProgTypeSet = false;
    s_weight = GREATER_88;
    s_Rampup = FIRST_TWO_INFUSIONS;
}

////////////////////////////////////////////////////////////////////////////////

static ERROR_T CLI_ProgramReview( SERIAL_CMD_SOURCE_T source_, PROGRAM_INFO_T *programInfo_, bool isDebugF_, bool adjustProgram_ )
{
    ERROR_T status =  ERR_NONE;

    if( programInfo_ == NULL )
    {
        return ERR_PARAMETER;
    }

    if( adjustProgram_ == true )
    {
        if( programInfo_->ProgType == CUSTOM )
        {
            status =  PM_PopulateAndCalculateCustomProgInfo( programInfo_ );
            if( status != ERR_NONE )
            {
                 return status;
            }
        }
        else if( programInfo_->ProgType == HYQVIA_TEMPLATE )
        {
            status = PM_PopulateAndCalculateHyQviaProgInfo( s_weight, s_Rampup, programInfo_ );
            if( status != ERR_NONE )
            {
                 return status;
            }
        }
        else
        {
             return ERR_PARAMETER;
        }
    }

    if( isDebugF_ == true )
    {
        ( void ) SM_SendF( "Prog Type          : %s\r\n", programInfo_->ProgType == CUSTOM ? "CUSTOM" : "HyQvia Template" );
        ( void ) SM_SendF( "Dosage             : %0.1f\r\n", programInfo_->ProgDosage );
        ( void ) SM_SendF( "Needle             : %s\r\n", programInfo_->ProgNeedleSet == SINGLE_NEEDLE_LINESET ? "Single Needle" : "Bifurcated Needle" );
        if( programInfo_->ProgType == HYQVIA_TEMPLATE )
        {
            ( void ) SM_SendF( "Patient Weight     : %s\r\n", s_weight == GREATER_88 ? "Above 88lbs" : "Below or = 88lbs" );
            ( void ) SM_SendF( "Infusion Ramp Up   : %s\r\n", s_Rampup == FIRST_TWO_INFUSIONS ? "First 2 Infusions" : "Ongoing Infusions" );
            ( void ) SM_SendF( "IG Interval        : %d\r\n", programInfo_->ProgIgTimeInterval );
        }
        ( void ) SM_SendF( "Total IG Steps     : %d\r\n", programInfo_->ProgIgSteps );
        for( uint8_t step = 0; step <= programInfo_->ProgIgSteps; step++ )
        {
            if( step == HY_STEP_INDEX )
            {
                if( programInfo_->ProgStepFlowRate[step] == ML_0 )
                {
                    ( void ) SM_SendF( "HY Step            : NOT SET PROPERLY\r\n");
                }
                else
                {
                    ( void ) SM_SendF( "HY Step            : %02d:%02d @ %3d ml/hr/site\r\n", programInfo_->ProgStepTime[step].Hour, programInfo_->ProgStepTime[step].Min, RATE_VALUE_ARR[programInfo_->ProgStepFlowRate[step]]);
                }
            }
            else
            {
                if( programInfo_->ProgStepFlowRate[step] == ML_0 )
                {
                    ( void ) SM_SendF( "IG Step%d           : NOT SET PROPERLY\r\n", step);
                }
                else
                {
                    ( void ) SM_SendF( "IG Step%d           : %02d:%02d @ %3d ml/hr/site\r\n", step,programInfo_->ProgStepTime[step].Hour, programInfo_->ProgStepTime[step].Min, RATE_VALUE_ARR[programInfo_->ProgStepFlowRate[step]]);
                }

            }
        }
        ( void ) SM_SendF( "Rate               : %s\r\n", programInfo_->RateLocked ? "Locked" : "Not Locked" );
        ( void ) SM_SendF( "NeedleCheck        : %s\r\n", programInfo_->NeedlePlacementCheckEnabled ? "Enabled" : "Disabled" );
        ( void ) SM_SendF( "Flush              : %s\r\n", programInfo_->FlushEnabled ? "Enabled" : "Disabled" );
    }
    return status;
}

////////////////////////////////////////////////////////////////////////////////
static void CLI_ProgramCreatHelp ( SERIAL_CMD_SOURCE_T source_ )
{
    ( void ) SM_SendF( "\r\n !!!!!!!  Lists of command to create program !!!!!!!!! \r\n" );
    ( void ) SM_SendF( "Set Program Type by        : progc type custom \r\n" );
    ( void ) SM_SendF( "Set Needle Type by         : progc needle [single/bifurcated] \r\n" );
    ( void ) SM_SendF( "Set Dosage by              : progc dosage [value] \r\n" );
    ( void ) SM_SendF( "Set HY Flowrate by         : progc hyflow [value] \r\n" );
    ( void ) SM_SendF( "Set IG Step No. by         : progc igstep [value] \r\n" );
    ( void ) SM_SendF( "Set IG Step Flow by        : progc igflow [step1_flow] [step2_flow] ... [stepN_flow]\r\n" );
    ( void ) SM_SendF( "Set IG Step Time by        : progc igtime [step1_time] [step2_time] ... [stepN-1_time]\r\n" );
    ( void ) SM_SendF( "Set Patient Weight by      : progc weight [above88/below88] \r\n" );
    ( void ) SM_SendF( "Set Infusion RampUp        : progc rampup [first2/ongoing] \r\n" );
    ( void ) SM_SendF( "Set IG Step Interval by    : progc interval [value] \r\n" );
    ( void ) SM_SendF( "Set Rate Lock Config       : progc lockrate [yes/no]\r\n" );
    ( void ) SM_SendF( "Set NeedleCheck Config by  : progc needlecheck [enable/disable]\r\n" );
    ( void ) SM_SendF( "Set Flush Config by        : progc flush [enable/disable]\r\n" );
    ( void ) SM_SendF( "Review Program by          : progc review \r\n" );
    ( void ) SM_SendF( "Save Program by            : progc save \r\n" );
}

