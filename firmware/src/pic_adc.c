/*******************************************************************************
  MPLAB Harmony Application Source File
  
  Company:
    Microchip Technology Inc.
  
  File Name:
    pic_adc.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It 
    implements the logic of the application's state machine and it may call 
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
Copyright (c) 2013-2014 released Microchip Technology Inc.  All rights reserved.

Microchip licenses to you the right to use, modify, copy and distribute
Software only when embedded on a Microchip microcontroller or digital signal
controller that is integrated into your product or third party product
(pursuant to the sublicense terms in the accompanying license agreement).

You should refer to the license agreement accompanying this Software for
additional information regarding your rights and obligations.

SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
IN NO EVENT SHALL MICROCHIP OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER
CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR
OTHER LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR
CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT OF
SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
(INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.
 *******************************************************************************/
// DOM-IGNORE-END


// *****************************************************************************
// *****************************************************************************
// Section: Included Files 
// *****************************************************************************
// *****************************************************************************

#include "pic_adc.h"
/*** ADD ***/
#include "stdio.h"
/*** ADD ***/
// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************
/*** ADD ***/
#define ADC_USB_SEND_FORMAT "ADC DATA = 0x%03X\r\n"
/*** ADD ***/

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
*/

PIC_ADC_DATA pic_adcData;
/* Static buffers, suitable for DMA transfer */
#define PIC_ADC_MAKE_BUFFER_DMA_READY  __attribute__((coherent)) __attribute__((aligned(16)))

static uint8_t PIC_ADC_MAKE_BUFFER_DMA_READY writeBuffer[PIC_ADC_USB_CDC_COM_PORT_SINGLE_WRITE_BUFFER_SIZE];
static uint8_t writeString[] = "Hello World\r\n";

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************


/*******************************************************
 * USB CDC Device Events - Application Event Handler
 *******************************************************/

USB_DEVICE_CDC_EVENT_RESPONSE PIC_ADC_USBDeviceCDCEventHandler
(
    USB_DEVICE_CDC_INDEX index ,
    USB_DEVICE_CDC_EVENT event ,
    void * pData,
    uintptr_t userData
)
{
    PIC_ADC_DATA * appDataObject;
    appDataObject = (PIC_ADC_DATA *)userData;
    USB_CDC_CONTROL_LINE_STATE * controlLineStateData;

    switch ( event )
    {
        case USB_DEVICE_CDC_EVENT_GET_LINE_CODING:

            /* This means the host wants to know the current line
             * coding. This is a control transfer request. Use the
             * USB_DEVICE_ControlSend() function to send the data to
             * host.  */

            USB_DEVICE_ControlSend(appDataObject->deviceHandle,
                    &appDataObject->getLineCodingData, sizeof(USB_CDC_LINE_CODING));

            break;

        case USB_DEVICE_CDC_EVENT_SET_LINE_CODING:

            /* This means the host wants to set the line coding.
             * This is a control transfer request. Use the
             * USB_DEVICE_ControlReceive() function to receive the
             * data from the host */

            USB_DEVICE_ControlReceive(appDataObject->deviceHandle,
                    &appDataObject->setLineCodingData, sizeof(USB_CDC_LINE_CODING));

            break;

        case USB_DEVICE_CDC_EVENT_SET_CONTROL_LINE_STATE:

            /* This means the host is setting the control line state.
             * Read the control line state. We will accept this request
             * for now. */

            controlLineStateData = (USB_CDC_CONTROL_LINE_STATE *)pData;
            appDataObject->controlLineStateData.dtr = controlLineStateData->dtr;
            appDataObject->controlLineStateData.carrier = controlLineStateData->carrier;

            USB_DEVICE_ControlStatus(appDataObject->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);

            break;

        case USB_DEVICE_CDC_EVENT_SEND_BREAK:

            /* This means that the host is requesting that a break of the
             * specified duration be sent.  */
            break;

        case USB_DEVICE_CDC_EVENT_READ_COMPLETE:
            /* This means that the host has sent some data*/
            break;

        case USB_DEVICE_CDC_EVENT_CONTROL_TRANSFER_DATA_RECEIVED:

            /* The data stage of the last control transfer is
             * complete. For now we accept all the data */

            USB_DEVICE_ControlStatus(appDataObject->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);
            break;

        case USB_DEVICE_CDC_EVENT_CONTROL_TRANSFER_DATA_SENT:

            /* This means the GET LINE CODING function data is valid. We dont
             * do much with this data in this demo. */
            break;

        case USB_DEVICE_CDC_EVENT_WRITE_COMPLETE:

            /* This means that the host has sent some data*/
            appDataObject->writeTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
            /*** ADD ***/
            memset( writeBuffer, 0x00, sizeof(writeBuffer));
            /*** ADD ***/
            break;

        default:
            break;
    }

    return USB_DEVICE_CDC_EVENT_RESPONSE_NONE;
}

