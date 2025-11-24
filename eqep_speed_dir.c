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
 * This example can be used to sense the speed and direction of motor
 * using eQEP in quadrature encoder mode.
 * EQEP unit timeout is set which will generate an interrupt every
 *  \b UNIT_PERIOD microseconds and speed calculation occurs continuously
 *  based on the direction of motor
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
 * - UNIT_PERIOD is specified as 2M ticks = 1ms
 * - Simulated quadrature signal frequency is 200000 protože 4 tiky měřim jako jedno T
 * - Encoder holes assumed as 4000 třeba nevim,- v pdfku je napsáno "to be defined"
 * - Thus Simulated motor speed is the maximum speed
 *       firc *60 /4000 = 3000 rpm
 *
 * freq : Simulated quadrature signal frequency measured by counting the external input pulses for UNIT_PERIOD (set v syscfgu)
 * speed : Measure motor speed in rpm
 * dir : Indicates clockwise (1) or anticlockwise (-1)
 * 
 * 
 */ 


/*
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
volatile int32_t gSpeed = 0;                    // Measured speed of motor in rpm 
int32_t gDir = 0;                       // Direction of rotation of motor 
volatile uint32_t gNewcount = 0 ;       // stores the actual position counter value

// Function Prototypes:
static void App_eqepIntrISR(void *handle);


//functions: 

//eqep set a main
void eqep_speed_dir_main(void *args)
{
    int32_t status;
    HwiP_Params hwiPrms;

    DebugP_log("EQEP Speed Direction Test Started ...\r\n");
	DebugP_log("Please wait few seconds ...\r\n");              //huh
	
    gEqepBaseAddr = CONFIG_EQEP0_BASE_ADDR;

    HwiP_Params_init(&hwiPrms);     // Register & enable interrupt

    hwiPrms.intNum      = CSLR_R5FSS0_CORE0_CONTROLSS_INTRXBAR0_OUT_0;      // Integrate with Syscfg
    hwiPrms.callback    = &App_eqepIntrISR;                                 // Integrate with Syscfg
    hwiPrms.isPulse     = APP_INT_IS_PULSE;                                 // Integrate with Syscfg
    status              = HwiP_construct(&gEqepHwiObject, &hwiPrms);
    DebugP_assert(status == SystemP_SUCCESS);

    EQEP_loadUnitTimer(gEqepBaseAddr, UNIT_PERIOD); //radši to sem hodim i mimo syscfg
    EQEP_clearInterruptStatus(gEqepBaseAddr,EQEP_INT_UNIT_TIME_OUT|EQEP_INT_GLOBAL);     // Clear eqep interrupt 

    //čekej na deset měření, tzn 10 period = 10* UNIT_PERIOD = 10ms
    while( *(volatile uint32_t *) ((uintptr_t) &gCount) < 10) //vau hustý
    {
        //Wait for Speed results
    }
	
    DebugP_log("frequency of pulses: %i \r\n" , gFreq);

    DebugP_log("Expected speed = 3000 RPM, Measured speed = %i RPM \r\n", gSpeed);

	
	DebugP_log("Rotation direction = ");
	if(gDir==1)
	{
		DebugP_log("Clockwise, forward \r\n");
	}
	if(gDir==-1)
	{
		DebugP_log("Counter Clockwise, reverse \r\n");
	}

    DebugP_log("EQEP Speed Direction Test Passed!!\r\n");
    DebugP_log("All tests have passed!!\r\n");
    Board_driversClose();
    Drivers_close();
}


static void App_eqepIntrISR(void *handle)
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
   
   // gSpeed Calculation:
   // Motor has 4000 encoder holes (pulses per revolution)
   // gFreq [pulses/sec] / 4000 [pulses/rev] = revolutions/sec
   // (revolutions/sec) * 60 [sec/min] = RPM
   gSpeed = (int32_t)((int64_t)gFreq  / (int64_t)(4000UL * 60UL / QUAD_TIC));

   EQEP_clearInterruptStatus(gEqepBaseAddr,EQEP_INT_UNIT_TIME_OUT|EQEP_INT_GLOBAL);    // Clear interrupt flag
}
