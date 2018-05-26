#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/exec.h>
#include <dos/dos.h>
#include <devices/timer.h>

#include "i2c_library.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#include <clib/alib_protos.h>
#include <clib/debug_protos.h>

#include "global.h"
#include "protos.h"

/* BumpRev version information */
#include "ICYcle_rev.h"

/* Library bases */
struct Library          *I2C_Base      = NULL;
struct Device           *TimerBase     = NULL;

/* Timer port & stuff */
struct MsgPort *TimePort = NULL;
struct timerequest *TimeRequest = NULL;


/* Version information */
char VersionString[] ="$VER: " VSTRING;


/* variables & pointers for the command line */
#define CLI_TEMPLATE "RAW/S,R=REPEAT/S"

struct RDArgs *rda;
LONG *argvalues[2] = { NULL };
int retval = RETURN_OK;

/* and here we go */
int main(int argc, char **argv) {
    
    Startup(argc);
    
	if (argvalues[1])
	{
	    ULONG timer_mask;
        ULONG signal_mask;
        ULONG signals;
        ULONG td_Interval = 100;
		ULONG terminate_loop = FALSE;
		ULONG test = 0;
        
		DBG_PRINT("ICYcle: REPEAT mode\n");

		if (!(SetupTimerRequest())) {
			retval = RETURN_ERROR;
			Terminate();
		}

		timer_mask = 1L << TimePort->mp_SigBit;

        /* we want to get a signal at ctrl-C or every second */
		signal_mask = SIGBREAKF_CTRL_C | timer_mask;
		signals = 0L;

		TimeRequest->tr_node.io_Command	= TR_ADDREQUEST;
		TimeRequest->tr_time.tv_secs	= 0;
        /* user input is measured in 1/1000 s */
		TimeRequest->tr_time.tv_micro	= td_Interval;

		SendIO((struct IORequest *)TimeRequest);
   

        /*** Main loop starts here ***/
   	    while (!(terminate_loop)) 
		{
			if (signals == 0) {
                /* wait for our signals */
                signals = Wait(signal_mask);
            }
            else {
                signals |= SetSignal(0,signal_mask) & signal_mask;
            }
        
            /* we got a signal from timer.device */                
            if (signals & timer_mask) {
                if (GetMsg(TimePort) != NULL) {
                    WaitIO((struct IORequest *)TimeRequest);
                        
                    /* call our little main routine */
					PrintSensorData();
					printf("\r");

                    /* and renew the timer request */
                    TimeRequest->tr_node.io_Command	= TR_ADDREQUEST;
                    TimeRequest->tr_time.tv_secs	= 0;
                    TimeRequest->tr_time.tv_micro	= td_Interval * 1000;
                        
                    SendIO((struct IORequest *)TimeRequest);
                }
                signals &= ~timer_mask;
            }
            /* we are a nice program and exit at ctrl-c */
            if (signals & SIGBREAKF_CTRL_C) 
			{
            	terminate_loop = TRUE;
                signals &= ~SIGBREAKF_CTRL_C;

	            /* User pressed CTRL-C, raise the request_exit flag */
                printf("\r\r\033[2KExiting...\n");
        	}
        }

		/* abort and wait for pending device requests */
        if (CheckIO((struct IORequest *)TimeRequest) == NULL) {
            AbortIO((struct IORequest *)TimeRequest);
        }
        WaitIO((struct IORequest *)TimeRequest);

    }
	else {
		PrintSensorData();
		printf("\n\n");
	}
    Terminate();
}

VOID Startup(int argc) {
    if (! (I2C_Base = OpenLibrary("i2c.library", 40)) ) {
        
        Printf("Failed to open i2c.library V40 - no I2C installed?\n");
        
        retval = RETURN_ERROR;
        Terminate();
    }
	DBG_PRINT("ICYcle: Found I2C device...\n");
    
    /* try to parse the command line */
    if ((rda = ReadArgs(CLI_TEMPLATE, (LONG *) argvalues, NULL)) == NULL) {
    
        PrintFault(IoErr(), NULL);
        retval = RETURN_FAIL;
        Terminate();
    }
    
    Printf("ICYcle %ld.%ld\n\n", VERSION, REVISION);
}


VOID Terminate(VOID) {
    
    /* Clean up starts here */

    if (TimeRequest != NULL) {
        DBG_PRINT("ICYcle: Freeing timer request...\n");
        if (TimeRequest->tr_node.io_Device != NULL) {
            CloseDevice((struct IORequest *)TimeRequest);
        }
		DeleteIORequest((struct IORequest *)TimeRequest);
		TimeRequest = NULL;
    }
    if (TimePort != NULL) {
        DBG_PRINT("ICYcle: Removing message port...\n");
        DeleteMsgPort(TimePort);
		TimePort = NULL;
    }

    if (rda) {
        DBG_PRINT("ICYcle: Freeing command line arguments...\n");
        FreeArgs(rda);
    }

    if (I2C_Base) {
        CloseLibrary((struct Library *) I2C_Base);
    }
    exit(retval);
}

BOOL SetupTimerRequest(VOID) {
    
    TimePort = CreateMsgPort();
    
    if(TimePort == NULL) {
        Printf("Could not create timer message port.\n");
        return FALSE;
    } else {
        TimeRequest = (struct timerequest *)CreateIORequest(TimePort,sizeof(*TimeRequest));
        
	if(TimeRequest == NULL) {
            Printf("Could not create timer I/O request.\n");
            return FALSE;
	} else {
            if(OpenDevice(TIMERNAME,UNIT_VBLANK,(struct IORequest *)TimeRequest,0) != 0) {
                Printf("Could not open 'timer.device'.\n");
                return FALSE;
            } else {
                TimerBase = TimeRequest->tr_node.io_Device;
            }
        }
    }
    return TRUE;
}

VOID PrintSensorData(VOID) {
	ULONG i2c_error;
    char temp[30] = {"\0"};
    UBYTE LTC_Mode[] = { 0x01, 0x58 };
    UBYTE LTC_Trigger[] = { 0x02, 0x01 };
    UBYTE LTC_Data[17] = { "\0" };
    int i;

    i2c_error = SendI2C(0x98, 2, LTC_Mode);
    i2c_error = ReceiveI2C(0x98, 16, LTC_Data);
	i2c_error = SendI2C(0x98, 2, LTC_Trigger); 
    
	if (argvalues[0] && (!(argvalues[1]))) 
	{
		Printf("Raw I2C data: ");    
    
    	for (i = 0; i < 16; i++) {
    		Printf("%lx ", (UBYTE) LTC_Data[i]);
    	}
    	Printf("\n");
    }
    
    printf("Temp1: %.2f°C  Temp2: %.2f°C.  V1: %.2fV  V2: %.2fV  VCC: %.2fV",
    	(float) ((float)(((LTC_Data[8] & 0x7F) << 8) + (float)LTC_Data[9]) / 16),
       	(float) ((float)(((LTC_Data[2] & 0x7F) << 8) + LTC_Data[3]) / 16),
    	(float) ((((LTC_Data[4] & 0x7F) << 8) + LTC_Data[5]) * 0.61 / 1000),
    	(float) ((((LTC_Data[6] & 0x7F) << 8) + LTC_Data[7]) * 1.22 / 1000),
    	(float) (2.5 + (305.18 * (float) ((((LTC_Data[12] & 0x7F) << 8) + LTC_Data[13])) /1000000))
    	);             
	return;
}