/***********************************************
 * Application USB Device Layer Event Handler.
 ***********************************************/
void PIC_ADC_USBDeviceEventHandler ( USB_DEVICE_EVENT event, void * eventData, uintptr_t context )
{
    USB_DEVICE_EVENT_DATA_CONFIGURED *configuredEventData;

    switch ( event )
    {
        case USB_DEVICE_EVENT_SOF:
            break;

        case USB_DEVICE_EVENT_RESET:

            pic_adcData.isConfigured = false;

            break;

        case USB_DEVICE_EVENT_CONFIGURED:

            /* Check the configuration. We only support configuration 1 */
            configuredEventData = (USB_DEVICE_EVENT_DATA_CONFIGURED*)eventData;
            if ( configuredEventData->configurationValue == 1)
            {
                /* Register the CDC Device application event handler here.
                 * Note how the pic_adcData object pointer is passed as the
                 * user data */

                USB_DEVICE_CDC_EventHandlerSet(USB_DEVICE_CDC_INDEX_0, PIC_ADC_USBDeviceCDCEventHandler, (uintptr_t)&pic_adcData);

                /* Mark that the device is now configured */
                pic_adcData.isConfigured = true;

            }
            break;

        case USB_DEVICE_EVENT_POWER_DETECTED:

            /* VBUS was detected. We can attach the device */
            USB_DEVICE_Attach(pic_adcData.deviceHandle);
            break;

        case USB_DEVICE_EVENT_POWER_REMOVED:

            /* VBUS is not available any more. Detach the device. */
            USB_DEVICE_Detach(pic_adcData.deviceHandle);
            break;

        case USB_DEVICE_EVENT_SUSPENDED:
            break;

        case USB_DEVICE_EVENT_RESUMED:
        case USB_DEVICE_EVENT_ERROR:
        default:
            break;
    }
}

