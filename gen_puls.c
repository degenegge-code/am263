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
 * - ePWM3A -> E2 ->  GPIO49 -> INPUTXBAR3 -> PWMXBAR1 -> eQEP0A
 * - ePWM3B -> E3 ->  GPIO50 -> INPUTXBAR4 -> PWMXBAR2 -> eQEP0B
 *
 * 
 */
 

// Defines 
// Sysclk frequency 
#define DEVICE_SYSCLK_FREQ  (200000000U)

//PWM output at 200 kHz
#define PWM_CLK   (10000u)
#define PWM_PRD   ((DEVICE_SYSCLK_FREQ / PWM_CLK / 4))  //no prsc and up 10 kHz, FIXME:  z nějakýho důvodu tam musim přidat dělení 4 
#define SAWS_IN_SIN  (10U)
#define APP_INT_IS_PULSE    (1U)


// Global variables and objects 
uint32_t gEpwmBaseAddr3 = CONFIG_EPWM3_BASE_ADDR;
static uint32_t i = 0;
static HwiP_Object  gEpwmHwiObject3;           //objekt 
static uint32_t vals[SAWS_IN_SIN];

// prototypy
static void App_epwmgenISR(void *handle);

//upřimně neumim udlat nějakej pin assign...
//stejně to musim aspoň pustit v syscfgu, jakože přidat další epwmku,a by to brtalo jmena těch maker atd. 
// takže to, kvuli čemu mam syscfg teď: povolit global loady nastavení a nastavení výstupního pinu (instance)

void pwm_conv_gen(void)
{
    DebugP_log("pwm_conv_gen \n");

    HwiP_Params  hwiPrms3;     //initialize interrupt parameters
    int32_t  status;            //status

    // EPWM3 register and enable interrupt:
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

    //enable gloabl loads a Enable EPWM Interrupt v syscfgu
    EPWM_setClockPrescaler(gEpwmBaseAddr3, 1, 1);
    EPWM_setTimeBasePeriod(gEpwmBaseAddr3, PWM_PRD);
    EPWM_setTimeBaseCounterMode(gEpwmBaseAddr3, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setTimeBaseCounter(gEpwmBaseAddr3, 0);

    EPWM_setCounterCompareValue(gEpwmBaseAddr3, EPWM_COUNTER_COMPARE_A, 0);      
    EPWM_setCounterCompareValue(gEpwmBaseAddr3, EPWM_COUNTER_COMPARE_B, PWM_PRD); 

    EPWM_setActionQualifierAction(gEpwmBaseAddr3, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH,  EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(gEpwmBaseAddr3, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(gEpwmBaseAddr3, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(gEpwmBaseAddr3, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB);

    EPWM_setRisingEdgeDeadBandDelayInput(gEpwmBaseAddr3, EPWM_DB_INPUT_EPWMA);
    EPWM_setRisingEdgeDeadBandDelayInput(gEpwmBaseAddr3, EPWM_DB_INPUT_EPWMB);
    EPWM_setDeadBandDelayPolarity(gEpwmBaseAddr3, EPWM_DB_RED, EPWM_DB_POLARITY_ACTIVE_HIGH);


    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr3);            //Clear any pending interrupts if any


    DebugP_log("pwm of sin 1kHz (fsw 10kHz) \n");
}


static void App_epwmgenISR(void *handle)
{
    if (i < 10)
    {
        i ++;
    }
    else 
    {
        i = 0;
    }

    EPWM_setCounterCompareValue(gEpwmBaseAddr3, EPWM_COUNTER_COMPARE_A, vals[i]);
    EPWM_setCounterCompareValue(gEpwmBaseAddr3, EPWM_COUNTER_COMPARE_B, PWM_PRD - vals[i]);
    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr3);     // Clear any pending interrupts if any
}

