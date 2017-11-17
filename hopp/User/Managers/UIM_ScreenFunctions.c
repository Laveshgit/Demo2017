//////////////////////////////////////////////////////////////////////////////
///
///                Copyright(c) 2016-2017 FlexMedical
///
///                   *** Confidential Company Proprietary ***
///
//
/// \file   UIM_ScreenFunctions.c
/// \brief  Various functions that are called by the screens to populate dynamic data.
///
///////////////////////////////////////////////////////////////////////////////
#include <clk.h>
#include <math.h>

///User includes
#include "ConfigurationManager.h"
#include "HallManager.h"
#include "LEDManager.h"
#include "ProgramManager.h"
#include "SafetyMonitor.h"
#include "ScreenIcons.h"
#include "ScreenTemplates.h"
#include "UIManager.h"
#include "UIM_ScreenFunctions.h"
#include "ScreenCallBacks.h"
#include "Version.h"

////////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND #DEFINES
////////////////////////////////////////////////////////////////////////////////

#define MAX_HR_DIGITS              3 //Has to be this long for null char
#define MAX_MIN_DIGITS             3 //Has to be this long for null char
#define NUM_TIME_DIGITS            6 //Has to be this long for null char
#define MAX_STEP_ROWS              6
#define FIRST_INIT_STEP            1
#define SCND_INIT_STEP             4
#define NUM_STEP_COLS              3
#define MAX_DOSE_DIGITS            4
#define MAX_RATE_DIGITS            4
#define STEP_TITLE_DIGITS          16 //Takes into account null char
#define MAX_VOL_BRIGHNESS_LEVEL    10 // Volume and Brightness screen has total 10 levels

#define DYNAMIC_ARROW_X_POS     ( 10 )
#define DYNAMIC_ARROW_Y_POS     ( -10 )

#define IG_DYNAMIC_ARROW_X_POS  ( 150 )
#define IG_DYNAMIC_ARROW_Y_POS  ( -15 )

#define FONT_VERSION GUI_FONT_8X10_ASCII

#define DYNAMIC_ARROW_X_POS_TEMP_D  ( 70 )

////////////////////////////////////////////////////////////////////////////////
// ENUMS, TYPEDEFS, AND STRUCTURES
////////////////////////////////////////////////////////////////////////////////

typedef enum
{
    TOP,
    MIDDLE,
    BOTTOM
}SCREEN_LOCATION_T;

///////////////////////////////////////////////////////////////////////////////
// Static Variables
///////////////////////////////////////////////////////////////////////////////

// save current value of Brightness
static uint8_t s_BrightnessValue;
// save current value of Volume
static uint8_t s_VolumeValue;
//store current Hy FlowRate  Value Initilize with default
static RATE_VALUE_T s_CurrHyFlowRateValue = PM_HY_FlOW_RATE_DEFAULT_VALUE;
// Store IG Flow Rate data index value
static RATE_VALUE_T s_CurrIgFlowValue = PM_IG_FlOW_RATE_DEFAULT_VALUE;
// Store IG Duration data index value
static uint8_t s_CurrIgDurationValue = PM_IG_TIME_DEFAULT_VALUE;
//Current Ig step index to store IG duration and IG flowrate
static uint8_t s_CurrIgStepScreenNoIndex = PM_IG_STEP_DEFAULT_INDEX;
// Set IG Default step  Value
static uint8_t s_CurrProgIgSteps = PM_IG_STEP_DEFAULT_VALUE;
//Set current Dosage Value asd default
static float s_CurrProgDosage = PM_DOSAGE_DEFAULT_VALUE;
static float s_CurrProgHyDosage;         // Save calculated Hy
static float s_CurrProgIgDosage;        // Save calculated Ig

static SCREEN_LOCATION_T s_ScreenLocation = TOP;
static unsigned int s_PowerProgressCounter = 0;

GUI_FONT const *const FONT_ARR[MAX_FONTS] =
{
    &GUI_FontRobotoHOPP32,
    &GUI_FontRobotoHOPP28,
    &GUI_FontRobotoHOPP24,
    &GUI_FontRobotoHOPP20,
};

///////////////////////////////////////////////////////////////////////////////
// Local Variables
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Prototypes
///////////////////////////////////////////////////////////////////////////////

static const void *GetImageById( U32 id_, U32 *pSize_ );
static void UIM_GetPluralLangInfo( bool *const status_ );
static void UIM_CreateStepReviewListView( WM_HWIN hWin_, unsigned int numRows_ );
static void UIM_StringCoordsLCD( WM_HWIN hWin_, GUI_RECT *rect_, char const *textString_, int alignment_, FONT_ID_T font_ );