/* TODO:  Add any necessary callback functions.
*/
static void callbackTimer( uintptr_t context, uint32_t currTick ){
    pic_adcData.adcState = PIC_ADC_STATE_ADC_START;
    return;
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

/******************************************************************************
  Function:
    static void USB_TX_Task (void)
    
   Remarks:
    Feeds the USB write function. 
*/
static void USB_TX_Task (void)
{
    /*** ADD ***/
    uint8_t size;
    /*** ADD ***/
    
    if(!pic_adcData.isConfigured)
    {
        pic_adcData.writeTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
    }
    else
    {
        /* Schedule a write if data is pending 
         */
        /*** ADD ***/
        //if ((pic_adcData.writeLen > 0)/* && (pic_adcData.writeTransferHandle == USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID)*/)
        //{
        //    USB_DEVICE_CDC_Write(USB_DEVICE_CDC_INDEX_0,
        //                         &pic_adcData.writeTransferHandle,
        //                         writeBuffer, 
        //                         pic_adcData.writeLen,
        //                         USB_DEVICE_CDC_TRANSFER_FLAGS_DATA_COMPLETE);
        //}
        size = strlen(writeBuffer);
        if ((size > 0) && (pic_adcData.writeTransferHandle == USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID)){
            USB_DEVICE_CDC_Write(USB_DEVICE_CDC_INDEX_0,
                                 &pic_adcData.writeTransferHandle,
                                 writeBuffer, 
                                 size,
                                 USB_DEVICE_CDC_TRANSFER_FLAGS_DATA_COMPLETE);
        }
        /*** ADD ***/
    }
}


/* TODO:  Add any necessary local functions.
*/
/*** ADD ***/
// ADC 取得タスク
static void ADC_Task (void) {
    
    switch(pic_adcData.adcState){
        case PIC_ADC_STATE_ADC_INIT:
        {
            pic_adcData.adcState = PIC_ADC_STATE_ADC_TIMERSTART;
            break;
        }
        case PIC_ADC_STATE_ADC_TIMERSTART:
        {
            pic_adcData.timerHandle = SYS_TMR_ObjectCreate(1000, 0, callbackTimer, SYS_TMR_FLAG_PERIODIC);
            pic_adcData.adcState = PIC_ADC_STATE_ADC_WAIT;
            break;
        }
        case PIC_ADC_STATE_ADC_START:
        {
            DRV_ADC_Start();
            pic_adcData.adcState = PIC_ADC_STATE_ADC_GET;
            break;
        }
        case PIC_ADC_STATE_ADC_GET:
        {
            if(DRV_ADC_SamplesAvailable(ADCHS_AN0)){
                pic_adcData.adcData = DRV_ADC_SamplesRead(ADCHS_AN0);
                pic_adcData.adcState = PIC_ADC_STATE_ADC_WRITEBUFFER;
            }
            break;
        }
        case PIC_ADC_STATE_ADC_WRITEBUFFER:
        {
            memset( writeBuffer, 0x00, sizeof(writeBuffer) );
            sprintf( writeBuffer, ADC_USB_SEND_FORMAT, pic_adcData.adcData );
                pic_adcData.adcState = PIC_ADC_STATE_ADC_WAIT;
            break;
        }
        case PIC_ADC_STATE_ADC_ERROR:
        {
            break;
        }
        default:
            break;
    }
    return;
}
/*** ADD ***/

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void PIC_ADC_Initialize ( void )

  Remarks:
    See prototype in pic_adc.h.
 */

void PIC_ADC_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    pic_adcData.state = PIC_ADC_STATE_INIT;


    /* Device Layer Handle  */
    pic_adcData.deviceHandle = USB_DEVICE_HANDLE_INVALID ;

    /* Device configured status */
    pic_adcData.isConfigured = false;

    /* Initial get line coding state */
    pic_adcData.getLineCodingData.dwDTERate   = 9600;
    pic_adcData.getLineCodingData.bParityType =  0;
    pic_adcData.getLineCodingData.bParityType = 0;
    pic_adcData.getLineCodingData.bDataBits   = 8;


    /* Write Transfer Handle */
    pic_adcData.writeTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
    
    /*Initialize the write data */
    pic_adcData.writeLen = sizeof(writeString);
	memcpy(writeBuffer, writeString, pic_adcData.writeLen);
    
    /* TODO: Initialize your application's state machine and other
     * parameters.
     */
    /*** ADD ***/
    pic_adcData.adcState = PIC_ADC_STATE_ADC_INIT;
    pic_adcData.adcData = 0;
    pic_adcData.timerHandle = SYS_TMR_HANDLE_INVALID;
    /*** ADD ***/
}


/******************************************************************************
  Function:
    void PIC_ADC_Tasks ( void )

  Remarks:
    See prototype in pic_adc.h.
 */

void PIC_ADC_Tasks ( void )
{

    /* Check the application's current state. */
    switch ( pic_adcData.state )
    {
        /* Application's initial state. */
        case PIC_ADC_STATE_INIT:
        {
            bool appInitialized = true;
       

            /* Open the device layer */
            if (pic_adcData.deviceHandle == USB_DEVICE_HANDLE_INVALID)
            {
                pic_adcData.deviceHandle = USB_DEVICE_Open( USB_DEVICE_INDEX_0,
                                               DRV_IO_INTENT_READWRITE );
                appInitialized &= ( USB_DEVICE_HANDLE_INVALID != pic_adcData.deviceHandle );
            }
        
            if (appInitialized)
            {

                /* Register a callback with device layer to get event notification (for end point 0) */
                USB_DEVICE_EventHandlerSet(pic_adcData.deviceHandle,
                                           PIC_ADC_USBDeviceEventHandler, 0);
                
                /*** ADD ***/
                // ADC0 オープン
                DRV_ADC0_Open();
                /*** ADD ***/
            
                pic_adcData.state = PIC_ADC_STATE_SERVICE_TASKS;
            }
            break;
        }

        case PIC_ADC_STATE_SERVICE_TASKS:
        {
            USB_TX_Task();
            
            /*** Add ***/
            ADC_Task();
            /*** Add ***/
            
            break;
        }

        /* TODO: implement your application state machine.*/
        

        /* The default state should never be executed. */
        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}

 

/*******************************************************************************
 End of File
 */
