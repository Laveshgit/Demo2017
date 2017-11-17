//////////////////////////////////////////////////////////////////////////////
///
///                Copyright(c) 2017 FlexMedical
///
///                   *** Confidential Company Proprietary ***
///
/// \file USBManager.c
///
/// \brief Interface for USB Manager.
//
///////////////////////////////////////////////////////////////////////////////

// Vendor's header files
#include  <usbd_core.h>
#include  <usbd_dev_cfg.h>
#include  <usbd_drv_stm32f_fs.h>
#include  <usbd_msc.h>
#include  <lib_ascii.h>
#include  <os.h>
#include  <clk.h>
#include  <usbd_cdc.h>
#include  <usbd_acm_serial.h>

// Project header files
#include "GPIO.h"
#include "SerialManager.h"
#include "USBDriver.h"

// Interface header
#include "USBManager.h"

////////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND #DEFINES
////////////////////////////////////////////////////////////////////////////////

#define USBD_MSC_VID    "Micrium"
#define USBD_MSC_PID    "MSC FS Storage"

////////////////////////////////////////////////////////////////////////////////
// ENUMS, TYPEDEFS, AND STRUCTURES
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Prototypes
////////////////////////////////////////////////////////////////////////////////

// MSC Class
static CPU_BOOLEAN USB_MSC_Init( CPU_INT08U dev_nbr_,
                                 CPU_INT08U cfg_hs_,
                                 CPU_INT08U cfg_fs_ );
// CDC Class
static CPU_BOOLEAN USB_CDC_Init( CPU_INT08U dev_nbr_,
                                 CPU_INT08U cfg_hs_,
                                 CPU_INT08U cfg_fs_ );

// USB Callbacks
static void USB_EventConnCallback( CPU_INT08U dev_nbr_ );
static void USB_EventDisconnCallback( CPU_INT08U dev_nbr_ );
static void USB_EventResetCallback( CPU_INT08U dev_nbr_ );
static void USB_EventSuspendCallback( CPU_INT08U dev_nbr_ );
static void USB_EventResumeCallback( CPU_INT08U dev_nbr_ );
static void USB_EventConfiguredCallback( CPU_INT08U dev_nbr_, CPU_INT08U  cfg_val_ );
static void USB_EventDeConfiguredCallback( CPU_INT08U dev_nbr_, CPU_INT08U  cfg_val_ );


////////////////////////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////////////////////////

CPU_INT08U g_DevNumber;
CPU_INT08U g_UsbCdcSubClassNbr;
bool g_IsUsbConnected = FALSE;

///////////////////////////////////////////////////////////////////////////////
// Static Variables
///////////////////////////////////////////////////////////////////////////////

static bool s_usbEnabled = FALSE;   // TRUE when usb is enabled

// Device Callback Structure
static USBD_BUS_FNCTS s_UsbdBusFncts =
{
    USB_EventResetCallback,         // Reset Callback
    USB_EventSuspendCallback,       // Suspend Callback
    USB_EventResumeCallback,        // Resume Callback
    USB_EventConfiguredCallback,    // ConfigSet Callback
    USB_EventDeConfiguredCallback,  // De-configured Callback
    USB_EventConnCallback,          // Connect Callback
    USB_EventDisconnCallback,       // Disconnect Callback
};

///////////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////////

