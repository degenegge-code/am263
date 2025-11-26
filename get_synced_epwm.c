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
 * ePWM4A/B is configured to simulate an output of  pwm converter
 * 
 * The configuration for this example is as follows
 * - PWM frequency is specified as 10 kHz
 * - asymmetrical comaparation 
 * - modulating cosinus 1kHz
 * - deadtimes 
 *  
 * Internal Connections SYSCFG:
 * EPWM 4A/4B -> D1 / E4 -> HSEC 57 / 59 -> J21_5 / J21_6 (generuje up, komplemtární a/b uměle, moduluje cos)
 * - INT_XBAR_4
 *
 * ePWM5A/B is configured to simulate an output of DC/DC
 * 
 * The configuration for this example is as follows
 * - PWM frequency is specified as 50 kHz
 * ePWM5A se sepne za ZERO a vypne při CMPA, ePWM5B se zapne na TBPRD a vypne na CMPB
 * after 3 secs it synchronises with external signal source (EPWM0B signal)
 * width of pulse 5% and phase shift to sync 10%
 *  
 * Internal Connections SYSCFG:
 * EPWM 5A/5B -> F2 / G2 -> HSEC 61 / 63 -> J20_7 / J20_8 (generuje 5% pulsy 50kHz)
 * GPIO65 -> H1 -> HSEC 86 -> J21_18 (input pro signal 50kHz)
 * 
 * pozn.: GPIO0 -> P1 -> na HSECu asi neni? jedniy HSEC_GPIO je tam 41, ale to je tam jak HSEC_5V0
 */
 

// Defines 
// Sysclk frequency 
#define DEVICE_SYSCLK_FREQ  (200000000UL)

//PWM output at 10 kHz
#define PWM_CLK   (10000u)
#define PWM_PRD   (DEVICE_SYSCLK_FREQ / PWM_CLK)
#define SAWS_IN_SIN  (10U)

//PWM output at 50kHz
#define PWM_CLK_2 (50000u)
#define PWM_PRD_2   (DEVICE_SYSCLK_FREQ / PWM_CLK_2 / 2) //tbprd pro up down
#define PULSE_WIDTH (PWM_PRD_2 /10)       //dvacetina periody je puls 
#define OFFSET_TICS_2     (PWM_PRD_2/5)   // o 10% T posunuto oproti synchru 

#define APP_INT_IS_PULSE    (1U)


// Global variables and objects 
uint32_t gEpwmBaseAddr4 = CONFIG_EPWM4_BASE_ADDR;
uint32_t gEpwmBaseAddr5 = CONFIG_EPWM5_BASE_ADDR;

static uint32_t i = 0;
static HwiP_Object  gEpwmHwiObject4;           //objekt 
static uint32_t vals[SAWS_IN_SIN];

// prototypy
static void App_genISR(void *handle);
static void sync_epwems(void);


//stejně to musim aspoň pustit v syscfgu, jakože přidat další epwmku,a by to brtalo jmena těch maker atd. 
// takže to, kvuli čemu mam syscfg teď: povolit global loady nastavení a nastavení výstupního pinu (instance)

