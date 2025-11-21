
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
 * - UNIT_PERIOD is specified as 10000 us - v pdfku je napsáno "to be defined"
 * - Simulated quadrature signal frequency is 20000Hz (4 * 5000) - původní blábol, proč je tam to 4* ????
 * - Encoder holes assumed as 4000 třeba nevim, aby zas nebylo velký rpm
 * - Thus Simulated motor speed is the maximum speed
 *       (200000 * (60 / 4000)) = 3 000 rpm
 *
 * freq : Simulated quadrature signal frequency measured by counting
 * the external input pulses for UNIT_PERIOD (unit timer set to 10 ms).
 * speed : Measure motor speed in rpm
 * dir : Indicates clockwise (1) or anticlockwise (-1)
 * 
 * 
 */ 


/*
 *
 * Internal Connections \n
 * - ePWM2A -> GPIO47 -> INPUTXBAR1 -> PWMXBAR1 -> eQEP0A
 * - ePWM2B -> GPIO48 -> INPUTXBAR2 -> PWMXBAR2 -> eQEP0B
 * 
 * 1X Resolution: Count rising edge only
 * emulation: Counter unaffected by suspend
 * QEPI/Index Source: Signal comes from Device Pin
 * maximum of the counter: 4294967295
 * Position Counter Mode: Reset position on a unit time event
 * enable unit timer and Unit Timer Period : 2000000
 * INT XBAR		CONFIG_INT_XBAR0	EQEP0_INT	INT_XBAR_0
 * INPUT XBAR naroutovat na EPWMXBAR (v epwmxbar, input xbary se vytvoří samy)
 * 
 */
// Defines:
#define DEVICE_SYSCLK_FREQ  (200000000U) // Sysclk frequence
#define APP_INT_IS_PULSE  (1U) // Macro for interrupt pulse
#define PWM_CLK (200000u)       //same as form irc sim (?)
#define UNIT_PERIOD  10000U // Specify the period in microseconds 

// Global variables and objects :
uint32_t gEqepBaseAddr;
static HwiP_Object gEqepHwiObject;
uint32_t gCount = 0;                    // Counter to check measurement gets saturated
uint32_t gOldcount = 0;                 // Stores the previous position counter value 
int32_t gFreq = 0;                      // Measured quadrature signal frequency of motor using eQEP 
float gSpeed = 0.0f;                    // Measured speed of motor in rpm 
int32_t gDir = 0;                       // Direction of rotation of motor 
uint32_t gPass = 0, gFail = 0;          // Pass or fail indicator 
volatile uint32_t gNewcount = 0 ;

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

    EQEP_clearInterruptStatus(gEqepBaseAddr,EQEP_INT_UNIT_TIME_OUT|EQEP_INT_GLOBAL);     // Clear eqep interrupt 

    while( *(volatile uint32_t *) ((uintptr_t) &gCount) < 10)
    {
        //Wait for Speed results
    }
	
    if((gPass == 1) && (gFail == 0))
    {
		DebugP_log("Expected speed = 3000 RPM, Measured speed = %f RPM \r\n", gSpeed);	 		// Expected 3 000 RPM based on programmed EPWM frequency

		
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
    }
    else
    {
        DebugP_log("Fail\r\n");
    }

    Board_driversClose();
    Drivers_close();
}



// eqep0 ISR--interrupts once every 4 QCLK counts (one period) 
static void App_eqepIntrISR(void *handle)
{
    gCount++;     // Increment count value 
    gNewcount = EQEP_getPositionLatch(gEqepBaseAddr);     // New position counter value 

    gDir = EQEP_getDirection(gEqepBaseAddr);    // Gets direction of rotation of motor 


   /* Calculates the number of position in unit time based on
      motor's direction of rotation */

   if (gDir > 0 ){
        // Do vubec nic
        }
   else {
            gNewcount = (0xFFFFFFFF - gNewcount) + 1;
        }

   gOldcount = gNewcount;    // Stores the current position count value to oldcount variable 

   gFreq = (gNewcount * (uint32_t)1000000U)/((uint32_t)UNIT_PERIOD);    // Simulovana frekvenca
   gSpeed = (gFreq * 60)/4000.0f;                                       //simulovana rychlsost

   // Compares the measured quadrature simulated frequency with input quadrature frequency and if difference is within the measurement resolution
   // (1/10ms = 100Hz) then pass = 1 else fail = 1
   if (gCount >= 2){
        if(((gFreq - PWM_CLK*4) <100) && ((PWM_CLK*4 - gFreq ) < 100)){
            gPass = 1; 
            gFail = 0;
        }else {
            gPass = 0;  
            gFail = 1;
        }
    }

   EQEP_clearInterruptStatus(gEqepBaseAddr,EQEP_INT_UNIT_TIME_OUT|EQEP_INT_GLOBAL);    // Clear interrupt flag
}
