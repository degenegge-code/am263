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
 *	EQEP: EQEP0_A / EQEP0_B -> B14 / A14 -> gpio 130 / 131 -> HSEC 102 / 100 -> J21_25 / J21_24
 * 
 * Internal Connections \n
 * - GPIO130 ->  eQEP0A , GPIO131 ->  eQEP0B (setup in syscfg as input of eqep, not issued gpio. přes xbary se vede jen EQEP index and strobe)
 *
 * Needs to have external connection to the source of q signal:
 * ePWM → PIN (output) → fyzický drát → PIN (input) → INPUTXBAR → EQEP
 * 
 * 1X Resolution: Count rising edge only
 * emulation: Counter unaffected by suspend
 * QEPI/Index Source: Signal comes from Device Pin
 * maximum of the counter: 4294967295
 * Position Counter Mode: Reset position on a unit time event
 * enable unit timer and Unit Timer Period : 2000000
 * INT XBAR		CONFIG_INT_XBAR0	EQEP0_INT	INT_XBAR_0 	woe
 * quadrature mode je tam implicitně
 * 
 * EQEP na AM263Px neumí vzít signál z EPWM interně!
 * SysCfg NEUMÍ udělat ePWM → INPUT XBAR → PWM XBAR → EQEP
 */


// Defines:
#define DEVICE_SYSCLK_FREQ  (uint32_t)(2e8) // Sysclk frequence 200MHz
#define APP_INT_IS_PULSE  (1U) // Macro for interrupt pulse
#define UNIT_PERIOD  (uint32_t)(DEVICE_SYSCLK_FREQ / 1e3) // period timer value, at which timer-out interrupt is set, takže na 1ms: 2e8 / 1e3 = 2e5
#define QUAD_TIC 4U //Quadrature-clock mode

// Global variables and objects :
uint32_t gEqepBaseAddr = CONFIG_EQEP0_BASE_ADDR;
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

/*
 * eqep_speed_dir_init - Initializes eQEP for speed and direction measurement
 *                      and configures and enables the eQEP interrupt.
 */
void eqep_speed_dir_init(void *args)
{
    int32_t status;
    HwiP_Params hwiPrms;

    DebugP_log("EQEP Speed Direction Test Started ...\r\n");

    HwiP_Params_init(&hwiPrms);     // Register & enable interrupt

    hwiPrms.intNum      = CSLR_R5FSS0_CORE0_CONTROLSS_INTRXBAR0_OUT_2;      
    //  Syscfg: INT XBAR -> CONFIG_INT_XBAR1, EQEP0_INT, INT_XBAR_2
    hwiPrms.callback    = &App_eqepISR;                                 //předat do handle*
    hwiPrms.isPulse     = APP_INT_IS_PULSE;                                 
    status              = HwiP_construct(&gEqepHwiObject, &hwiPrms);
    DebugP_assert(status == SystemP_SUCCESS);

    EQEP_loadUnitTimer(gEqepBaseAddr, UNIT_PERIOD); //radši to sem hodim i mimo syscfg
    EQEP_clearInterruptStatus(gEqepBaseAddr,EQEP_INT_UNIT_TIME_OUT|EQEP_INT_GLOBAL);     // Clear eqep interrupts
    ClockP_usleep(10000);   //some time is needed to run before read, so its here
}

/*
 * eqep_measure10 - Waits until 10 unit timeouts have occurred to allow speed
 *                  and direction measurement to be completed.
 */
void eqep_measure10(void)
{
    //čekej na deset měření, tzn 10 period = 10* UNIT_PERIOD = 10ms
    while( *(volatile uint32_t *) ((uintptr_t) &gCount) < 10) //vau hustý
    {
        //Wait for Speed results
    }
}

/*
 * eqep_close - Closes the EQEP driver and prints the final direction of rotation.
 */
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
        DebugP_log("AAAAAAAAAAAAAAAA HELP \r\n");   //it should return dir = 0 as counterclockwise?

    }

    DebugP_log("EQEP tests have passed!!\r\n");
}

/*
 * eqep_freq - returns the measured frequency with sign indicating direction
 *            positive for forward, negative for reverse
 */
int32_t eqep_freq(void)
{
    if (gDir==0)
    {
        DebugP_log("Dir is 0!!!");
        return 666;
    }
    else {
        return gDir*gFreq;
    }
}


/*
 * App_eqepISR - Interrupt Service Routine for eQEP to calculate speed and direction
 *               of motor using eQEP position counter value.
 * void handle:  Pointer to the eQEP instance in hwiparams
 */
static void App_eqepISR(void *handle)
{
    gCount++;     // Increment count value 
    gNewcount = EQEP_getPositionLatch(gEqepBaseAddr);     // New position counter value 

    gDir = EQEP_getDirection(gEqepBaseAddr);    // Gets direction of rotation of motor 

   // gFreq Calculation:
   // gNewcount = position pulses accumulated in UNIT_PERIOD (1 ms by default)
   // UNIT_PERIOD = 2e5 ticks at 200 MHz sysclock = 1 ms
   // gFreq [pulses/sec] = gNewcount [pulses/ms] * (1e3 ms/sec)
    gFreq = (int32_t)((int64_t)gNewcount * 1000 / QUAD_TIC);
   
   EQEP_clearInterruptStatus(gEqepBaseAddr,EQEP_INT_UNIT_TIME_OUT|EQEP_INT_GLOBAL);    // Clear interrupt flag
}

/*
AM263Px má tyto XBAR typy:
    INPUT XBAR
    PWM XBAR
    HRCAP XBAR
    DMAXBAR (pro DMA triggery)
    INT XBAR (interrupt router)
INPUT XBAR – funguje JEN jako GPIO→Periferie. K přivedení externího signálu z pinu do vybraných modulů: EQEP, ECAP, EPWM sync-in, CMPSS, SDFM, případně další modulo (?)
- Co může být zdroj INPUT XBAR? Pouze GPIO piny (0–191), NIC JIN0HO!        GPIOx → INPUT XBAR → EQEP / ECAP / EPWM / SDFM / CMPSS
PWM XBAR – funguje JEN jako periferie→GPIO . Možné zdroje: EPWM tzv. Trip signals, CMPSS, DCAEVT/DCBEVT, INPUTXBAR (tj. zase GPIO), některé další interní zdroje
- PWMXBAROUT → EQEP, PWMXBAROUT → INPUTXBAR, PWMXBAROUT → ECAP, PWMXBAROUT → EPWM (kromě trip)
INT XBAR — pouze na routování interruptů (mapuje vnitřní zdroje do CPU IRQ linek)

původní přístup nefungoval, tzn vyvedu na jeden pin a z toho samého čtu pres crossbary. AM263Px GPIO pin nemá interní loopback. 
Tzn: když nastavim pin jako výstup (EPWM1_A), nejdee současně číst jeho logickou hodnotu dovnitř přes INPUT XBAR.
Přímo nelze poslat signál z epwm do eqep. 
A TI to v dokumentaci (TRM) uvádí nepřímo, nebo tak divně prostě, ale GUI syscfgu mne k tomu nepustí, jak bych chtěl. 
takhle přes externí piny je to stejně užitečnější modelová sotuace. 
*/