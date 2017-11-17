///////////////////////////////////////////////////////////////////////////////
//
//                Copyright(c) 2016 FlexMedical
//
//                   *** Confidential Company Proprietary
//
//
// DESCRIPTION:
//  CLI.h provides definitions, enumerations, static variables, and non-static
//  function prototypes in support of CLI.c.
//
//
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _CLI_H_
#define _CLI_H_

///////////////////////////////////////////////////////////////////////////////
// Includes
///////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include "Errors.h"
#include "SerialManager.h"
#include "TaskFramework.h"

///////////////////////////////////////////////////////////////////////////////
// Defines
///////////////////////////////////////////////////////////////////////////////

#define CLI_TASK_STK_SIZE           ( 2560u )
#define MAX_PADDING_CHARS           ( 50 )
#define MAX_SERIAL_MSG              ( 120 )
#define COMMAND_LINE_LENGTH_MAX     ( 256 )
#define COMMAND_LENGTH_MAX          ( 20 )
#define COMMAND_PARMS_MAX           ( 64 )
#define COMMAND_PROMPT              "HOPP: "
#define CONTINUOUS_DISP_TIMEOUT     ( 500 )   // milliseconds
#define CLI_Q_SIZE                  ( 20 )

///////////////////////////////////////////////////////////////////////////////
// Structures, Enumerations, Typedefs
///////////////////////////////////////////////////////////////////////////////

typedef  struct
{
    const char *CMD;                                            /// Command string
    const char *SYNTAX;                                         /// Command parameter
    void (*Handler)( SERIAL_CMD_SOURCE_T source_ );             /// Command handler
    const char *HELPMSG;                                        /// Help message for command
} CLI_COMMANDS_T;

///////////////////////////////////////////////////////////////////////////////
// Local/Extern Variables
///////////////////////////////////////////////////////////////////////////////
extern char g_ParsedMsg[COMMAND_PARMS_MAX][COMMAND_LENGTH_MAX];
extern uint16_t g_LongestSyntaxLength;
extern uint16_t g_LongestCmdLength;
extern uint8_t g_ParmCount;
extern char g_PadCmd[MAX_PADDING_CHARS];
extern char g_PadSyntax[MAX_PADDING_CHARS];
extern const CLI_COMMANDS_T COMMANDS[];



///////////////////////////////////////////////////////////////////////////////
// Function Prototypes
///////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// \brief      Called by the RTOS CLI task which will monitor for messages from
///             other tasks and begin processing messages as they come in.
///
/// \param[]    void argument from RTOS task creation.
void CLI_Task( void *arg_ );

////////////////////////////////////////////////////////////////////////////////
/// \brief      Sets the starting values for all variables needed in CLI.
///
/// \return     Return_Error
ERROR_T CLI_Initialize( void );

////////////////////////////////////////////////////////////////////////////////
/// \brief      Called by Serial Manager to notify the CLI that a command has
///             been received and should be processed.
//
/// \param[in]  source_ Source of the command
//
/// \return     ERR_NONE
///             ERR_OS
ERROR_T CLI_CommandNotify( SERIAL_CMD_SOURCE_T source_ );

///////////////////////////////////////////////////////////////////////////////
/// \brief      Provides access to the task descriptor
///
/// \param[in]  **pDesc pointer to a pointer for assignment
///
/// \param[out] **pDesc now points to the task descriptor
///
/// \return     ERR_NONE
ERROR_T CLI_GetTaskControlBlock( TF_DESC_T * *pDesc_ );

////////////////////////////////////////////////////////////////////////////////
/// \brief Retrieves the source: USB or UART. Should always be USB once product in production
SERIAL_CMD_SOURCE_T CLI_GetSource( void );

#endif
