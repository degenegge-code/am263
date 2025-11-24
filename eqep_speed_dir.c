#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/HwiP.h>
#include <drivers/eqep.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

/*
 *  Speed and Direction Measurement Using eQEP
 * 
 *
 * The configuration for this example is as follows:
 * irc information taken from ti-gel-2xxx-rotorpos-encoder-en-pre.pdf
 *
 *          |¯¯¯¯¯¯¯¯¯|          |¯¯¯¯¯¯¯¯¯|
 *     A    |         |__________|         |>
 *          |
 *          |
 *          |    |¯¯¯¯¯¯¯¯¯|          |¯¯¯¯¯>
 *     B    |____|         |__________|
 *
 * - UNIT_PERIOD is specified as 2e5 ticks = 1ms
 * - Simulated quadrature signal frequency is 800000 protože 4 pulsy jsou jeden puls (quadrature mode)
 * - Encoder holes assumed as 4000 třeba nevim,- v pdfku je napsáno "to be defined"

 * freq : Simulated quadrature signal frequency measured by counting the external input pulses for UNIT_PERIOD (set v syscfgu) i zde
 * dir : Indicates clockwise (1) or anticlockwise (-1)
 *
 *	EQEP: EQEPxA Pin(EQEP0_A) B14 a EQEPxB Pin(EQEP0_B) A14 - gpio 130 a 131
 * Internal Connections \n
 * - ePWM2A -> C2 -> GPIO47 -> INPUTXBAR1 -> PWMXBAR1 -> eQEP0A
 * - ePWM2B -> C1 -> GPIO48 -> INPUTXBAR2 -> PWMXBAR2 -> eQEP0B
 * 
 * 1X Resolution: Count rising edge only
 * emulation: Counter unaffected by suspend
 * QEPI/Index Source: Signal comes from Device Pin
 * maximum of the counter: 4294967295
 * Position Counter Mode: Reset position on a unit time event
 * enable unit timer and Unit Timer Period : 2000000
 * INT XBAR		CONFIG_INT_XBAR0	EQEP0_INT	INT_XBAR_0 	woe
 * INPUT XBAR naroutovat na EPWMXBAR (v epwmxbar, input xbary se vytvoří samy)
 * quadrature mode je tam implicitně
 * 
 * snad jsem nastavil směr správně, ještě zkontrolovat
 * 
 */


// Defines:
#define DEVICE_SYSCLK_FREQ  (uint32_t)(2e8) // Sysclk frequence 2MHz
#define APP_INT_IS_PULSE  (1U) // Macro for interrupt pulse
#define UNIT_PERIOD  (uint32_t)(DEVICE_SYSCLK_FREQ / 1e3) // period timer value, at which timer-out interrupt is set, takže na 1ms: 2e8 / 1e3 = 2e5
#define QUAD_TIC 4U //Quadrature-clock mode

// Global variables and objects :
uint32_t gEqepBaseAddr;
static HwiP_Object gEqepHwiObject;
uint32_t gCount = 0;                    // Counter to check measurement gets saturated
uint32_t gOldcount = 0;                 // Stores the previous position counter value 
int32_t gFreq = 0;                     // Measured signal frequency (respecting quadrature mode) of motor using eQEP 
int32_t gDir = 0;                       // Direction of rotation of motor 
volatile uint32_t gNewcount = 0 ;       // stores the actual position counter value

// Function Prototypes:
static void App_eqepISR(void *handle);
void eqep_measure10(void);


//functions: 

//eqep set a main
void eqep_speed_dir_init(void *args)
{
    int32_t status;
    HwiP_Params hwiPrms;

    DebugP_log("EQEP Speed Direction Test Started ...\r\n");
	
    gEqepBaseAddr = CONFIG_EQEP0_BASE_ADDR;

    HwiP_Params_init(&hwiPrms);     // Register & enable interrupt

    hwiPrms.intNum      = CSLR_R5FSS0_CORE0_CONTROLSS_INTRXBAR0_OUT_2;      //  Syscfg: INT XBAR -> CONFIG_INT_XBAR1, EQEP0_INT, INT_XBAR_2
    hwiPrms.callback    = &App_eqepISR;                                 //předat do handle*
    hwiPrms.isPulse     = APP_INT_IS_PULSE;                                 
    status              = HwiP_construct(&gEqepHwiObject, &hwiPrms);
    DebugP_assert(status == SystemP_SUCCESS);

    EQEP_loadUnitTimer(gEqepBaseAddr, UNIT_PERIOD); //radši to sem hodim i mimo syscfg
    EQEP_clearInterruptStatus(gEqepBaseAddr,EQEP_INT_UNIT_TIME_OUT|EQEP_INT_GLOBAL);     // Clear eqep interrupt 
}

void eqep_measure10(void)
{
    //čekej na deset měření, tzn 10 period = 10* UNIT_PERIOD = 10ms
    while( *(volatile uint32_t *) ((uintptr_t) &gCount) < 10) //vau hustý
    {
        //Wait for Speed results
    }
}

void eqep_close(void)
{
	DebugP_log("IRC Rotation direction = ");
	if(gDir==1)
	{
		DebugP_log("Clockwise, forward \r\n");
	}
	else if(gDir==-1)
	{
		DebugP_log("Counter Clockwise, reverse \r\n");
	}
    else 
    {
        DebugP_log("AAAAAAAAAAAAAAAA HELP \r\n");   //it returns 0 as counterclockwise?

    }

    DebugP_log("EQEP tests have passed!!\r\n");
}

int32_t eqep_freq(void)
{
    if (gDir > 0)
    {
    return gFreq;
    }
    else 
    return (-1)*gFreq;
}
static void App_eqepISR(void *handle)
{
    gCount++;     // Increment count value 
    gNewcount = EQEP_getPositionLatch(gEqepBaseAddr);     // New position counter value 

    gDir = EQEP_getDirection(gEqepBaseAddr);    // Gets direction of rotation of motor 

   if (gDir > 0 ){
        // Forward direction: use gNewcount as-is
        }
   else {
            // Reverse direction: convert to magnitude (dvojkovy doplněk)
            gNewcount = (0xFFFFFFFF - gNewcount) + 1;
        }

   gOldcount = gNewcount;    // Stores the current position count value to oldcount variable 

   // gFreq Calculation:
   // gNewcount = position pulses accumulated in UNIT_PERIOD (1 ms by default)
   // UNIT_PERIOD = 2e5 ticks at 200 MHz sysclock = 1 ms
   // gFreq [pulses/sec] = gNewcount [pulses/ms] * (1e3 ms/sec)
   // Simplified: gFreq = gNewcount * 1000
   gFreq = (int32_t)(gNewcount * 1000 / QUAD_TIC );
   
   EQEP_clearInterruptStatus(gEqepBaseAddr,EQEP_INT_UNIT_TIME_OUT|EQEP_INT_GLOBAL);    // Clear interrupt flag
}