void submissive_gen(void)
{
    DebugP_log("submissive_gen \n");

    HwiP_Params  hwiPrms4;     //initialize interrupt parameters
    int32_t  status;            //status

    // EPWM4 register and enable interrupt: - furt ho musim dat do cfgu
    HwiP_Params_init(&hwiPrms4);
    hwiPrms4.intNum      = CSLR_R5FSS0_CORE0_CONTROLSS_INTRXBAR0_OUT_4;
    hwiPrms4.isPulse     = APP_INT_IS_PULSE;
    hwiPrms4.callback    = &App_genISR;
    status               = HwiP_construct(&gEpwmHwiObject4, &hwiPrms4);
    DebugP_assert(status == SystemP_SUCCESS);

    //naplnim vals pro vypočet sinu
    for (uint32_t y=0; y<SAWS_IN_SIN; y++)
    {
        vals[y] = (uint32_t)((cos(2.0*M_PI*((double)y/(double)SAWS_IN_SIN))+1.0)/2.0*PWM_PRD);   
    }

    //enable gloabl loads a Enable EPWM Interrupt v syscfgu
    EPWM_setClockPrescaler(gEpwmBaseAddr4, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);

    EPWM_setTimeBasePeriod(gEpwmBaseAddr4, PWM_PRD);
    EPWM_setTimeBaseCounterMode(gEpwmBaseAddr4, EPWM_COUNTER_MODE_UP);
    EPWM_setTimeBaseCounter(gEpwmBaseAddr4, 0);

    EPWM_setCounterCompareValue(gEpwmBaseAddr4, EPWM_COUNTER_COMPARE_A, 0);      

    EPWM_setActionQualifierAction(gEpwmBaseAddr4, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH,  EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(gEpwmBaseAddr4, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(gEpwmBaseAddr4, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(gEpwmBaseAddr4, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

    //jestli to nebude kolidovat s deadband unit, tak bych to nechal na cmpa vše
    EPWM_setRisingEdgeDeadBandDelayInput(gEpwmBaseAddr4, EPWM_DB_INPUT_EPWMA);
    EPWM_setRisingEdgeDeadBandDelayInput(gEpwmBaseAddr4, EPWM_DB_INPUT_EPWMB);
    EPWM_setDeadBandDelayPolarity(gEpwmBaseAddr4, EPWM_DB_RED, EPWM_DB_POLARITY_ACTIVE_HIGH);


    /* pseudocode:
     * EPWM4->TBPHS = phase_counts;                     // kolik ticků offset
     * EPWM4->TBCTL.PHSEN = 1;                          // povolit načítání TBPHS při SYNCI
     * EPWM4->EPWMSYNCINSEL.SEL = EPWM3_SYNCOUT_SEL;    // selekce zdroje SYNCI = EPWM3
    */
    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr4);            //Clear any pending interrupts if any


    DebugP_log("submissive_gen of cos 1kHz (fsw 10kHz) \n");

    DebugP_log("submissive_gen: press single on oscilloscope\n");
    ClockP_sleep(3);

    EPWM_enablePhaseShiftLoad(gEpwmBaseAddr4);
    EPWM_setPhaseShift(gEpwmBaseAddr4, 0); 
    EPWM_setSyncInPulseSource(gEpwmBaseAddr4, EPWM_SYNC_IN_PULSE_SRC_SYNCOUT_EPWM3);

    DebugP_log("internally synced epwm4 with epwm3 (rising edges) \n");

}


static void App_genISR(void *handle)
{
    EPWM_setCounterCompareValue(gEpwmBaseAddr4, EPWM_COUNTER_COMPARE_A, vals[i]);
    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr4);     // Clear any pending interrupts if any

    if (i < SAWS_IN_SIN-1) // indexuju s nulou
    {
        i ++;
    }
    else 
    {
        i = 0;
    }
}

void pwm_5p_off10(void)
{
    DebugP_log("pwm_5p_off10\r\n");

    //set comp values HB:
    EPWM_setCounterCompareValue(gEpwmBaseAddr5, EPWM_COUNTER_COMPARE_A, PULSE_WIDTH);
    EPWM_setCounterCompareValue(gEpwmBaseAddr5, EPWM_COUNTER_COMPARE_B, PWM_PRD_2 - PULSE_WIDTH);
    EPWM_setTimeBasePeriod(gEpwmBaseAddr5, PWM_PRD_2);
    //všechno ostatní v syscfgu, aby to bylo pořádně nepřehlendé

    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr5);            //Clear any pending interrupts if any

    //v syscfgu GPIO -> INPUT XBAR -> ePWM SYNCI
    //nebylo by spolehlivější? pin -> eCAP -> XBAR -> ePWM SYNCI
    

    DebugP_log("pwm_5p_off10: press single on oscilloscope \n");

    EPWM_enablePhaseShiftLoad(gEpwmBaseAddr5);
    EPWM_setPhaseShift(gEpwmBaseAddr5, 0);

    ClockP_sleep(3);

    EPWM_setSyncInPulseSource(gEpwmBaseAddr5, EPWM_SYNC_IN_PULSE_SRC_INPUTXBAR_OUT4);  //inputxbar pouze 4 a 20
    DebugP_log("pwm_5p_off10: pwm5 externally synced \n");

    //po synchronizaci je opožděna za synchronizačním signálem o 54us ! 
}