////////////////////////////////////////////////////////////////////////////////
// Global Variables
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
static void UIM_DetermineID ( int *id_, LINE_NUM_T line_ )
{

    ERROR_T err = ERR_NONE;

    if( id_ == NULL )
    {
        err = ERR_PARAMETER;
        UIM_ScreenFuncTrace( err, __LINE__ );
        return;
    }

    switch( line_ )
    {
        case TITLE_LINE:
            *id_ = TITLE_LINE_ID;
            break;

        case LINE1_TEXT:
            *id_ = LINE1_TEXT_ID;
            break;

        case LINE2_TEXT:
            *id_ = LINE2_TEXT_ID;
            break;

        case LINE3_TEXT:
            *id_ = LINE3_TEXT_ID;
            break;

        case LINE4_TEXT:
            *id_ = LINE4_TEXT_ID;
            break;

        case TEXT_BOX:
            *id_ = TEXTBOX_ID;
            break;

        case ACTION_LINE:
            *id_ = ACTION_LINE_ID;
            break;

        default:
            err = ERR_PARAMETER;
            UIM_ScreenFuncTrace( err, __LINE__ );
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_TextConcatenate ( TEXT_Handle hItem_, char *textString_, const char *concreteString_ )
{
    ERROR_T err = ERR_NONE;

    if( ( textString_ != NULL ) && ( concreteString_ != NULL ) )
    {
        TEXT_GetText( hItem_, textString_, MAX_NUM_LINE_CHARS );

        if( textString_[0] != '\0' )
        {
            strncat( textString_, " ", sizeof( " " ) );
            strncat( textString_, concreteString_, strlen( concreteString_ ) );

            (void)TEXT_SetText( hItem_, textString_ );
        }
        else
        {
            (void)TEXT_SetText( hItem_, textString_ );
        }
    }
    else
    {
        err = ERR_PARAMETER;
        UIM_ScreenFuncTrace( err, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetScreenTime2 ( WM_HWIN window_, LINE_NUM_T line_, bool totalFlag_ )
{
    WM_HWIN hItem;

    int err = 0;
    int id = 0;

    PROGRAM_INFO_T currProg;

    UIM_GetCurrProg( &currProg );

    char hourString[MAX_HR_DIGITS];
    char minuteString[MAX_MIN_DIGITS];
    char timeString[NUM_TIME_DIGITS];
    char textString[MAX_NUM_LINE_CHARS];

    memset( hourString, 0, MAX_HR_DIGITS );
    memset( minuteString, 0, MAX_MIN_DIGITS );
    memset( timeString, 0, NUM_TIME_DIGITS );

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( window_, id );
    if( hItem == 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
        return;
    }

    if( totalFlag_ )
    {

        err = snprintf( minuteString, MAX_MIN_DIGITS, "%02d", currProg.TotalInfusionTime.Min ); //lint !e516 data type for argument 4 can be anything

        if( err < 0 )
        {
            UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
        }

        err = snprintf( hourString, MAX_HR_DIGITS, "%02d", currProg.TotalInfusionTime.Hour ); //lint !e516 data type for argument 4 can be anything
        if( err < 0 )
        {
            UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
        }

    }
    else
    {
        uint16_t currStep = 0;

        UIM_GetStep( &currStep );
        err = snprintf( minuteString, MAX_MIN_DIGITS, "%02d", currProg.ProgStepTime[currStep].Min ); //lint !e516 data type for argument 4 can be anything
        if( err < 0 )
        {
            UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
        }

        err = snprintf( hourString, MAX_HR_DIGITS, "%02d", currProg.ProgStepTime[currStep].Hour ); //lint !e516 data type for argument 4 can be anything
        if( err < 0 )
        {
            UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
        }


    }

    strncat( timeString, " ", 1 );
    strncat( timeString, hourString, 2 );
    strncat( timeString, ":", 1 );
    strncat( timeString, minuteString, 2 );

    TEXT_GetText( hItem, textString, MAX_NUM_LINE_CHARS );

    if( textString[0] != '\0' )
    {
        strncat( textString, timeString, sizeof( timeString ) );

        (void)TEXT_SetText( hItem, textString );
    }
    else
    {
        (void)TEXT_SetText( hItem, timeString );
    }

    strncat( textString, timeString, sizeof( timeString ) );

}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetRateConfig ( WM_HWIN hWin_, LINE_NUM_T line_ )
{
    WM_HWIN hItem;

    ERROR_T err = ERR_NONE;
    int id = 0;

    PROGRAM_INFO_T currProg;

    UIM_GetCurrProg( &currProg );

    char rateStr[MAX_NUM_LINE_CHARS];
    char textString[MAX_NUM_LINE_CHARS];

    memset( textString, 0, MAX_NUM_LINE_CHARS );
    memset( rateStr, 0, MAX_NUM_LINE_CHARS );

    err = LM_GetRateSettingString( rateStr, (bool)currProg.RateLocked );
    UIM_ScreenFuncTrace( err, __LINE__ );

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( hWin_, id );
    if( hItem == 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
        return;
    }

    UIM_TextConcatenate( hItem, textString, rateStr );
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetNeedleType ( WM_HWIN hWin_, LINE_NUM_T line_ )
{
    WM_HWIN hItem;

    ERROR_T err = ERR_NONE;
    int id = 0;

    PROGRAM_INFO_T currProg;

    UIM_GetCurrProg( &currProg );

    char needleStr[MAX_NUM_LINE_CHARS];
    char textString[MAX_NUM_LINE_CHARS];

    memset( textString, 0, MAX_NUM_LINE_CHARS );
    memset( needleStr, 0, MAX_NUM_LINE_CHARS );

    err = LM_GetNeedleSetString( needleStr, currProg.ProgNeedleSet );
    UIM_ScreenFuncTrace( err, __LINE__ );

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( hWin_, id );
    if( hItem == 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
        return;
    }

    UIM_TextConcatenate( hItem, textString, needleStr );
}

////////////////////////////////////////////////////////////////////////////////
//This function takes a text widget  and determines where to move the arrows in relation to it based upon the length of the string
//contained within the text widget. It also has the ability to specify how much space, in X and Y, from the string, the arrows should be placed.
//The input xPos_ moves the arrow further right on the screen while the input yPos_ moves the arrow further DOWN the screen.
void UIM_SetUpDownArrowsDynamic ( WM_HWIN hWin_, bool upFlag_, bool downFlag_, int xPos_, int yPos_, LINE_NUM_T line_,
                                  int alignment_ )
{
    WM_HWIN hItem;
    const void *pData;
    U32 fileSize;
    ERROR_T err = ERR_NONE;
    SCREEN_ID_T screen;
    FONT_ID_T font;

    GUI_RECT rect = {0, 0, 0, 0};
    int id = 0;
    char textString[MAX_NUM_LINE_CHARS];

    UIM_GetScreenNo( &screen );
    memset( textString, 0, MAX_NUM_LINE_CHARS );

    err = LM_GetString( textString, line_, screen, &font, false ); //Used only to get font
    UIM_ScreenFuncTrace( err, __LINE__ );

    memset( textString, 0, MAX_NUM_LINE_CHARS );

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( hWin_, id );

    TEXT_GetText( hItem, textString, MAX_NUM_LINE_CHARS );

    UIM_StringCoordsLCD( hItem, &rect, textString, alignment_, font );

    // Initialization of up arrow
    hItem = WM_GetDialogItem( hWin_, UP_ARROW_IMAGE );
    pData = GetImageById( ID_IMAGE_2_IMAGE_0, &fileSize );

    if( upFlag_ )
    {
        WM_MoveTo( hItem, ( rect.x1 + xPos_ ),  ( rect.y0 + yPos_ ) );
        IMAGE_SetBMP( hItem, pData, fileSize );
        WM_ShowWindow( hItem );
    }
    else
    {
        WM_HideWindow( hItem );
    }

    // Initialization of down arrow
    hItem = WM_GetDialogItem( hWin_, DOWN_ARROW_IMAGE );
    pData = GetImageById( ID_IMAGE_3_IMAGE_0, &fileSize );

    if( downFlag_ )
    {
        WM_MoveTo( hItem, ( rect.x1 + xPos_ ),  ( rect.y0 + yPos_ + ARROW_HGT ) );
        IMAGE_SetBMP( hItem, pData, fileSize );
        WM_ShowWindow( hItem );
    }
    else
    {
        WM_HideWindow( hItem );
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetDosage2 ( WM_HWIN window_, LINE_NUM_T line_ )
{
    int err = 0;
    int id = 0;

    WM_HWIN hItem;
    PROGRAM_INFO_T currProg;
    char doseString[MAX_DOSE_DIGITS];
    char textString[MAX_NUM_LINE_CHARS];
    UIM_GetCurrProg( &currProg );

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( window_, id );
    if( hItem == 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
        return;
    }

    memset( doseString, 0, MAX_DOSE_DIGITS );

    //Check trailing zero logic for dosage value
    if( fmod( (double)currProg.ProgDosage, (double)DOSAGE_INTEGER_BASE ) )
    {
        err = sprintf( doseString, "%.1f", currProg.ProgDosage );
    }
    else
    {
        err = sprintf( doseString, "%.0f", currProg.ProgDosage );
    }

    if( err < 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
    }


    strncat( doseString, G_STR, sizeof( doseString ) );

    TEXT_GetText( hItem, textString, MAX_NUM_LINE_CHARS );

    if( textString[0] != '\0' )
    {
        strncat( textString, " ", 1 );
        strncat( textString, doseString, sizeof( doseString ) );

        (void)TEXT_SetText( hItem, textString );
    }
    else
    {
        (void)TEXT_SetText( hItem, doseString );
    }

}

////////////////////////////////////////////////////////////////////////////////
// This function concatenates each item in the list for display in a single text box.
// It is intended to address alignment issues for multiline text
void UIM_SetListSingleTextBox ( WM_HWIN hWin_, LINE_NUM_T line_, uint8_t listStartIndex_, LINE_NUM_T maxListLines_ )
{
    WM_HWIN hItem;
    ERROR_T err = ERR_NONE;
    int guiErr = 0;
    size_t charLen = 0;
    char textStringCat[MAX_NUM_LINE_CHARS] = {0};
    char *string = NULL;
    size_t totalStrLen = 0;
    char textString[MAX_NUM_LINE_CHARS];
    char displayString[MAX_NUM_LINE_CHARS * 3];
    FONT_ID_T font = FL;
    int id;
    bool langPluralStatus = false;
    SCREEN_ID_T screen;

    UIM_GetScreenNo( &screen );

    memset( displayString, 0, sizeof( displayString ) );

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( hWin_, id );

    for( LINE_NUM_T i = LINE1_TEXT; i <= maxListLines_; i++ )
    {
        memset( textString, 0, MAX_NUM_LINE_CHARS );

        textString[0] = (char)( ( i - LINE1_TEXT ) + listStartIndex_ ) + '0';
        textString[1] = '.';

        UIM_GetPluralLangInfo( &langPluralStatus );
        err = LM_GetString( &textString[2], i, screen, &font, langPluralStatus );
        UIM_ScreenFuncTrace( err, __LINE__ );

        string = strstr( &textString[0], END_CHECK_STRING );
        totalStrLen = strnlen( textString, MAX_NUM_LINE_CHARS );

        charLen = strnlen( string, MAX_NUM_LINE_CHARS );

        //Check for remain string
        if( charLen > END_CHECK_STRING_SIZE )
        {
            if( string != NULL )
            {
                memcpy( &textStringCat[0], &string[END_CHECK_STRING_SIZE], charLen );
            }
            //Add space for adjust string
            textString[totalStrLen + END_CHECK_STRING_SIZE - charLen] = ' ';
            textString[totalStrLen + END_CHECK_STRING_SIZE + 1 - charLen] = ' ';
            textString[totalStrLen + END_CHECK_STRING_SIZE + 2 - charLen] = ' ';
            memcpy( &textString[(totalStrLen + END_CHECK_STRING_SIZE + 3) - charLen], textStringCat, charLen );
        }

        strncat( displayString, textString, sizeof(textString) );
        strncat( displayString, "\r\n", sizeof("\r\n") );
    }

    TEXT_SetWrapMode( hItem, GUI_WRAPMODE_WORD );
    guiErr = TEXT_SetText( hItem, displayString );
    TEXT_SetFont( hItem, FONT_ARR[font] );
    TEXT_SetTextAlign( hItem, GUI_TA_VCENTER );
    if( guiErr )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
//This should be used when an underline is not needed and the rate and rate units are in a single widget.
void UIM_SetScreenSingleRate2 ( WM_HWIN window_, LINE_NUM_T line_ )
{
    int strErr = 0;
    WM_HWIN hItem;
    int id = 0;
    PROGRAM_INFO_T currProg;
    uint16_t currStep = 0;
    char textString[MAX_NUM_LINE_CHARS] = {0};
    char tempTextString[MAX_NUM_LINE_CHARS] = {0};
    ERROR_T err = ERR_NONE;
    FONT_ID_T font = FL;
    unsigned int stringLen;
    SCREEN_ID_T screen;

    UIM_GetScreenNo( &screen );

    memset( textString, 0, MAX_NUM_LINE_CHARS );

    UIM_DetermineID( &id, line_ );

    UIM_GetCurrProg( &currProg );
    UIM_GetStep( &currStep );

    if( screen == SCREEN_13 )
    {
        //Something comes before the rate
        memset( tempTextString, 0, MAX_NUM_LINE_CHARS );
        err = LM_GetString( &tempTextString[0], line_, screen, &font, false );

        if( err == ERR_NONE )
        {
            strncpy( textString, tempTextString, sizeof( textString ) - 1 );

            //one space added to maintain space between two text word
            stringLen = strlen( textString );
            if( ( stringLen + 1 ) <= MAX_NUM_LINE_CHARS )
            {
                strncat( textString, " ", 1 );
            }
        }
    }

    memset( tempTextString, 0, MAX_NUM_LINE_CHARS );
    strErr = snprintf( tempTextString, MAX_RATE_DIGITS, "%d", RATE_VALUE_ARR[currProg.ProgStepFlowRate[currStep]] ); //lint !e516 data type for argument 4 can be anything

    if( strErr < 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
    }

    strcat( textString, tempTextString );
    memset( tempTextString, 0, MAX_NUM_LINE_CHARS );

    err = LM_GetRateUnitsString( tempTextString );
    UIM_ScreenFuncTrace( err, __LINE__ );

    strncat( textString, " ", 1 );
    strcat( textString, tempTextString );

    hItem = WM_GetDialogItem( window_, id );

    if( hItem == 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
    }
    else
    {
        strErr = TEXT_SetText( hItem, textString );

        if( strErr )
        {
            UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
        }

        TEXT_SetFont( hItem, FONT_ARR[font] );
        TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );
    }
}

//////////////////////////////////////////////////////////////////////////////////
void UIM_SetScreenStepInfo ( WM_HWIN window_ )
{
    int err = 0;
    SCREEN_ID_T screen;
    WM_HWIN hItem;

    UIM_GetScreenNo( &screen );

    PROGRAM_INFO_T currProg;
    UIM_GetCurrProg( &currProg );

    hItem = WM_GetDialogItem( window_, STEPVIEW_ID );
    if( hItem == 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
    }
    else
    {

        unsigned int numRows = 0;
        unsigned int stepNumber = 0;

        if( screen == SCREEN_14_1 )       //STEP_INFO1
        {
            numRows = MIN( currProg.ProgIgSteps, NUM_STEP_ROWS_PER_SCR );
            stepNumber = FIRST_INIT_STEP;
        }
        else if( screen == SCREEN_14_2 )  //STEP_INFO2
        {
            numRows = MIN( currProg.ProgIgSteps - NUM_STEP_ROWS_PER_SCR, NUM_STEP_ROWS_PER_SCR );
            stepNumber = SCND_INIT_STEP;
        }

        if( numRows > NUM_STEP_ROWS_PER_SCR ) //If screen 14-2 is ever reached and there are 3 or less steps, then numRows would have
        {                                     //have wrapped. Verify that there are not more than 3 rows calculated.
            numRows = 0;
        }

        UIM_CreateStepReviewListView( window_, numRows );

        for( unsigned int i = 0; i <= numRows; i++ )
        {
            char rowNumString[2];
            err = sprintf( rowNumString, "%d.", stepNumber );
            if( err < 0 )
            {
                UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
            }
            LISTVIEW_SetItemText( hItem, 0, i, rowNumString );

            char rateString[3];

            err = sprintf( rateString, "%d", RATE_VALUE_ARR[currProg.ProgStepFlowRate[stepNumber]] );

            if( err < 0 )
            {
                UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
            }

            LISTVIEW_SetItemText( hItem, 2, i, rateString );

            char timeString[4];
            uint16_t min;

            min = ( currProg.ProgStepTime[stepNumber].Hour * 60 ) + currProg.ProgStepTime[stepNumber].Min;
            err = sprintf( timeString, "%d", min );


            if( err < 0 )
            {
                UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
            }
            LISTVIEW_SetItemText( hItem, 1, i, timeString );

            stepNumber++;

        }
    }

}

////////////////////////////////////////////////////////////////////////////////
void UIM_AdjustLevel ( GUI_HWIN currWindow_, bool incrementFlag_, bool decrementFlag_ )
{
    WM_HWIN hItem;
    const void *pData = NULL;
    U32 fileSize;
    SCREEN_ID_T screen;
    GUI_HWIN currWindow = 0;
    UIM_GetWindow( &currWindow );

    UIM_GetScreenNo( &screen );

    if( ( screen == SCREEN_40 )    ||  // ADJUST_BRIGHTNESS
        ( screen == SCREEN_29_4I ) || //PROG_BRIGHTNESS
        ( screen == SCREEN_30_4I ) )  //PROG_PAUSE_BRIGHT
    {
        //Increment Brightness Value
        if( incrementFlag_ == true )
        {
            hItem = WM_GetDialogItem( currWindow_, (ID_IMAGE_4 + s_BrightnessValue) );
            pData = GetImageById( ID_IMAGE_9_IMAGE_0, &fileSize );
            IMAGE_SetBMP( hItem, pData, fileSize );
            GUI_Delay( GUI_DELAY_AMOUNT );

            if( s_BrightnessValue < MAX_VOL_BRIGHNESS_LEVEL )
            {
                s_BrightnessValue++;
            }
        }
        else if( decrementFlag_ == true )     //decrement Brightness Value
        {
            if( s_BrightnessValue > 0 )
            {
                s_BrightnessValue--;
            }
            hItem = WM_GetDialogItem( currWindow, (ID_IMAGE_4 + s_BrightnessValue) );
            pData = GetImageById( ID_IMAGE_8_IMAGE_0, &fileSize );
            IMAGE_SetBMP( hItem, pData, fileSize );
            GUI_Delay( GUI_DELAY_AMOUNT );
        }
        UIM_SetBrightnessLevel( false );    //set current brightness level and not save into flash
    }
    else if( ( screen == SCREEN_41 )    ||       // ADJUST_VOLUME
             ( screen == SCREEN_29_5I ) ||      // PROG_SOUND_VOL
             ( screen == SCREEN_30_5I ) )       // PROG_PAUSE_SOUND
    {
        //Increment volume Value
        if( incrementFlag_ == true )
        {
            hItem = WM_GetDialogItem( currWindow_, (ID_IMAGE_4 + s_VolumeValue) );
            pData = GetImageById( ID_IMAGE_9_IMAGE_0, &fileSize );
            IMAGE_SetBMP( hItem, pData, fileSize );
            GUI_Delay( GUI_DELAY_AMOUNT );

            if( s_VolumeValue < MAX_VOL_BRIGHNESS_LEVEL )
            {
                s_VolumeValue++;
            }
        }
        else if( decrementFlag_ == true )     //decrement volume Value
        {
            if( s_VolumeValue > 0 )
            {
                s_VolumeValue--;
            }
            hItem = WM_GetDialogItem( currWindow, (ID_IMAGE_4 + s_VolumeValue) );
            pData = GetImageById( ID_IMAGE_8_IMAGE_0, &fileSize );
            IMAGE_SetBMP( hItem, pData, fileSize );
            GUI_Delay( GUI_DELAY_AMOUNT );
        }
        UIM_SetVolumeLevel( false );    //set current volume level and not save into flash
    }
}

////////////////////////////////////////////////////////////////////////////////
static const void *GetImageById ( U32 id_, U32 *pSize_ )
{
    if( pSize_ == NULL )
    {
        UIM_ScreenFuncTrace( ERR_PARAMETER, __LINE__ );
        return NULL;
    }          
  
    switch( id_ )
    {
        case ID_IMAGE_0_IMAGE_0:
            *pSize_ = sizeof(INFO_IMAGE);
            return (const void *)INFO_IMAGE;
        case ID_IMAGE_1_IMAGE_0:
            *pSize_ = sizeof(RIGHT_ARROW);
            return (const void *)RIGHT_ARROW;
        case ID_IMAGE_2_IMAGE_0:
            *pSize_ = sizeof(UP_ARROW);
            return (const void *)UP_ARROW;
        case ID_IMAGE_3_IMAGE_0:
            *pSize_ = sizeof(DOWN_ARROW);
            return (const void *)DOWN_ARROW;
        case ID_IMAGE_4_IMAGE_0:
            *pSize_ = sizeof(LOGO_IMAGE);
            return (const void *)LOGO_IMAGE;
        case ID_IMAGE_5_IMAGE_0:
            *pSize_ = sizeof(LEFT_ARROW);
            return (const void *)LEFT_ARROW;
        case ID_IMAGE_6_IMAGE_0:
            *pSize_ = sizeof(LOW_LIGHT);
            return (const void *)LOW_LIGHT;
        case ID_IMAGE_7_IMAGE_0:
            *pSize_ = sizeof(HIGH_LIGHT);
            return (const void *)HIGH_LIGHT;
        case ID_IMAGE_8_IMAGE_0:
            *pSize_ = sizeof(HOLLOW_SQUARE);
            return (const void *)HOLLOW_SQUARE;
        case ID_IMAGE_9_IMAGE_0:
            *pSize_ = sizeof(BLACK_SQUARE);
            return (const void *)BLACK_SQUARE;
        case ID_IMAGE_10_IMAGE_0:
            *pSize_ = sizeof(RIGHT_ARROW);
            return (const void *)RIGHT_ARROW;
        case ID_IMAGE_11_IMAGE_0:
            *pSize_ = sizeof(LOW_VOLUME);
            return (const void *)LOW_VOLUME;
        case ID_IMAGE_12_IMAGE_0:
            *pSize_ = sizeof(HIGH_VOLUME);
            return (const void *)HIGH_VOLUME;
        case ID_IMAGE_13_IMAGE_0:
            *pSize_ = sizeof(LOW_BATTERY);
            return (const void *)LOW_BATTERY;
        case ID_IMAGE_14_IMAGE_0:
            *pSize_ = sizeof(EXH_BATTERY);
            return (const void *)EXH_BATTERY;
        case ID_IMAGE_15_IMAGE_0:
            *pSize_ = sizeof(NEGATIVE_INFO);
            return (const void *)NEGATIVE_INFO;


        default:
            break;

    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetBatteryImage ( GUI_HWIN currWindow_ )
{
    WM_HWIN hItem;
    SCREEN_ID_T screen;
    const void *pData = NULL;
    U32 fileSize = ( 0U );

    UIM_GetScreenNo( &screen );

    hItem = WM_GetDialogItem( currWindow_, BATTERY_IMAGE_ID );

    if( screen == SCREEN_2 )      //LOW_BATT_CONN_CHARGE_S
    {
        pData = GetImageById( ID_IMAGE_13_IMAGE_0, &fileSize );
    }
    else if( screen == SCREEN_1 ) //SHUT_DOWN_CONN_CHARGE_S
    {
        pData = GetImageById( ID_IMAGE_14_IMAGE_0, &fileSize );
    }

    IMAGE_SetBMP( hItem, pData, fileSize );
}

////////////////////////////////////////////////////////////////////////////////
void UIM_ListboxDecrementAction ( GUI_HWIN currWindow_ )
{

    WM_HWIN hItemLB;

    hItemLB = WM_GetDialogItem( currWindow_, PRI_LISTBOX );

    if( hItemLB )
    {
        LISTBOX_IncSel( hItemLB );
        if( s_ScreenLocation < BOTTOM )
        {
            s_ScreenLocation++;
        }

        GUI_Delay( GUI_DELAY_AMOUNT );
    }
    else //In CLI screen review, up and down will do something for every screen
    {    //This will make sure there is a list box before attempting to change anything.
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_ListboxIncrementAction ( GUI_HWIN currWindow_ )
{
    WM_HWIN hItemLB;

    hItemLB = WM_GetDialogItem( currWindow_, PRI_LISTBOX );

    if( hItemLB )
    {
        LISTBOX_DecSel( hItemLB );
        if( s_ScreenLocation > TOP )
        {
            s_ScreenLocation--;
        }

        GUI_Delay( GUI_DELAY_AMOUNT );
    }
    else //In CLI screen review, up and down will do something for every screen
    {    //This will make sure there is a list box before attempting to change anything.
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_InsertRate ( char *textString_, LINE_NUM_T textID_ )
{
    ERROR_T err = ERR_NONE;

    char numString[MAX_NUM_LINE_CHARS];
    PROGRAM_INFO_T currProg;
    uint16_t currStep = 0;
    RATE_VALUE_T rate = ML_0;
    RATE_VALUE_T nextRate = ML_0;
    int numRate = 0;

    if( textString_ == NULL )
    {
        err = ERR_PARAMETER;
        UIM_ScreenFuncTrace( err, __LINE__ );
        return;
    }

    UIM_GetCurrProg( &currProg );
    UIM_GetStep( &currStep );

    err = PM_GetIgStepFlowrate( ( uint8_t )( currStep + 1 ), &nextRate );
    UIM_ScreenFuncTrace( err, __LINE__ );

    if( textID_ == OPTION1 )
    {
        rate = currProg.ProgStepFlowRate[currStep];
    }
    else if( textID_ == OPTION2 )
    {
        rate = nextRate;
    }

    numRate = RATE_VALUE_ARR[rate];

    snprintf( &numString[0], MAX_RATE_DIGITS, "%d", numRate ); //lint !e516 data type for argument 4 can be anything

    err = LM_SetDynamicStrData( textString_, numString );
    UIM_ScreenFuncTrace( err, __LINE__ );


}

////////////////////////////////////////////////////////////////////////////////
void UIM_InsertBifurcatedRate ( WM_HWIN hWin_, LINE_NUM_T line_ )
{
    WM_HWIN hItem;
    ERROR_T err = ERR_NONE;

    char numString[MAX_NUM_LINE_CHARS];
    char tempStr[MAX_NUM_LINE_CHARS];
    PROGRAM_INFO_T currProg;
    uint16_t currStep = 0;
    RATE_VALUE_T rate = ML_0;

    int numRate = 0;
    int id = 0;

    memset( numString, 0, MAX_NUM_LINE_CHARS );
    memset( tempStr, 0, MAX_NUM_LINE_CHARS );

    UIM_GetCurrProg( &currProg );
    UIM_GetStep( &currStep );

    if( currStep == HY_STEP_INDEX )
    {
        rate = currProg.ProgStepFlowRate[HY_STEP_INDEX];
    }
    else
    {
        rate = currProg.ProgStepFlowRate[currProg.ProgIgSteps];
    }

    numRate = RATE_VALUE_ARR[rate] * 2;

    snprintf( &numString[0], MAX_RATE_DIGITS, "%d", numRate ); //lint !e516 data type for argument 4 can be anything

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( hWin_, id );

    TEXT_GetText( hItem, tempStr, MAX_NUM_LINE_CHARS );

    if( tempStr[0] != '\0' )
    {
        err = LM_SetDynamicStrData( tempStr, numString );
        UIM_ScreenFuncTrace( err, __LINE__ );

        (void)TEXT_SetText( hItem, tempStr );
    }
}

////////////////////////////////////////////////////////////////////////////////
//This function is used to determine which item in the list box should be highlighted once a screen is drawn.
//Typically only needed when its something other than the default value, such as when the back button is pressed.
static void UIM_DermineLBlocation ( WM_HWIN hWin_, int index_ )
{

    if( index_ == 0 )
    {
        s_ScreenLocation = TOP;
    }
    else if( index_ == 1 )
    {
        s_ScreenLocation = MIDDLE;
    }
    else
    {
        s_ScreenLocation = BOTTOM;
    }

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SetLanguageListBox ( WM_HWIN hItem_ )
{
    ERROR_T err = ERR_NONE;
    FONT_ID_T font = FL;
    FONT_ID_T prevFont = FL;
    uint8_t numOfLangs = 0;

    char textString[MAX_FILE_NAME_CHAR_SIZE]= {0};
    WM_HWIN lb = hItem_;
    
    err = LM_GetLanguageCount( &numOfLangs );
    UIM_ScreenFuncTrace( err, __LINE__ );               //Report to error handler                    
    
    for( int i = 0; i < numOfLangs; i++ )
    {
        memset( textString, 0, MAX_FILE_NAME_CHAR_SIZE );

        err = LM_GetLanguageName( textString, (uint8_t)i, &font );
        UIM_ScreenFuncTrace( err, __LINE__ ); 
        if( font < prevFont ) 
        {            
            prevFont = font;
        }
        LISTBOX_AddString( lb, textString );
    }
    
    LISTBOX_SetFont( lb, FONT_ARR[prevFont] );
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SetStandardListBox ( char *textString_, uint16_t txtStrSize_, WM_HWIN hItem_ )
{
    ERROR_T err = ERR_NONE;
    FONT_ID_T curFont = FL;     //Current string font
    FONT_ID_T prevFont = FL;    //Previous string font
    SCREEN_ID_T screen;

    UIM_GetScreenNo( &screen );

    if( ( textString_ == NULL ) || ( hItem_ == 0 ) )
    {
        UIM_ScreenFuncTrace( ERR_PARAMETER, __LINE__ );               //Report to error handler
        return;
    }

    for( LINE_NUM_T textID = OPTION1; textID < LINE1_TEXT; textID++ )
    {
        //Space added at beginning to gaurantee minimum 4 pixel buffer from edge of screen

        err = LM_GetString( textString_, textID, screen, &curFont, false );

        if( ( err != ERR_NOT_SET ) && //Since traversing several indexes that may not be set, do not report all them.
            ( err != ERR_NONE ) )
        {
            UIM_ScreenFuncTrace( err, __LINE__ );
        }

        if( textString_[0] != '\0' )
        {
            if( screen == SCREEN_31_1 ) //IG_STEP_ENDING
            {
                UIM_InsertRate( textString_, textID );
            }
            LISTBOX_AddString( hItem_, textString_ );
            memset( textString_, 0, MAX_NUM_LINE_CHARS );

            // if the current string font (curFont) is smaller than the previous string font then set the
            // listbox with current string font (curFont) else update the font (curFont) with the previous string font.
            // Main objective is to set the list box with the smaller size font from the available fonts.
            if( prevFont > curFont )       //Smaller the font, higher the enum value. (FL = 0 to FT = 4)
            {
                curFont = prevFont;
            }
            else
            {
                prevFont = curFont; //update the previous font
            }
            LISTBOX_SetFont( hItem_, FONT_ARR[curFont] );
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
static void UIM_SetListBoxWithExclusion ( char *textString_, uint16_t txtStrSize_, WM_HWIN hItem_, uint8_t excludeIndex_ )
{
    ERROR_T err = ERR_NONE;
    FONT_ID_T curFont = FL;     //Current string font
    FONT_ID_T prevFont = FL;    //Previous string font
    uint16_t excludeId;
    SCREEN_ID_T screen;

    UIM_GetScreenNo( &screen );

    if( ( textString_ == NULL ) || ( hItem_ == 0 ) )
    {
        UIM_ScreenFuncTrace( ERR_PARAMETER, __LINE__ );               //Report to error handler
        return;
    }

    excludeId = excludeIndex_ + OPTION1;

    for( LINE_NUM_T textID = OPTION1; textID < LINE1_TEXT; textID++ )
    {
        if( textID != excludeId )
        {
            err = LM_GetString( textString_, textID, screen, &curFont, false );

            if( ( err != ERR_NOT_SET ) && //Since traversing several indexes that may not be set, do not report all them.
                ( err != ERR_NONE ) )
            {
                UIM_ScreenFuncTrace( err, __LINE__ );
            }

            if( textString_[0] != '\0' )
            {
                LISTBOX_AddString( hItem_, textString_ );
                memset( textString_, 0, MAX_NUM_LINE_CHARS );

                // if the current string font (curFont) is smaller than the previous string font then set the
                // listbox with current string font (curFont) else update the font (curFont) with the previous string font.
                // Main objective is to set the list box with the smaller size font from the available fonts.
                if( prevFont > curFont )       //Smaller the font, higher the enum value. (FL = 0 to FT = 4)
                {
                    curFont = prevFont;
                }
                else
                {
                    prevFont = curFont; //update the previous font
                }
                LISTBOX_SetFont( hItem_, FONT_ARR[curFont] );
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetListBox ( WM_HWIN hWin_ )
{
    WM_HWIN hItem;
    ERROR_T err = ERR_NONE;
    SCREEN_ID_T screen;
    int listBoxIndex = 0;
    char textString[MAX_NUM_LINE_CHARS];
    FONT_ID_T font = FL;
    uint8_t numOfLanguages = 0;
    uint8_t excludeIndex;

    UIM_GetScreenNo( &screen );

    memset( textString, 0, MAX_NUM_LINE_CHARS );

    //Since the listbox is the only screen that has a header, we can leave the header code here.
    hItem = WM_GetDialogItem( hWin_, TITLE_LINE_ID );

    err = LM_GetString( textString, TITLE_LINE, screen, &font, false );
    UIM_ScreenFuncTrace( err, __LINE__ );

    HEADER_SetFont( hItem, FONT_ARR[font] );

    //Must set the height after changing the font to ensure that it maintains what was originally planned.
    HEADER_SetHeight( hItem, HDR_HEIGHT );

    HEADER_AddItem( hItem, HDR_WIDTH, textString, GUI_TA_LEFT );
    memset( textString, 0, MAX_NUM_LINE_CHARS );

    hItem = WM_GetDialogItem( hWin_, PRI_LISTBOX );

    //Should set any widget parameters after setting the font.
    LISTBOX_SetItemSpacing( hItem, LB_ITEM_SPACING );

    //Set Widget's line draw effect none
    WIDGET_SetEffect( hItem, &WIDGET_Effect_None );

    // Get Current Listbox Index to highlight listbox
    err = UIM_GetListboxIndex( &listBoxIndex );
    UIM_ScreenFuncTrace( err, __LINE__ );               //Report to error handler

    UIM_DermineLBlocation( hWin_, listBoxIndex );

    switch( screen )
    {
        case SCREEN_39:
            err = LM_GetLanguageCount( &numOfLanguages );
            UIM_ScreenFuncTrace( err, __LINE__ );

            if( numOfLanguages > 1 )
            {
                //Populate the listbox using standard template methods
                UIM_SetStandardListBox( textString, sizeof( textString ), hItem );
            }
            else
            {
                // Do not show LANGUAGE in the menu because only one language is available.
                excludeIndex = LANGUAGE_LISTBOX;
                UIM_SetListBoxWithExclusion( textString, sizeof( textString ), hItem, excludeIndex );
            }

            break;

        case SCREEN_39_8:
            //Populate the listbox with language selection options
            UIM_SetLanguageListBox( hItem );
            break;

        default:
            //Populate the listbox using standard template methods
            UIM_SetStandardListBox( textString, sizeof( textString ), hItem );
            break;
    }

    LISTBOX_SetTextAlign( hItem, GUI_TA_LEFT | GUI_TA_VCENTER );

    LISTBOX_SetSel( hItem, listBoxIndex );

}

////////////////////////////////////////////////////////////////////////////////
void UIM_AddInfoImage ( WM_HWIN hWin_ )
{
    WM_HWIN hItem;
    INFO_T info;
    const void *pData;
    U32 fileSize;

    UIM_GetInfoValue( &info );

    if( info == HAS_INFO )
    {
        hItem = WM_GetDialogItem( hWin_, INFO_IMAGE_ID );
        pData = GetImageById( ID_IMAGE_0_IMAGE_0, &fileSize );
        IMAGE_SetBMP( hItem, pData, fileSize );
    }

}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetUpDownArrows ( WM_HWIN hWin_, bool upFlag_, bool downFlag_ )
{
    WM_HWIN hItem;
    const void *pData;
    U32 fileSize;

    // Initialization of up arrow
    hItem = WM_GetDialogItem( hWin_, UP_ARROW_IMAGE );
    pData = GetImageById( ID_IMAGE_2_IMAGE_0, &fileSize );

    if( upFlag_ )
    {
        IMAGE_SetBMP( hItem, pData, fileSize );
        WM_ShowWindow( hItem );
    }
    else
    {
        WM_HideWindow( hItem );
    }

    // Initialization of down arrow
    hItem = WM_GetDialogItem( hWin_, DOWN_ARROW_IMAGE );
    pData = GetImageById( ID_IMAGE_3_IMAGE_0, &fileSize );

    if( downFlag_ )
    {
        IMAGE_SetBMP( hItem, pData, fileSize );
        WM_ShowWindow( hItem );
    }
    else
    {
        WM_HideWindow( hItem );
    }

}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetLeftArrows ( WM_HWIN hWin_, int numArrows_ )
{
    WM_HWIN hItem;
    const void *pData;
    U32 fileSize;
    bool sameScreenFlag = FALSE;

    static bool boolArr[MAX_NUM_LEFT_ARROWS];

    static int index = 0;

    UIM_GetScreenFlag( &sameScreenFlag );

    if( ( index <= 0 ) ||
        ( !sameScreenFlag ) )
    {
        index = MAX_NUM_LEFT_ARROWS - 1;
        memset( boolArr, 0, sizeof( boolArr ) );
    }
    else
    {
        boolArr[index] = true;
    }

    index--;

    for( int i = numArrows_ - 1; i >= 0; i-- )
    {
        hItem = WM_GetDialogItem( hWin_, ARROW1_ID + i );
        pData = GetImageById( ID_IMAGE_5_IMAGE_0, &fileSize );

        if( boolArr[i] )
        {
            IMAGE_SetBMP( hItem, pData, fileSize );
        }
        else
        {
            IMAGE_SetBMP( hItem, 0, 0 );
        }
    }

}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetRightArrows ( WM_HWIN hWin_, int numArrows_ )
{
    WM_HWIN hItem;
    const void *pData;
    U32 fileSize;

    bool sameScreenFlag = FALSE;
    UIM_GetScreenFlag( &sameScreenFlag );

    static bool boolArr[MAX_NUM_RIGHT_ARROWS];

    static int index = 0;

    if( ( index >= numArrows_ ) ||
        ( !sameScreenFlag ) )
    {
        index = 0;
        memset( boolArr, 0, sizeof( boolArr ) );
    }
    else
    {
        boolArr[index] = true;
        index++;
    }

    for( int i = 0; i < numArrows_; i++ )
    {
        hItem = WM_GetDialogItem( hWin_, ARROW1_ID + i );
        pData = GetImageById( ID_IMAGE_10_IMAGE_0, &fileSize );

        if( boolArr[i] )
        {
            IMAGE_SetBMP( hItem, pData, fileSize );
        }
        else
        {
            IMAGE_SetBMP( hItem, 0, 0 );
        }
    }

}

////////////////////////////////////////////////////////////////////////////////
void UIM_AddLogoImage ( WM_HWIN hWin_ )
{
    WM_HWIN hItem;

    const void *pData;
    U32 fileSize;

    hItem = WM_GetDialogItem( hWin_, LOGO_IMAGE_ID );
    pData = GetImageById( ID_IMAGE_4_IMAGE_0, &fileSize );
    IMAGE_SetBMP( hItem, pData, fileSize );
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetActionLine ( WM_HWIN hWin_  )
{
    WM_HWIN hItem;
    ERROR_T err = ERR_NONE;
    int guiErr = 0;
    char textString[MAX_NUM_LINE_CHARS];
    bool primedFlag = false;
    RATE_VALUE_T flowRate = ML_0;
    FONT_ID_T font = FL;
    SCREEN_ID_T screen;

    UIM_GetScreenNo( &screen );

    memset( textString, 0, MAX_NUM_LINE_CHARS );

    if( screen == SCREEN_16 ) //PRIMING2
    {
        err = UIM_GetPrimedFlag( &primedFlag );

        if( primedFlag )
        {
            err = LM_GetString( &textString[0], ACTION_LINE, screen, &font, false );
            UIM_ScreenFuncTrace( err, __LINE__ );
        }
    }
    else
    {
        uint16_t currStep;
        UIM_GetStep( &currStep );

        if( screen == SCREEN_29_3 )
        {
            PROGRAM_INFO_T currProg;

            UIM_GetCurrProg( &currProg );
            UIM_GetStep( &currStep );

            if( currStep == HY_STEP_INDEX )
            {
                PM_GetHyStepFlowrate( &flowRate );
            }
            else
            {
                PM_GetIgStepFlowrate( ( uint8_t ) currStep, &flowRate );
            }

            if( currProg.ProgStepFlowRate[currStep] == flowRate )
            {
                err = LM_GetString( &textString[0], ACTION_LINE, screen, &font, false );
                UIM_ScreenFuncTrace( err, __LINE__ );
            }
            else
            {
                err = LM_GetString( &textString[0], ACTION_LINE_OPTION, screen, &font, false );
                UIM_ScreenFuncTrace( err, __LINE__ );
            }
        }
        else
        {
            err = LM_GetString( &textString[0], ACTION_LINE, screen, &font, false );
            UIM_ScreenFuncTrace( err, __LINE__ );
        }
    }

    hItem = WM_GetDialogItem( hWin_, ACTION_LINE_ID );
    TEXT_SetFont( hItem, FONT_ARR[font] );
    TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );

    guiErr = TEXT_SetText( hItem, textString );
    if( guiErr )
    {
        UIM_ScreenFuncTrace( err, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetHdrProperties ( WM_HWIN hItem_ )
{
    HEADER_SKINFLEX_PROPS props;
    HEADER_GetSkinFlexProps( &props, 0 );

    INFO_T info;
    UIM_GetInfoValue( &info );

    if( info == IS_INFO )
    {
        HEADER_SetTextColor( hItem_, GUI_WHITE );

        props.aColorUpper[0] = GUI_BLACK;
        props.aColorUpper[1] = GUI_BLACK;
        props.aColorLower[0] = GUI_BLACK;
        props.aColorLower[1] = GUI_BLACK;
    }
    else
    {
        HEADER_SetTextColor( hItem_, GUI_BLACK );

        props.aColorUpper[0] = GUI_WHITE;
        props.aColorUpper[1] = GUI_WHITE;
        props.aColorLower[0] = GUI_WHITE;
        props.aColorLower[1] = GUI_WHITE;

    }

    HEADER_SetSkinFlexProps( &props, 0 );
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetStepNumber ( WM_HWIN hWin_, LINE_NUM_T line_  )
{
    uint16_t currStep = 0;

    WM_HWIN hItem;

    char stepString[MAX_NUM_LINE_CHARS];
    char numString[2];
    SCREEN_ID_T screen;
    ERROR_T internalErr = ERR_NONE;
    PROGRAM_INFO_T currProg;
    FONT_ID_T font = FL;
    int id = 0;

    UIM_GetScreenNo( &screen );

    memset( stepString, 0, MAX_NUM_LINE_CHARS );
    memset( numString, 0, 2 );

    UIM_DetermineID( &id, line_ );

    internalErr = LM_GetString( stepString, line_, screen, &font, false );
    UIM_ScreenFuncTrace( internalErr, __LINE__ );

    UIM_GetCurrProg( &currProg );

    hItem = WM_GetDialogItem( hWin_, id );

    if( hItem == 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
    }
    else
    {

        UIM_GetStep( &currStep );
        sprintf( &numString[0], "%d", currStep );

        if( screen == SCREEN_31_1 ) //STEP_ENDING_S
        {
            //Must set the height after changing the font to ensure that it maintains what was originally planned.
            HEADER_SetHeight( hItem, HDR_HEIGHT );

            internalErr = LM_GetString( stepString, line_, screen, &font, false );
            UIM_ScreenFuncTrace( internalErr, __LINE__ );

            HEADER_SetFont( hItem, FONT_ARR[font] );

            internalErr = LM_SetDynamicStrData( stepString, numString );
            UIM_ScreenFuncTrace( internalErr, __LINE__ );

            HEADER_AddItem( hItem, HDR_WIDTH, stepString, GUI_TA_LEFT );
            memset( stepString, 0, MAX_NUM_LINE_CHARS );
        }
        else
        {

            internalErr = LM_GetString( stepString, line_, screen, &font, false );
            UIM_ScreenFuncTrace( internalErr, __LINE__ );

            internalErr = LM_SetDynamicStrData( stepString, numString ); //Set dynamic data twice since there is more than 1 set of XXX
            UIM_ScreenFuncTrace( internalErr, __LINE__ );

            sprintf( &numString[0], "%d", currProg.ProgIgSteps );
            internalErr = LM_SetDynamicStrData( stepString, numString );
            UIM_ScreenFuncTrace( internalErr, __LINE__ );

            (void)TEXT_SetText( hItem, stepString );

            TEXT_SetFont( hItem, FONT_ARR[font] );

            TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );
        }
    }


}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetFooter ( WM_HWIN hWin_  )
{
    WM_HWIN hItem;

    ERROR_T err = ERR_NONE;
    SCREEN_ID_T screen;
    char textString[MAX_NUM_LINE_CHARS];
    char tempStr[MAX_NUM_LINE_CHARS];
    FONT_ID_T font = FL;

    UIM_GetScreenNo( &screen );

    memset( textString, 0, MAX_NUM_LINE_CHARS );
    memset( tempStr, 0, MAX_NUM_LINE_CHARS );

    err = LM_GetString( tempStr, ACTION_LINE, screen, &font, false );
    UIM_ScreenFuncTrace( err, __LINE__ );

    strcat( textString, tempStr );

    hItem = WM_GetDialogItem( hWin_, FT_TITLE_ID );
    HEADER_SetFont( hItem, FONT_ARR[font] );

    HEADER_AddItem( hItem, SCREEN_WIDTH, textString, GUI_TA_HCENTER | GUI_TA_VCENTER );


}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetTextBox ( WM_HWIN hWin_ )
{
    WM_HWIN hItem;

    LINESET_INFO_T needleSet = NO_LINESET;
    char textString[MAX_NUM_LINE_CHARS];
    FONT_ID_T font = FL;
    ERROR_T err = ERR_NONE;
    int guiErr = 0;
    SCREEN_ID_T screen;

    UIM_GetScreenNo( &screen );

    memset( textString, 0, MAX_NUM_LINE_CHARS );

    hItem = WM_GetDialogItem( hWin_, TEXTBOX_ID );

    if( screen == SCREEN_22_1 )       // REPLACE_NEEDLE,
    {
        TEXT_SetTextAlign( hItem, GUI_TA_VCENTER );
    }
    else
    {
        TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );
    }

    //Check if one of the screens that contain plural language content
    if( ( screen == SCREEN_16)   ||   //PRIMING2
        ( screen == SCREEN_16_I) ||   //PRIMING2_INFO
        ( screen == SCREEN_18)   ||   //INSERT_NEEDLE,
        ( screen == SCREEN_22)   ||   //REMOVE_NEEDLE_BLOOD,
        ( screen == SCREEN_38) )      //REMOVE_NEEDLE_FLUSH
    {

        err = HALL_GetNeedleInfo( &needleSet );
        UIM_ScreenFuncTrace( err, __LINE__ );

        //if it does potentially contain plural language content, then check that there are actually bifurcated needles.
        if( needleSet == BIFURCATED_NEEDLE_LINESET )
        {
            err = LM_GetString( &textString[0], TEXT_BOX, screen, &font, true );
            UIM_ScreenFuncTrace( err, __LINE__ );
        }
        else
        {
            err = LM_GetString( &textString[0], TEXT_BOX, screen, &font, false );
            UIM_ScreenFuncTrace( err, __LINE__ );
        }
    }
    else //if it is not a screen that contains plural language content, then continue as normal
    {
        err = LM_GetString( &textString[0], TEXT_BOX, screen, &font, false );
        UIM_ScreenFuncTrace( err, __LINE__ );

        if( strstr( textString, DYNAMIC_DATA_STR ) != NULL ) //Check if there is dynamic data.
        {
            uint16_t currStep = 0;

            UIM_GetStep( &currStep );

            if( currStep > 0 )
            {
                LM_SetDynamicStrData( textString, LG_STR );
            }
            else
            {
                LM_SetDynamicStrData( textString, HY_STR );
            }
        }

    }

    TEXT_SetFont( hItem, FONT_ARR[font] );
    TEXT_SetWrapMode( hItem, GUI_WRAPMODE_WORD );

    guiErr = TEXT_SetText( hItem, textString );
    if( guiErr )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetTextBox2 ( WM_HWIN hWin_, const char *text_ )
{
    WM_HWIN hItem;

    int guiErr = 0;

    SCREEN_ID_T screen;
    UIM_GetScreenNo( &screen );

    hItem = WM_GetDialogItem( hWin_, TEXTBOX_ID );
    TEXT_SetFont( hItem, FONT_VERSION );

    if( screen == SCREEN_22_1 )
    {
        TEXT_SetTextAlign( hItem, GUI_TA_VCENTER );
    }
    else
    {
        TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );
    }

    guiErr = TEXT_SetText( hItem, text_ );
    if( guiErr )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
    }
    TEXT_SetWrapMode( hItem, GUI_WRAPMODE_NONE );
}

//////////////////////////////////////////////////////////////////////////////////
void UIM_SetPasscodeTextBox ( WM_HWIN hWin_ )
{
    WM_HWIN hItem;
    char passcodeIndex = PASSCODE_COL1;
    ERROR_T err = ERR_NONE;

    PASSCODE_T passcodeData;
    err = UIM_GetPasscodeInfo( &passcodeData );
    UIM_ScreenFuncTrace( err, __LINE__ );
    char passCodeValue;
    for( passcodeIndex = PASSCODE_COL1; passcodeIndex < NUM_OF_PASSCOD_COL; passcodeIndex++ )
    {
        hItem = WM_GetDialogItem( hWin_, (TEXTBOX_ID + passcodeIndex) );
        TEXT_SetFont( hItem, FONT_ARR[FL] );    //Hardcoded to large font for all languages
        if( passcodeIndex < passcodeData.PassCurrentCol )
        {
            passCodeValue = passcodeData.PassColumn[passcodeIndex] + '0';
            TEXT_SetText( hItem, (const char *)&passCodeValue );
        }
        else
        {
            TEXT_SetText( hItem, PASSCODE_DEFAULT_TEXT );
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetBrightnessImage ( WM_HWIN hWin_ )
{
    WM_HWIN hItem;
    const void *pData;
    U32 fileSize;
    U8 imageNo = 0;

    hItem = WM_GetDialogItem( hWin_, LOW_SETTING_IMAGE_ID );
    pData = GetImageById( ID_IMAGE_6_IMAGE_0, &fileSize );
    IMAGE_SetBMP( hItem, pData, fileSize );

    hItem = WM_GetDialogItem( hWin_, HIGH_SETTING_IMAGE_ID );
    pData = GetImageById( ID_IMAGE_7_IMAGE_0, &fileSize );
    IMAGE_SetBMP( hItem, pData, fileSize );

    //Set Squeare 10 Images
    for( imageNo = 0; imageNo < MAX_SCROLL_LEVEL; imageNo++ )
    {
        hItem = WM_GetDialogItem( hWin_, (SQUARE_IMAGE_0 + imageNo) );
        pData = GetImageById( ID_IMAGE_8_IMAGE_0, &fileSize );
        IMAGE_SetBMP( hItem, pData, fileSize );
    }

    if( s_BrightnessValue > MAX_VOL_BRIGHNESS_LEVEL )  // if got invalid data from flash
    {
        s_BrightnessValue = DEFAULT_VOL_BRIGHT_LVLS;    // set default brightness value
    }
    for( imageNo = 0; imageNo < s_BrightnessValue; imageNo++ )
    {
        hItem = WM_GetDialogItem( hWin_, (SQUARE_IMAGE_0 + imageNo) );
        pData = GetImageById( ID_IMAGE_9_IMAGE_0, &fileSize );
        IMAGE_SetBMP( hItem, pData, fileSize );
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetVolumeImage ( WM_HWIN hWin_ )
{
    WM_HWIN hItem;
    const void *pData;
    U32 fileSize;
    U8 volumeNo = 0;

    hItem = WM_GetDialogItem( hWin_, LOW_SETTING_IMAGE_ID );
    pData = GetImageById( ID_IMAGE_11_IMAGE_0, &fileSize );
    IMAGE_SetBMP( hItem, pData, fileSize );

    hItem = WM_GetDialogItem( hWin_, HIGH_SETTING_IMAGE_ID );
    pData = GetImageById( ID_IMAGE_12_IMAGE_0, &fileSize );
    IMAGE_SetBMP( hItem, pData, fileSize );

    //Set Squeare 10 Images
    for( volumeNo = 0; volumeNo < MAX_SCROLL_LEVEL; volumeNo++ )
    {
        hItem = WM_GetDialogItem( hWin_, (SQUARE_IMAGE_0 + volumeNo) );
        pData = GetImageById( ID_IMAGE_8_IMAGE_0, &fileSize );
        IMAGE_SetBMP( hItem, pData, fileSize );
    }

    if( s_VolumeValue > MAX_VOL_BRIGHNESS_LEVEL )  // if got invalid data from flash
    {
        s_VolumeValue = DEFAULT_VOL_BRIGHT_LVLS;     // set default volume value
    }

    //Set Volume screen index accordingly Current Volume
    for( volumeNo = 0; volumeNo < s_VolumeValue; volumeNo++ )
    {
        hItem = WM_GetDialogItem( hWin_, (SQUARE_IMAGE_0 + volumeNo) );
        pData = GetImageById( ID_IMAGE_9_IMAGE_0, &fileSize );
        IMAGE_SetBMP( hItem, pData, fileSize );
    }

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_GetPluralLangInfo ( bool *const status_ )
{
    ERROR_T err = ERR_NONE;
    LINESET_INFO_T needleSet = NO_LINESET;

    SCREEN_ID_T screen;
    UIM_GetScreenNo( &screen );

    if( status_ == NULL )
    {
        UIM_ScreenFuncTrace( ERR_PARAMETER, __LINE__ );
        return;
    }

    if( ( screen == SCREEN_22 ) ||   //REMOVE_NEEDLE_BLOOD
        ( screen == SCREEN_23 ) ||   //START_HY
        ( screen == SCREEN_38 ) )    //REMOVE_NEEDLE_FLUSH

    {
        err = HALL_GetNeedleInfo( &needleSet );
        UIM_ScreenFuncTrace( err, __LINE__ );

        //if it does potentially contain plural language content, then check that there are actually bifurcated needles.
        if( needleSet == BIFURCATED_NEEDLE_LINESET )
        {
            *status_ = true;
        }
        else
        {
            *status_ = false;
        }
    }
    else
    {
        *status_ = false;
    }
}

////////////////////////////////////////////////////////////////////////////////
// This function is meant to display a numbered list on the screen.
void UIM_SetListText ( WM_HWIN hWin_ )
{
    WM_HWIN hItem;
    ERROR_T err = ERR_NONE;
    int guiErr = 0;
    size_t charLen = 0;
    char textStringCat[MAX_NUM_LINE_CHARS] = {0};
    int id = LINE1_TEXT_ID;
    char *string = NULL;
    size_t totalStrLen = 0;
    char textString[MAX_NUM_LINE_CHARS];
    bool langPluralStatus = false;
    FONT_ID_T font = FL;
    SCREEN_ID_T screen;

    UIM_GetScreenNo( &screen );

    for( LINE_NUM_T i = LINE1_TEXT; i <= LINE3_TEXT; i++ )
    {
        memset( textString, 0, MAX_NUM_LINE_CHARS );

        hItem = WM_GetDialogItem( hWin_, id );

        textString[0] = (int)( ( i - LINE1_TEXT ) + 1 ) + '0';
        textString[1] = '.';

        UIM_GetPluralLangInfo( &langPluralStatus );
        err = LM_GetString( &textString[2], i, screen, &font, langPluralStatus );
        UIM_ScreenFuncTrace( err, __LINE__ );

        string = strstr( &textString[0], END_CHECK_STRING );
        totalStrLen = strnlen( textString, MAX_NUM_LINE_CHARS );

        if( string != NULL )
        {
            charLen = strnlen( string, MAX_NUM_LINE_CHARS );
        }

        //Check for remain string
        if( charLen > END_CHECK_STRING_SIZE )
        {
            if( string != NULL )
            {
                memcpy( &textStringCat[0], &string[END_CHECK_STRING_SIZE], charLen );
            }
            //Add space for adjust string
            textString[totalStrLen + END_CHECK_STRING_SIZE - charLen] = ' ';
            textString[totalStrLen + END_CHECK_STRING_SIZE + 1 - charLen] = ' ';
            textString[totalStrLen + END_CHECK_STRING_SIZE + 2 - charLen] = ' ';
            memcpy( &textString[(totalStrLen + END_CHECK_STRING_SIZE + 3) - charLen], textStringCat, charLen );
        }

        if( err == ERR_NONE )
        {
            guiErr = TEXT_SetText( hItem, textString );
            TEXT_SetFont( hItem, FONT_ARR[font] );
            TEXT_SetTextAlign( hItem, GUI_TA_VCENTER );
            if( guiErr )
            {
                UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
            }

            id++;
        }
        else
        {
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetText ( WM_HWIN hWin_, LINE_NUM_T line_, int alignment_ )
{
    WM_HWIN hItem;
    ERROR_T err = ERR_NONE;
    char textString[MAX_NUM_LINE_CHARS];
    char tempStr[MAX_NUM_LINE_CHARS];
    int id = 0;
    size_t length = 0;
    FONT_ID_T font = FL;
    SCREEN_ID_T screen;

    UIM_GetScreenNo( &screen );

    memset( textString, 0, MAX_NUM_LINE_CHARS );
    memset( tempStr, 0, MAX_NUM_LINE_CHARS );

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( hWin_, id );

    err = LM_GetString( &textString[0], line_, screen, &font, false );
    UIM_ScreenFuncTrace( err, __LINE__ );

    TEXT_GetText( hItem, tempStr, MAX_NUM_LINE_CHARS );

    if( tempStr[0] != '\0' )
    {
        strncat( tempStr, " ", 1 );
        length = strnlen( tempStr, MAX_NUM_LINE_CHARS );
        strncat( tempStr, textString, MAX_NUM_LINE_CHARS - length );

        (void)TEXT_SetText( hItem, tempStr );
    }
    else
    {
        (void)TEXT_SetText( hItem, textString );
    }

    TEXT_SetFont( hItem, FONT_ARR[font] );

    TEXT_SetTextAlign( hItem, alignment_ );

    TEXT_SetWrapMode( hItem, GUI_WRAPMODE_WORD );
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetLineTextFont ( WM_HWIN hWin_, LINE_NUM_T line_, const GUI_FONT *font_ )
{
    WM_HWIN hItem;

    ERROR_T err = ERR_NONE;
    SCREEN_ID_T screen;
    char textString[MAX_NUM_LINE_CHARS];
    char textString2[MAX_NUM_LINE_CHARS];
    FONT_ID_T font = FL;

    UIM_GetScreenNo( &screen );

    memset( textString, 0, MAX_NUM_LINE_CHARS );

    int id = 0;

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( hWin_, id );

    err = LM_GetString( &textString[0], line_, screen, &font, false );
    UIM_ScreenFuncTrace( err, __LINE__ );

    TEXT_GetText( hItem, textString2, MAX_NUM_LINE_CHARS );

    if( textString2[0] != '\0' )
    {
        strncat( textString2, " ", 1 );
        strncat( textString2, textString, MAX_NUM_LINE_CHARS );

        (void)TEXT_SetText( hItem, textString2 );
    }
    else
    {
        (void)TEXT_SetText( hItem, textString );
    }

    TEXT_SetFont( hItem, font_ );

    TEXT_SetTextAlign( hItem, GUI_TA_LEFT | GUI_TA_VCENTER );

}

////////////////////////////////////////////////////////////////////////////////
static void UIM_CreateStepReviewListView ( WM_HWIN hWin_, unsigned int numRows_ )
{
    ERROR_T err = ERR_NONE;

    WM_HWIN hItem;
    SCREEN_ID_T screen;
    int32_t size = 0;
    int32_t rowSize = 0;
    FONT_ID_T font = FL;

    UIM_GetScreenNo( &screen );

    char textString[MAX_NUM_LINE_CHARS];

    hItem = WM_GetDialogItem( hWin_, STEPVIEW_ID );

    err = LM_GetString( textString, OPTION1, screen, &font, false );
    UIM_ScreenFuncTrace( err, __LINE__ );
    (void)LISTVIEW_AddColumn( hItem, 75, textString, GUI_TA_HCENTER | GUI_TA_VCENTER );
    memset( textString, 0, MAX_NUM_LINE_CHARS );

    err = LM_GetString( textString, OPTION2, screen, &font, false );
    UIM_ScreenFuncTrace( err, __LINE__ );
    (void)LISTVIEW_AddColumn( hItem, 125, textString, GUI_TA_HCENTER | GUI_TA_VCENTER );
    memset( textString, 0, MAX_NUM_LINE_CHARS );

    err = LM_GetString( textString, OPTION3, screen, &font, false );
    UIM_ScreenFuncTrace( err, __LINE__ );
    (void)LISTVIEW_AddColumn( hItem, 199, textString, GUI_TA_HCENTER | GUI_TA_VCENTER );
    memset( textString, 0, MAX_NUM_LINE_CHARS );

    unsigned int i = 0;

    for( i = 0; i < numRows_; i++ )
    {
        (void)LISTVIEW_AddRow( hItem, NULL );
    }

    if( numRows_ != MAX_LISTVIEW_ROWS )
    {
        size = LISTVIEW_HGT;
        rowSize = LISTVIEW_HGT / ( MAX_LISTVIEW_ROWS + 1 ); // Rows + one header
        size = size - ( rowSize * ( (int32_t)MAX_LISTVIEW_ROWS - (int32_t)numRows_ ) );
        WM_SetYSize( hItem, ( int ) size );
    }

    (void)LISTVIEW_SetGridVis( hItem, 1 );

    HEADER_Handle hHeader = LISTVIEW_GetHeader( hItem );
    HEADER_SetFont( hHeader, FONT_ARR[FM] ); //The header in the listview should always be the same size.
    LISTVIEW_SetFont( hItem, FONT_ARR[FM] ); //The numbers in the listview should always be the same size.

}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetProgBar ( WM_HWIN hWin_, bool showFlag_ )
{
    WM_HWIN hItem;
    SCREEN_ID_T screen;
    PROGRAM_INFO_T prog;
    float volumeInfused = 0.0f;
    float percentage = 0.0f;
    uint8_t battPercent = 0;

    UIM_GetScreenNo( &screen );

    bool sameScreenFlag = false;
    UIM_GetScreenFlag( &sameScreenFlag );

    hItem = WM_GetDialogItem( hWin_, PROGBAR_ID_0 );

    PROGBAR_SetText( hItem, "" );   //Remove text from within the progbar

    if( showFlag_ )
    {
        UIM_GetCurrProg( &prog );

        PROGBAR_SetBarColor( hItem, 0, GUI_BLACK );

        if( ( screen == SCREEN_30_1I ) || //PROG_PAUSE_INFO1
            ( screen == SCREEN_29_1I ) )  //PROG_INFO1
        {
            uint16_t currStep = 0;

            UIM_GetStep( &currStep );

            UIM_GetVolInfused( &volumeInfused );

            if( currStep == 0 )
            {
                percentage = ( volumeInfused / prog.ProgHyDosage ) * MAX_PERCENT;
            }
            else
            {
                percentage = ( volumeInfused / prog.ProgIgDosage ) * MAX_PERCENT;
            }

            PROGBAR_SetValue( hItem, ( int )percentage );
        }
        else if( ( screen == SCREEN_30_3I ) || //PROG_PAUSE_INFO3
                 ( screen == SCREEN_29_3I ) )  //PROG_INFO3
        {
            UIM_GetBattLeft( &battPercent );
            PROGBAR_SetValue( hItem, ( int )battPercent );
        }
        else if( screen == SCREEN_0 ) //POWER_OFF_S
        {

            if( !sameScreenFlag )
            {
                s_PowerProgressCounter = 0;
            }

            s_PowerProgressCounter = s_PowerProgressCounter + PWR_BAR_INC_AMNT + 1; // the 1 is to account for any remainder that was lost and since accuracy is not
            //important with this progress bar.
            if( s_PowerProgressCounter > 100 )
            {
                s_PowerProgressCounter = 100;
            }

            PROGBAR_SetValue( hItem, ( int )s_PowerProgressCounter );
        }


        //This code is to change the color of the frame around the progress bar.
        PROGBAR_SKINFLEX_PROPS props;

        PROGBAR_GetSkinFlexProps( &props, 0 );
        props.aColorUpperL[0] = GUI_BLACK;
        props.aColorUpperL[1] = GUI_BLACK;
        props.ColorFrame = GUI_BLACK;
        PROGBAR_SetSkinFlexProps( &props, 0 );
        WM_InvalidateWindow( hWin_ );
    }
    else
    {
        PROGBAR_SKINFLEX_PROPS props;

        PROGBAR_SetValue( hItem, 0 );
        PROGBAR_GetSkinFlexProps( &props, 0 );
        props.ColorFrame = GUI_WHITE;
        PROGBAR_SetSkinFlexProps( &props, 0 );
        WM_InvalidateWindow( hWin_ );
    }

}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetVolLeft2 ( WM_HWIN hWin_, LINE_NUM_T line_ )
{
    WM_HWIN hItem;
    SCREEN_ID_T screen;
    char textString[MAX_NUM_LINE_CHARS];
    char volString[MAX_DIGITS];
    memset( textString, 0, MAX_NUM_LINE_CHARS );
    memset( volString, 0, MAX_DIGITS );

    float volumeInfused = 0.0f;
    uint8_t battLife = 0;

    UIM_GetScreenNo( &screen );

    int id;

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( hWin_, id );

    TEXT_GetText( hItem, textString, MAX_NUM_LINE_CHARS );

    if( ( screen == SCREEN_30_1I ) ||   //PROG_PAUSE_INFO1
        ( screen == SCREEN_29_1I ) )    //PROG_INFO1
    {
        UIM_GetVolInfused( &volumeInfused );

        snprintf( volString, MAX_DIGITS, "%d", ( int )volumeInfused ); //lint !e516 data type for argument 4 can be anything
        strncat( textString, " ", 1 );
        strncat( textString, volString, sizeof( volString ) );
        strncat( textString, " ", 1 );
        strncat( textString, ML_STR, sizeof( ML_STR ) );   //Using a hardcoded string here is ok, since it is an international measurement abbrev.
    }
    else if( ( screen == SCREEN_30_3I ) ||  //PROG_PAUSE_INFO3
             ( screen == SCREEN_29_3I ) )   //PROG_INFO3
    {
        UIM_GetBattLeft( &battLife );

        snprintf( volString, MAX_DIGITS, "%d", battLife ); //lint !e516 data type for argument 4 can be anything
        strncat( textString, " ", 1 );
        strncat( textString, volString, sizeof( volString ) );
        strncat( textString, "%", 1 );
    }

    TEXT_SetText( hItem, textString );

    TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );


}

////////////////////////////////////////////////////////////////////////////////
void UIM_DosageAction ( GUI_HWIN currWindow_, bool incrementFlag_, bool decrementFlag_ )
{
    WM_HWIN hItem;
    char textString[MAX_NUM_LINE_CHARS] = {0};

    int err = 0;
    float maxDosageValue = 0.0f;                       //Get max Dosage value from program manager
    float minDosageValue = 0.0f;                       //Get min Dosage value from program manager
    float incrementLevel = 0.0f;                       //Get Dosage increment Level value from program manager
    ERROR_T reportedError = ERR_NONE;

    reportedError = PM_GetDosageRange( &minDosageValue, &maxDosageValue, &incrementLevel );
    if( reportedError != ERR_NONE )
    {
        UIM_ScreenFuncTrace( reportedError, __LINE__ );     ///there is issue with UIM
        return;
    }
    //Increment Dosage Value
    if( incrementFlag_ == true )
    {
        if( s_CurrProgDosage < maxDosageValue )
        {
            s_CurrProgDosage = s_CurrProgDosage + incrementLevel;
        }
    }
    else if( decrementFlag_ == true )     //decrement Dosage Value
    {
        if( s_CurrProgDosage > minDosageValue )
        {
            s_CurrProgDosage = s_CurrProgDosage - incrementLevel;
        }
    }

    if( s_CurrProgDosage >= maxDosageValue ) // Max Dosage value
    {
        UIM_SetUpDownArrows( currWindow_, false, true ); //Display down arrow only
    }
    else if( s_CurrProgDosage <= minDosageValue ) //min Dosage value
    {
        UIM_SetUpDownArrows( currWindow_, true, false ); //Display up arrow only
    }
    else
    {
        UIM_SetUpDownArrows( currWindow_, true, true ); //Display up and down arrows
    }
    // Get Dosage values from program manager
    reportedError = PM_CalculateDosage( s_CurrProgDosage, &s_CurrProgHyDosage, &s_CurrProgIgDosage );
    if( reportedError != ERR_NONE )
    {
        s_CurrProgHyDosage = 0;
        s_CurrProgIgDosage = 0;
        UIM_ScreenFuncTrace( reportedError, __LINE__ );     ///there is issue with UIM
        return;
    }

    memset( textString, 0, MAX_NUM_LINE_CHARS );
    hItem = WM_GetDialogItem( currWindow_, TEXTBOX_ID );
    err = sprintf( textString, "%.1f %s", s_CurrProgDosage, GRAM_UNIT_STRING );
    if( err < 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
    }
    TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );
    TEXT_SetText( hItem, textString );
    TEXT_SetFont( hItem, FONT_ARR[FM] );

    memset( textString, 0, MAX_NUM_LINE_CHARS );
    hItem = WM_GetDialogItem( currWindow_, LINE2_TEXT_ID );

    err = sprintf( textString, "HY:%.2fml | IG:%.fml", s_CurrProgHyDosage, s_CurrProgIgDosage );
    if( err < 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
    }

    TEXT_SetFont( hItem, FONT_ARR[FM] );
    TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );
    TEXT_SetText( hItem, textString );
}

////////////////////////////////////////////////////////////////////////////////
void UIM_UpDownArrowsRate ( bool *up_, bool *down_, RATE_VALUE_T min_, RATE_VALUE_T max_, RATE_VALUE_T curr_ )
{
    if( ( up_ == NULL ) || ( down_ == NULL ) || ( min_ > ML_900 ) || ( max_ > ML_900 ) || ( curr_ > ML_900 ) )
    {
        UIM_ScreenFuncTrace( ERR_PARAMETER, __LINE__ );
        return;
    }

    if( min_ == max_ )
    {
        *up_ = false;
        *down_ = false;
    }
    else if( curr_ == max_ ) // Max flowrate
    {
        *up_ = false;
        *down_ = true;
    }
    else if( curr_ == min_ ) //min flowrate
    {
        *up_ = true;
        *down_ = false;
    }
    else
    {
        *up_ = true;
        *down_ = true;
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_HyFlowRateAction ( GUI_HWIN currWindow_, bool incrementFlag_, bool decrementFlag_ )
{
    WM_HWIN hItem;
    char textString[MAX_NUM_LINE_CHARS] = {0};
    RATE_VALUE_T minHyFlowRate;
    RATE_VALUE_T maxHyFlowRate;
    ERROR_T reportedError = ERR_NONE;
    uint8_t incrementLevel = 0;               //Get value from program manager
    bool upFlag = false;
    bool downFlag = false;
    char strBuff[MAX_NUM_LINE_CHARS] = {0};
    FONT_ID_T font = FL;

    reportedError = PM_GetHyFlowrateRange( &minHyFlowRate, &maxHyFlowRate, &incrementLevel );
    if( reportedError != ERR_NONE )
    {
        UIM_ScreenFuncTrace( reportedError, __LINE__ );     ///there is issue with UIM
        return;
    }
    //Increment Hy FlowRate Value
    if( incrementFlag_ == true )
    {
        if( s_CurrHyFlowRateValue < maxHyFlowRate )
        {
            s_CurrHyFlowRateValue = ( RATE_VALUE_T )( s_CurrHyFlowRateValue + incrementLevel );
        }
    }
    else if( decrementFlag_ == true )     //decrement Hy FlowRate Value
    {
        if( s_CurrHyFlowRateValue > minHyFlowRate )
        {
            s_CurrHyFlowRateValue = ( RATE_VALUE_T )( s_CurrHyFlowRateValue - incrementLevel );
        }
    }


    memset( textString, 0, MAX_NUM_LINE_CHARS );

    //Centred Align logic to display text
    hItem = WM_GetDialogItem( currWindow_, TEXTBOX_ID );

    reportedError = LM_GetString( &strBuff[0], TEXT_BOX, SCREEN_48, &font, false );
    UIM_ScreenFuncTrace( reportedError, __LINE__ );

    sprintf( textString, "%d %s", RATE_VALUE_ARR[s_CurrHyFlowRateValue], strBuff );

    TEXT_SetFont( hItem, FONT_ARR[FL] );
    TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );
    TEXT_SetText( hItem, textString );

    //Can not allow user to go above original rate. So using origProg instead of maxFlow
    UIM_UpDownArrowsRate( &upFlag, &downFlag, minHyFlowRate, maxHyFlowRate, s_CurrHyFlowRateValue );

    UIM_SetUpDownArrowsDynamic( currWindow_, upFlag, downFlag, ( DYNAMIC_ARROW_X_POS_TEMP_D ),
                                DYNAMIC_ARROW_Y_POS, TEXT_BOX, GUI_TA_LEFT );      //Display down arrow only
}

////////////////////////////////////////////////////////////////////////////////
void UIM_IgStepAction ( GUI_HWIN currWindow_, bool incrementFlag_, bool decrementFlag_ )
{
    WM_HWIN hItem;
    char textString[MAX_NUM_LINE_CHARS] = {0};
    uint8_t maxIgStepValue = 0;                //Get value from program manager
    uint8_t minIgStepValue = 0;               //Get value from program manager
    uint8_t incrementLevel = 0;               //Get value from program manager
    ERROR_T reportedError = ERR_NONE;

    reportedError = PM_GetIgStepRange( &minIgStepValue, &maxIgStepValue, &incrementLevel );
    if( reportedError != ERR_NONE )
    {
        UIM_ScreenFuncTrace( reportedError, __LINE__ );     ///there is issue with UIM
        return;
    }

    //Increment Ig Step Value
    if( incrementFlag_ == true )
    {
        if( s_CurrProgIgSteps < maxIgStepValue )
        {
            s_CurrProgIgSteps = s_CurrProgIgSteps + incrementLevel;
        }
    }
    else if( decrementFlag_ == true )     //decrement Ig Step Value
    {
        if( s_CurrProgIgSteps > minIgStepValue )
        {
            s_CurrProgIgSteps = s_CurrProgIgSteps - incrementLevel;
        }
    }
    if( s_CurrProgIgSteps == maxIgStepValue ) // Max Ig Step
    {
        UIM_SetUpDownArrows( currWindow_, false, true ); //Display down arrow only
    }
    else if( s_CurrProgIgSteps == minIgStepValue ) //min Ig Step value
    {
        UIM_SetUpDownArrows( currWindow_, true, false ); //Display up arrow only
    }
    else
    {
        UIM_SetUpDownArrows( currWindow_, true, true ); //Display up and down arrows
    }

    hItem = WM_GetDialogItem( currWindow_, TEXTBOX_ID );
    textString[0] = s_CurrProgIgSteps + '0';
    TEXT_SetFont( hItem, FONT_ARR[FL] );
    TEXT_SetText( hItem, textString );
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetNeedlePlacementConfig ( WM_HWIN hWin_, LINE_NUM_T line_ )
{
    WM_HWIN hItem;

    ERROR_T err = ERR_NONE;
    int id = 0;


    PROGRAM_INFO_T currProg;

    char needleStr[MAX_NUM_LINE_CHARS];
    char textString[MAX_NUM_LINE_CHARS];

    memset( textString, 0, MAX_NUM_LINE_CHARS );
    memset( needleStr, 0, MAX_NUM_LINE_CHARS );

    UIM_GetCurrProg( &currProg );

    err = LM_GetYesNoString( (bool)currProg.NeedlePlacementCheckEnabled, needleStr  );
    UIM_ScreenFuncTrace( err, __LINE__ );

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( hWin_, id );
    if( hItem == 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
        return;
    }

    UIM_TextConcatenate( hItem, textString, needleStr );
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetFlushConfig ( WM_HWIN hWin_, LINE_NUM_T line_ )
{
    WM_HWIN hItem;

    ERROR_T err = ERR_NONE;
    int id = 0;

    PROGRAM_INFO_T currProg;

    UIM_GetCurrProg( &currProg );

    char flushStr[MAX_NUM_LINE_CHARS];
    char textString[MAX_NUM_LINE_CHARS];

    memset( textString, 0, MAX_NUM_LINE_CHARS );
    memset( flushStr, 0, MAX_NUM_LINE_CHARS );


    err = LM_GetYesNoString( (bool)currProg.FlushEnabled, flushStr );
    UIM_ScreenFuncTrace( err, __LINE__ );

    UIM_DetermineID( &id, line_ );

    hItem = WM_GetDialogItem( hWin_, id );
    if( hItem == 0 )
    {
        UIM_ScreenFuncTrace( ERR_GUI, __LINE__ );
        return;
    }

    UIM_TextConcatenate( hItem, textString, flushStr );
}

////////////////////////////////////////////////////////////////////////////////
void UIM_IgFlowrateAction ( GUI_HWIN currWindow_, bool incrementFlag_, bool decrementFlag_ )
{
    WM_HWIN hItem;
    char textString[MAX_NUM_LINE_CHARS] = {0};
    char igTextString[MAX_NUM_LINE_CHARS] = {0};
    char numString[MAX_NUM_LINE_CHARS] = {0};
    RATE_VALUE_T minIgFlowValue;                //Get the value from program manager
    RATE_VALUE_T maxIgFlowValue;                //Get the value from program manager
    ERROR_T reportedError = ERR_NONE;

    bool upFlag = false;
    bool downFlag = false;
    char strBuff[MAX_NUM_LINE_CHARS] = {0};
    FONT_ID_T font = FL;

    memset( igTextString, 0, MAX_NUM_LINE_CHARS );
    memset( numString, 0, MAX_NUM_LINE_CHARS );

    reportedError =  PM_GetIgFlowrateRange( &minIgFlowValue, &maxIgFlowValue );
    if( reportedError != ERR_NONE )
    {
        UIM_ScreenFuncTrace( reportedError, __LINE__ );     ///there is issue with UIM
        return;
    }

    //Increment Ig Step Value
    if( incrementFlag_ == true )
    {
        if( s_CurrIgFlowValue < maxIgFlowValue )
        {
            s_CurrIgFlowValue++;

            // ML_320 is an invalid per site flowrate
            if( s_CurrIgFlowValue == ML_320 )
            {
                s_CurrIgFlowValue++;
            }
        }
    }
    else if( decrementFlag_ == true )     //decrement Ig Step Value
    {
        if( s_CurrIgFlowValue > minIgFlowValue )
        {
            s_CurrIgFlowValue--;

            // ML_320 is an invalid per site flowrate
            if( s_CurrIgFlowValue == ML_320 )
            {
                s_CurrIgFlowValue--;
            }
        }
    }

    //display the current IG step number dynamically
    hItem = WM_GetDialogItem( currWindow_, LINE1_TEXT_ID );
    TEXT_GetText( hItem, &igTextString[0], MAX_NUM_LINE_CHARS );

    sprintf( &numString[0], "%d", (s_CurrIgStepScreenNoIndex + 1) );

    reportedError = LM_SetDynamicStrData( igTextString, numString );
    UIM_ScreenFuncTrace( reportedError, __LINE__ );

    TEXT_SetText( hItem, igTextString );

    //Display IG flow value value
    memset( textString, 0, MAX_NUM_LINE_CHARS );

    hItem = WM_GetDialogItem( currWindow_, TEXTBOX_ID );
    reportedError = LM_GetString( &strBuff[0], TEXT_BOX, SCREEN_50, &font, false );
    UIM_ScreenFuncTrace( reportedError, __LINE__ );

    sprintf( textString, "%d %s", RATE_VALUE_ARR[s_CurrIgFlowValue], strBuff );
    TEXT_SetFont( hItem, FONT_ARR[FL] );
    TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );
    TEXT_SetText( hItem, textString );


    //Can not allow user to go above original rate. So using origProg instead of maxFlow
    UIM_UpDownArrowsRate( &upFlag, &downFlag, minIgFlowValue, maxIgFlowValue, s_CurrIgFlowValue );

    UIM_SetUpDownArrowsDynamic( currWindow_, upFlag, downFlag, ( DYNAMIC_ARROW_X_POS_TEMP_D  ),
                                DYNAMIC_ARROW_Y_POS, TEXT_BOX, GUI_TA_LEFT );  //Display down arrow only

}

////////////////////////////////////////////////////////////////////////////////
void UIM_IgDurationAction ( GUI_HWIN currWindow_, bool incrementFlag_, bool decrementFlag_ )
{
    WM_HWIN hItem;
    char textString[MAX_NUM_LINE_CHARS] = {0};
    char numString[MAX_NUM_LINE_CHARS] = {0};
    char igTextString[MAX_NUM_LINE_CHARS] = {0};
    uint8_t minIgDurationValue = 0;                  //Get the value from program manager
    uint8_t maxIgDurationValue = 0;                  //Get the value from program manager
    uint8_t incrementLevel = 0;                  //Get value from program manager
    ERROR_T reportedError = ERR_NONE;
    char strBuff[MAX_NUM_LINE_CHARS] = {0};
    FONT_ID_T font = FL;

    memset( igTextString, 0, MAX_NUM_LINE_CHARS );
    memset( numString, 0, MAX_NUM_LINE_CHARS );

    reportedError =  PM_GetIgStepDurationRange( &minIgDurationValue, &maxIgDurationValue, &incrementLevel );
    if( reportedError != ERR_NONE )
    {
        UIM_ScreenFuncTrace( reportedError, __LINE__ );     ///there is issue with UIM
        return;
    }

    //Increment Ig Step Value
    if( incrementFlag_ == true )
    {
        if( s_CurrIgDurationValue < maxIgDurationValue )
        {
            s_CurrIgDurationValue = s_CurrIgDurationValue + incrementLevel;
        }
    }
    else if( decrementFlag_ == true )     //decrement Ig Step Value
    {
        if( s_CurrIgDurationValue > minIgDurationValue )
        {
            s_CurrIgDurationValue = s_CurrIgDurationValue - incrementLevel;
        }
    }

    //display the current IG step number dynamically
    hItem = WM_GetDialogItem( currWindow_, LINE1_TEXT_ID );
    TEXT_GetText( hItem, &igTextString[0], MAX_NUM_LINE_CHARS );

    sprintf( &numString[0], "%d", (s_CurrIgStepScreenNoIndex + 1) );

    reportedError = LM_SetDynamicStrData( igTextString, numString );
    UIM_ScreenFuncTrace( reportedError, __LINE__ );

    TEXT_SetText( hItem, igTextString );


    memset( textString, 0, MAX_NUM_LINE_CHARS );

    //Centred Align logic to display text
    hItem = WM_GetDialogItem( currWindow_, TEXTBOX_ID );

    reportedError = LM_GetString( &strBuff[0], TEXT_BOX, SCREEN_51, &font, false );
    UIM_ScreenFuncTrace( reportedError, __LINE__ );

    sprintf( textString, " %d %s", s_CurrIgDurationValue, strBuff );

    TEXT_SetFont( hItem, FONT_ARR[FL] );
    TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );
    TEXT_SetText( hItem, textString );

    //Disply arrow dynamically
    if( s_CurrIgDurationValue == maxIgDurationValue ) // Max Ig Duration
    {
        UIM_SetUpDownArrowsDynamic( currWindow_, false, true, IG_DYNAMIC_ARROW_X_POS,
                                    IG_DYNAMIC_ARROW_Y_POS, TEXT_BOX, GUI_TA_LEFT );              //Display down arrow only
    }
    else if( s_CurrIgDurationValue == minIgDurationValue ) //min Ig Duration value
    {
        UIM_SetUpDownArrowsDynamic( currWindow_, true, false, IG_DYNAMIC_ARROW_X_POS,
                                    IG_DYNAMIC_ARROW_Y_POS, TEXT_BOX, GUI_TA_LEFT );              //Display down arrow only
    }
    else
    {
        UIM_SetUpDownArrowsDynamic( currWindow_, true, true, IG_DYNAMIC_ARROW_X_POS,
                                    IG_DYNAMIC_ARROW_Y_POS, TEXT_BOX, GUI_TA_LEFT );              //Display down arrow only
    }

}

////////////////////////////////////////////////////////////////////////////////
void UIM_IgTimeIntervalAction ( GUI_HWIN currWindow_, bool incrementFlag_, bool decrementFlag_ )
{
    WM_HWIN hItem;
    char textString[MAX_NUM_LINE_CHARS] = {0};
    uint8_t minIgDurationValue = 0;                  //Get the value from program manager
    uint8_t maxIgDurationValue = 0;                  //Get the value from program manager
    uint8_t incrementLevel = 0;                  //Get value from program manager
    ERROR_T reportedError = ERR_NONE;
    char strBuff[MAX_NUM_LINE_CHARS] = {0};
    FONT_ID_T font = FL;

    reportedError =  PM_GetIgTimeIntervalRange( &minIgDurationValue, &maxIgDurationValue, &incrementLevel );
    if( reportedError != ERR_NONE )
    {
        UIM_ScreenFuncTrace( reportedError, __LINE__ );     ///there is issue with UIM
        return;
    }
    //Increment Ig Step Value
    if( incrementFlag_ == true )
    {
        if( s_CurrIgDurationValue < maxIgDurationValue )
        {
            s_CurrIgDurationValue = s_CurrIgDurationValue + incrementLevel;
        }
    }
    else if( decrementFlag_ == true )     //decrement Ig Step Value
    {
        if( s_CurrIgDurationValue > minIgDurationValue )
        {
            s_CurrIgDurationValue = s_CurrIgDurationValue - incrementLevel;
        }
    }

    //Centred Align logic to display text
    hItem = WM_GetDialogItem( currWindow_, TEXTBOX_ID );
    reportedError = LM_GetString( &strBuff[0], TEXT_BOX, SCREEN_51, &font, false );
    UIM_ScreenFuncTrace( reportedError, __LINE__ );

    sprintf( textString, " %d %s", s_CurrIgDurationValue, strBuff );

    TEXT_SetFont( hItem, FONT_ARR[FL] );
    TEXT_SetTextAlign( hItem, GUI_TA_HCENTER | GUI_TA_VCENTER );
    TEXT_SetText( hItem, textString );

    if( s_CurrIgDurationValue == maxIgDurationValue ) // Max Ig Duration
    {
        UIM_SetUpDownArrowsDynamic( currWindow_, false, true, IG_DYNAMIC_ARROW_X_POS,
                                    IG_DYNAMIC_ARROW_Y_POS, TEXT_BOX, GUI_TA_LEFT );              //Display down arrow only
    }
    else if( s_CurrIgDurationValue == minIgDurationValue ) //min Ig Duration value
    {
        UIM_SetUpDownArrowsDynamic( currWindow_, true, false, IG_DYNAMIC_ARROW_X_POS,
                                    IG_DYNAMIC_ARROW_Y_POS, TEXT_BOX, GUI_TA_LEFT );              //Display up arrow only
    }
    else
    {
        UIM_SetUpDownArrowsDynamic( currWindow_, true, true, IG_DYNAMIC_ARROW_X_POS,
                                    IG_DYNAMIC_ARROW_Y_POS, TEXT_BOX, GUI_TA_LEFT );             //Display up and down arrows
    }
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetVolumeLevel ( uint8_t *level_ )
{
    if( level_ == NULL )
    {
        return ERR_PARAMETER;
    }
    *level_ = s_VolumeValue;

    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetBrightnessLevel ( uint8_t *level_ )
{
    if( level_ == NULL )
    {
        return ERR_PARAMETER;
    }
    *level_ = s_BrightnessValue;

    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetDosageValues ( float *dosage_, float *hyValue_, float *igValue_ )
{
    if( ( dosage_ == NULL ) || ( hyValue_ == NULL ) ||  ( igValue_ == NULL ) )
    {
        return ERR_PARAMETER;
    }

    *dosage_ =  s_CurrProgDosage;
    *hyValue_ = s_CurrProgHyDosage;
    *igValue_ = s_CurrProgIgDosage;
    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetHyFlowRateValue ( RATE_VALUE_T *hyFlowRate_ )
{
    if( hyFlowRate_ == NULL )
    {
        return ERR_PARAMETER;
    }

    *hyFlowRate_ = s_CurrHyFlowRateValue;
    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetIgStepValue ( uint8_t *igStepLevel_ )
{
    if( igStepLevel_ == NULL )
    {
        return ERR_PARAMETER;
    }

    *igStepLevel_ = s_CurrProgIgSteps;
    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetDefaultProgramValue ( void  )
{
    //store current Hy FlowRate  with default
    s_CurrHyFlowRateValue = PM_HY_FlOW_RATE_DEFAULT_VALUE;
    // default Store IG Flow Rate data index value
    s_CurrIgFlowValue = PM_IG_FlOW_RATE_DEFAULT_VALUE;
    // default Store IG Duration data index value
    s_CurrIgDurationValue = PM_IG_TIME_DEFAULT_VALUE;
    //default Current Ig step index to store IG duration and IG flowrate
    s_CurrIgStepScreenNoIndex = PM_IG_STEP_DEFAULT_INDEX;
    s_CurrProgIgSteps = PM_IG_STEP_DEFAULT_VALUE;
    // Set default current Dosage Value
    s_CurrProgDosage = PM_DOSAGE_DEFAULT_VALUE;
}

//////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetCurrentIgValues ( uint8_t *currIgStepScreenNoIndex_, RATE_VALUE_T *igFlowValue_, uint8_t *currIgDurationValue_ )
{
    if( ( currIgStepScreenNoIndex_ == NULL ) || ( igFlowValue_ == NULL ) ||  ( currIgDurationValue_ == NULL ) )
    {
        return ERR_PARAMETER;
    }

    *currIgStepScreenNoIndex_ = s_CurrIgStepScreenNoIndex;
    *igFlowValue_ = s_CurrIgFlowValue;
    *currIgDurationValue_ = s_CurrIgDurationValue;
    return ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////////////
ERROR_T UIM_GetIgTimeValues ( uint8_t *currIgDurationValue_ )
{
    if( currIgDurationValue_ == NULL )
    {
        return ERR_PARAMETER;
    }

    *currIgDurationValue_ = s_CurrIgDurationValue;
    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetCurrentIgValues ( RATE_VALUE_T igFlowValue_, uint8_t currIgDurationValue_ )
{
    s_CurrIgFlowValue = igFlowValue_;
    s_CurrIgDurationValue = currIgDurationValue_;
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetCurrentIgStepNo ( uint8_t currIgStepScreenNoIndex_ )
{
    s_CurrIgStepScreenNoIndex = currIgStepScreenNoIndex_;
}

////////////////////////////////////////////////////////////////////////////////
void UIM_ScreenFuncTrace ( ERROR_T errMsg_, int line_ )
{
    if( errMsg_ != ERR_NONE )
    {
        SAFE_ReportError( errMsg_, __FILENAME__, line_ ); //lint !e613 Filename is not NULL ptr
    }
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetDefaultBrightnessLevel ( uint8_t level_ )
{
    if( level_ > MAX_PERCENT )
    {
        UIM_ScreenFuncTrace( ERR_PARAMETER, __LINE__ );
    }

    s_BrightnessValue = level_;
    s_BrightnessValue = s_BrightnessValue / 10;
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetDefaultVolumeLevel ( uint8_t level_ )
{
    if( level_ > MAX_PERCENT )
    {
        UIM_ScreenFuncTrace( ERR_PARAMETER, __LINE__ );
    }

    s_VolumeValue = level_;
    s_VolumeValue = s_VolumeValue / 10;
}

////////////////////////////////////////////////////////////////////////////////
void UIM_ResetProgressCounter ( void )
{
    s_PowerProgressCounter = 0;
}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetSelectedValueInEditPath ( uint8_t programVal_ )
{
    PROGRAM_INFO_T currProg;
    //Get the current program Information
    UIM_GetCurrProg( &currProg );
    //Save selected program information
    s_CurrProgDosage      = currProg.ProgDosage;
    s_CurrProgHyDosage    = currProg.ProgHyDosage;
    s_CurrProgIgDosage    = currProg.ProgIgDosage;
    s_CurrHyFlowRateValue = currProg.ProgStepFlowRate[HY_STEP_INDEX];

}

////////////////////////////////////////////////////////////////////////////////
// This function takes the string and gives the coordinates relative to the LCD (in pixels) versus the coordinates relative to the
// parent TEXT widget.
// This function assumes that the text is horizontally and vertically centered in the text widget.
// Assumes that rect_ is init to 0
static void UIM_StringCoordsLCD ( WM_HWIN hWin_, GUI_RECT *rect_, char const *textString_, int alignment_, FONT_ID_T font_ )
{
    I16 winXorg = 0;
    I16 winYorg = 0;
    I16 winYsize = 0;
    I16 winXsize = 0;

    int size = 0;

    if( rect_ == NULL )
    {
        UIM_ScreenFuncTrace( ERR_PARAMETER, __LINE__ );
        return;
    }

    GUI_SetFont( FONT_ARR[font_] );

    size = GUI_GetStringDistX( textString_ );

    winXorg = ( I16 )WM_GetWindowOrgX( hWin_ );
    winYorg = ( I16 )WM_GetWindowOrgY( hWin_ );
    winYsize  = ( I16 )WM_GetWindowSizeY( hWin_ );
    winXsize  = ( I16 )WM_GetWindowSizeX( hWin_ );

    switch( alignment_ )
    {
        case GUI_TA_HCENTER:

            rect_->x0 = ( I16 )( ( winXsize / 2 ) - ( rect_->x1 / 2 ) ) + winXorg;
            rect_->x1 = ( I16 )( rect_->x0 + size );

            break;

        case GUI_TA_LEFT:

            rect_->x0 = winXorg;
            rect_->x1 = ( I16 )( rect_->x0 + size );

            break;

        default:
            break;

    }

    rect_->y0 = ( ( winYsize / 2 ) - ( rect_->y1 / 2 ) ) + winYorg;
    rect_->y1 = rect_->y0 + rect_->y1;

}

////////////////////////////////////////////////////////////////////////////////
// This function assumes that the text is horizontally and vertically centered in the text widget.
// Also assumes that only the numerical portion of the string will be underlined.
// Also assumes that the numerical portion is at the beginning of the string.
void UIM_SetUnderLineNum ( WM_HWIN hWin_, const char *string_, GUI_RECT *rect_ )
{
    int rateStrLen = 0;
    char rateBuff[MAX_DIGITS_WITH_SPACE];
    memset( rateBuff, 0, MAX_DIGITS_WITH_SPACE );

    if( ( string_ == NULL ) || ( rect_ == NULL ) )
    {
        UIM_ScreenFuncTrace( ERR_PARAMETER, __LINE__ );
        return;
    }

    for( int i = 1; i < MAX_DIGITS_WITH_SPACE; i++ )
    {
        // added ( string_[i] != '.')for the floating point value
        if( ( ( string_[i] > '9' ) || ( string_[i] < '0' ) || ( string_[i] == ' ' ) ) && ( string_[i] != '.') )
        {
            rateStrLen = i;
            break;
        }
    }

    memcpy( rateBuff, string_, ( uint32_t) rateStrLen );

    //This is used to clear the last drawn underline. The first time through, the rect will be at 0 coordinates and will seemingly clear nothing.
    GUI_ClearRect( rect_->x0, rect_->y1, rect_->x1, rect_->y1 + UNDERLINE_SPACE_SIZE );

    //This function only correctly gets x1 and y1 relative to the text widget it is contained in.
    GUI_GetTextExtend( rect_, string_, ( int )strlen( string_ ) );

    UIM_StringCoordsLCD( hWin_, rect_, rateBuff, GUI_TA_CENTER, FL ); //FL used for font type b/c we assume that numbers are always large.


}

////////////////////////////////////////////////////////////////////////////////
void UIM_SetHyqviaDefaults ( void )
{
    s_CurrIgDurationValue = PM_HYQVIA_DEFAULT_TIME;
}

