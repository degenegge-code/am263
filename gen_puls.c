#include <drivers/epwm.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/HwiP.h>
#include <math.h>


/*
 *
 * ePWM3A is configured to simulate an output of  pwm converter
 * 
 * The configuration for this example is as follows
 * - PWM frequency is specified as 10 kHz
 * - asymmetrical comaparation 
 * - modulating sinus 1kHz
 * - deadtimes 
 *  
 * Internal Connections SYSCFG:
 * EPWM 3A/3B -> E3 / E2 -> HSEC 54 / 56 -> J21_3 / J21_4 (generuje up, komplemtární a/b uměle, moduluje sin)
 * - INT_XBAR_3
 *
 * SYNC může způsobit ±1-2 cycle jitter; TRM varuje, aby SYNCO source nebyl CTR=0 nebo CTR=CMPB pokud chci bez jitteru
 */
 

// Defines 
// Sysclk frequency 
#define DEVICE_SYSCLK_FREQ  (200000000UL)

//PWM output at 10 kHz
#define PWM_CLK   (10000u)
/*
 * původně vypadal kod takhle:
 * ...
 * #define PWM_PRD   ((DEVICE_SYSCLK_FREQ / PWM_CLK)/4)  //no prsc and up 10 kHz, FIXME: what is going on???
 * ...
 * EPWM_setClockPrescaler(gEpwmBaseAddr3, 1, 1);
 * ... 
 * což by vypadalo, jako že je prescaler nastaven na 1, tzn, žádné dělení, ale tak to není, protože 
 * 1 reprezentuje 2. tzn 0 je bez dělení. EPWM_CLOCK_DIVIDER_1 = 0
 * jen tak pro pobavení 
*/
#define PWM_PRD   (DEVICE_SYSCLK_FREQ / PWM_CLK)
#define SAWS_IN_SIN  (10U)
#define APP_INT_IS_PULSE    (1U)


// Global variables and objects 
uint32_t gEpwmBaseAddr3 = CONFIG_EPWM3_BASE_ADDR;
static uint32_t i = 0;
static HwiP_Object  gEpwmHwiObject3;            
static uint32_t vals[SAWS_IN_SIN];

// prototypy
static void App_epwmgenISR(void *handle);

// Functions

/*
 * pwm_conv_gen - sets up ePWM3 as a generator of sinus modulated PWM signal
 *                  with frequency 1kHz and PWM frequency 10kHz. 
 *                  EPWM3A/B -> E3 / E2 -> HSEC 54 / 56 -> J21_3 / J21_4   
 * Note: set up syscfg for epwm3, enable global loads, enable interrupt
 */
void pwm_conv_gen(void)
{
    DebugP_log("pwm_conv_gen \n");

    HwiP_Params  hwiPrms3;     //initialize interrupt parameters
    int32_t  status;            //status

    // EPWM3 register and enable interrupt: (it doesnt enable tho??)
    HwiP_Params_init(&hwiPrms3);
    hwiPrms3.intNum      = CSLR_R5FSS0_CORE0_CONTROLSS_INTRXBAR0_OUT_3;
    hwiPrms3.isPulse     = APP_INT_IS_PULSE;
    hwiPrms3.callback    = &App_epwmgenISR;
    status              =   HwiP_construct(&gEpwmHwiObject3, &hwiPrms3);
    DebugP_assert(status == SystemP_SUCCESS);

    //naplnim vals pro vypočet sinu
    for (uint32_t y=0; y<SAWS_IN_SIN; y++)
    {
        vals[y] = (uint32_t)((sin(2.0*M_PI*((double)y/(double)SAWS_IN_SIN))+1.0)/2.0*PWM_PRD);   
    }

    //enable gloabl loads a Enable EPWM Interrupt v syscfgu, pořád nutné .
    EPWM_setClockPrescaler(gEpwmBaseAddr3, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);

    EPWM_setTimeBasePeriod(gEpwmBaseAddr3, PWM_PRD);
    EPWM_setTimeBaseCounterMode(gEpwmBaseAddr3, EPWM_COUNTER_MODE_UP);
    EPWM_setTimeBaseCounter(gEpwmBaseAddr3, 0);

    EPWM_setCounterCompareValue(gEpwmBaseAddr3, EPWM_COUNTER_COMPARE_A, 0);      
    EPWM_setCounterCompareValue(gEpwmBaseAddr3, EPWM_COUNTER_COMPARE_B, PWM_PRD); 

    EPWM_setActionQualifierAction(gEpwmBaseAddr3, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH,  EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(gEpwmBaseAddr3, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(gEpwmBaseAddr3, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(gEpwmBaseAddr3, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

    //TODO: deadband nastavení - zatím bez deadbandu
    ////jestli to nebude kolidovat s deadband unit, tak bych to nechal na cmpa vše
    //EPWM_setRisingEdgeDeadBandDelayInput(gEpwmBaseAddr3, EPWM_DB_INPUT_EPWMA);
    //EPWM_setRisingEdgeDeadBandDelayInput(gEpwmBaseAddr3, EPWM_DB_INPUT_EPWMB);
    //EPWM_setDeadBandDelayPolarity(gEpwmBaseAddr3, EPWM_DB_RED, EPWM_DB_POLARITY_ACTIVE_HIGH);
    

    /*
     * // pseudocode 
     * EPWM3->TBPRD = myPeriod;
     * EPWM3->TBCTL.CTRMODE = UP_COUNT;  
     * EPWM3->TBCTL.SYNCOSEL = SYNCO_CTR_ZERO; 
     *
     */
    EPWM_enableSyncOutPulseSource(gEpwmBaseAddr3, EPWM_SYNC_OUT_PULSE_ON_CNTR_ZERO);

    /* Pak lze i vytáhnout synchronizační signál ven přes XBAR a pinmux:
     * Vytáhnout SYNCOUT ven (route to pin) -> PWMSYNCOUTXBAR
     * PWMSyncOutXBar.Out[n] a přiřaď zdroj např. ePWMx SYNCOUT
     * PWMSyncOutXBar.G0.SEL[XX] = SELECT_EPWM3_SYNCO; // konfiguruj XBAR
     * OUTPUTXBAR.G9.0.SELECT = PWMSyncOutXBar.Out0;
     * PINMUX_configurePin(PIN_X, FUNC_OUTPUTXBAR_G9_0);
     */

    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr3);            //Clear any pending interrupts if any

    DebugP_log("pwm of sin 1kHz (fsw 10kHz) \n");
}

/*
 * App_epwmgenISR - Interrupt Service Routine for ePWM3 to update CMPA value to generate 
 *               a sinus modulated PWM signal. 
 * void handle:  Pointer to the ePWM instance in hwiparams
 */
static void App_epwmgenISR(void *handle)
{
    EPWM_setCounterCompareValue(gEpwmBaseAddr3, EPWM_COUNTER_COMPARE_A, vals[i]);
    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr3);     // Clear any pending interrupts if any

    if (i < SAWS_IN_SIN-1) // indexuju s nulou
    {
        i ++;
    }
    else 
    {
        i = 0;
    }
}