ERROR_T USB_Initialize ( void )
{
    CPU_INT08U cfgHsNbr;
    CPU_INT08U cfgFsNbr;
    CPU_BOOLEAN ok;
    USBD_ERR err;

    USBD_Init( &err );
    if( err != USBD_ERR_NONE )
    {
        return ERR_USB;
    }

    g_DevNumber = USBD_DevAdd( &USBD_DevCfg_STM32F479NIH6,
                               &s_UsbdBusFncts,
                               &USBD_DrvAPI_STM32F_OTG_FS,
                               &USBD_DrvCfg_STM32F479NIH6,
                               &USBD_DrvBSP_STM32F479NIH6,
                               &err );
    if( err != USBD_ERR_NONE )
    {
        return ERR_USB;
    }

    USBD_DevSelfPwrSet( g_DevNumber, DEF_NO, &err );

    cfgHsNbr = USBD_CFG_NBR_NONE;
    cfgFsNbr = USBD_CFG_NBR_NONE;

    // Add FS configuration
    cfgFsNbr = USBD_CfgAdd( g_DevNumber,
                              USBD_DEV_ATTRIB_SELF_POWERED,
                              100u,
                              USBD_DEV_SPD_FULL,
                              "FS configuration",
                              &err );
    if( err != USBD_ERR_NONE )
    {
        return ERR_USB;
    }

    // Initialize MSC class.
    ok = USB_MSC_Init( g_DevNumber, cfgHsNbr, cfgFsNbr );
    if( ok != DEF_OK )
    {
        return ERR_USB;
    }

    // Initialize CDC class.
    ok = USB_CDC_Init( g_DevNumber, cfgHsNbr, cfgFsNbr );
    if( ok != DEF_OK )
    {
        return ERR_USB;
    }

    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////


ERROR_T USB_InterfaceEnable ( void )
{
    USBD_ERR err;
    ERROR_T returnStatus;

    returnStatus = GPIO_Write( GPIO_ID_USB_OE, GPIO_OUTPUT_STATE_LOW );

    if( returnStatus == ERR_NONE )
    {
        USBD_DevStart( g_DevNumber, &err );
        if( err != USBD_ERR_NONE )
        {
            returnStatus = ERR_USB;
        }
        else
        {
            s_usbEnabled = TRUE;
        }
    }

    return returnStatus;
}

////////////////////////////////////////////////////////////////////////////////


ERROR_T USB_InterfaceDisable ( void )
{
    USBD_ERR err;
    ERROR_T returnStatus;

    returnStatus = GPIO_Write( GPIO_ID_USB_OE, GPIO_OUTPUT_STATE_HIGH );

    if( returnStatus == ERR_NONE )
    {
        USBD_DevStop( g_DevNumber, &err );
        if( err != USBD_ERR_NONE )
        {
            returnStatus = ERR_USB;
        }
        else
        {
            s_usbEnabled = FALSE;
        }
    }

    return returnStatus;
}

////////////////////////////////////////////////////////////////////////////////


ERROR_T USB_GetCdcSubClassNumber ( CPU_INT08U *number_ )
{
    if( number_ == NULL )
    {
        return ERR_PARAMETER;
    }
    *number_ = g_UsbCdcSubClassNbr;
    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////

ERROR_T USB_GetConnectionStatus ( bool *status_ )
{
    if( status_ == NULL )
    {
        return ERR_PARAMETER;
    }
    *status_ = g_IsUsbConnected;
    return ERR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// NOTE: This callback function is called when USB plugged into Host System
static void USB_EventConnCallback ( CPU_INT08U dev_nbr_ )
{
    UNUSED( dev_nbr_ );
    (void)SM_SendF( "USB Connected\r\n" );
}

////////////////////////////////////////////////////////////////////////////////
// NOTE: This callback function is called when USB unplugged from Host System
static void USB_EventDisconnCallback ( CPU_INT08U dev_nbr_ )
{
    UNUSED( dev_nbr_ );

    g_IsUsbConnected = FALSE;
    (void)SM_SendF( "USB Disconnected\r\n" );
}

////////////////////////////////////////////////////////////////////////////////
static void USB_EventResetCallback ( CPU_INT08U dev_nbr_ )
{
    UNUSED( dev_nbr_ );
    (void)SM_SendF( "USB Event Reset\r\n" );
}

////////////////////////////////////////////////////////////////////////////////
// NOTE: This callback function is called when USB unplugged from the host system
static void USB_EventSuspendCallback ( CPU_INT08U dev_nbr_ )
{
    UNUSED( dev_nbr_ );
    (void)SM_SendF( "USB Suspend\r\n" );
}

////////////////////////////////////////////////////////////////////////////////
// NOTE: This callback function is called when USB stack is already configured but
// USB unplugged form HOST system previously and now it is plugged to HOST system
static void USB_EventResumeCallback ( CPU_INT08U dev_nbr_ )
{
    UNUSED( dev_nbr_ );
    (void)SM_SendF( "USB Resume\r\n" );
}

////////////////////////////////////////////////////////////////////////////////
// NOTE: This callback function is called when USB unplugged from Host System
static void USB_EventConfiguredCallback ( CPU_INT08U dev_nbr_, CPU_INT08U  cfg_val_ )
{
    UNUSED( dev_nbr_ );
    UNUSED( cfg_val_ );

    // Update Global Flag
    g_IsUsbConnected = TRUE;
    (void)SM_SendF( "USB Configured\r\n" );
}

////////////////////////////////////////////////////////////////////////////////
// NOTE: This callback function is called when USB is plugged with
static void USB_EventDeConfiguredCallback ( CPU_INT08U dev_nbr_, CPU_INT08U  cfg_val_ )
{
    UNUSED( dev_nbr_ );
    UNUSED( cfg_val_ );

    g_IsUsbConnected = FALSE;
    (void)SM_SendF( "USB DeConfigured\r\n");
}

////////////////////////////////////////////////////////////////////////////////


static CPU_BOOLEAN USB_MSC_Init ( CPU_INT08U dev_nbr_, CPU_INT08U cfg_hs_, CPU_INT08U cfg_fs_ )
{
    USBD_ERR err;
    CPU_INT08U mscNbr;
    CPU_BOOLEAN valid;
    char vid[25];
    char pid[25];

    USBD_MSC_Init( &err );
    if( err != USBD_ERR_NONE )
    {
        return DEF_FAIL;
    }

    mscNbr = USBD_MSC_Add( &err );

    if( cfg_hs_ != USBD_CFG_NBR_NONE )
    {
        valid = USBD_MSC_CfgAdd( mscNbr, dev_nbr_, cfg_hs_, &err );
        if( valid != DEF_YES )
        {
            return DEF_FAIL;
        }
    }

    if( cfg_fs_ != USBD_CFG_NBR_NONE )
    {
        valid = USBD_MSC_CfgAdd( mscNbr, dev_nbr_, cfg_fs_, &err );
        if( valid != DEF_YES )
        {
            return DEF_FAIL;
        }
    }
    
    strncpy(vid, USBD_MSC_VID , sizeof(vid) );
    strncpy(pid, USBD_MSC_PID , sizeof(pid) );

    // Add Logical Unit to MSC interface.
    USBD_MSC_LunAdd( NOR_DEVICE, mscNbr, (CPU_CHAR *)vid,
                     (CPU_CHAR *)pid, (CPU_INT32U)0x00, DEF_FALSE, &err );
    if( err != USBD_ERR_NONE )
    {
        return DEF_FAIL;
    }

    return DEF_OK;
}

////////////////////////////////////////////////////////////////////////////////


static CPU_BOOLEAN  USB_CDC_Init ( CPU_INT08U dev_nbr_, CPU_INT08U cfg_hs_, CPU_INT08U cfg_fs_ )
{
    USBD_ERR err;
    USBD_ERR errHs = USBD_ERR_NONE;
    USBD_ERR errFs = USBD_ERR_NONE;

    USBD_CDC_Init( &err );
    if( err != USBD_ERR_NONE )
    {
        return DEF_FAIL;
    }

    USBD_ACM_SerialInit( &err );
    if( err != USBD_ERR_NONE )
    {
        return DEF_FAIL;
    }

    g_UsbCdcSubClassNbr = USBD_ACM_SerialAdd( 64u,
                                              ( USBD_ACM_SERIAL_CALL_MGMT_DATA_CCI_DCI | USBD_ACM_SERIAL_CALL_MGMT_DEV ),
                                              &err );
    if( err != USBD_ERR_NONE )
    {
        return DEF_FAIL;
    }

    // Register line coding and ctrl line change callbacks.
    USBD_ACM_SerialLineCodingReg( g_UsbCdcSubClassNbr, NULL, (void *)0, &err );

    USBD_ACM_SerialLineCtrlReg( g_UsbCdcSubClassNbr, NULL, (void *)0, &err );

    if( cfg_hs_ != USBD_CFG_NBR_NONE )
    {
        USBD_ACM_SerialCfgAdd( g_UsbCdcSubClassNbr, dev_nbr_, cfg_hs_, &errHs );
        if( errHs != USBD_ERR_NONE )
        {
            return DEF_FAIL;
        }
    }

    if( cfg_fs_ != USBD_CFG_NBR_NONE )
    {
        USBD_ACM_SerialCfgAdd( g_UsbCdcSubClassNbr, dev_nbr_, cfg_fs_, &errFs );
        if( errFs != USBD_ERR_NONE )
        {
            return DEF_FAIL;
        }
    }

    return DEF_OK;
}

////////////////////////////////////////////////////////////////////////////////


ERROR_T USB_State( bool *pEnabled_ )
{
    ERROR_T returnStatus = ERR_NONE;

    if( pEnabled_ == NULL )
    {
        returnStatus = ERR_PARAMETER;
    }
    else
    {
        *pEnabled_ = s_usbEnabled;
    }

    return returnStatus;
}
