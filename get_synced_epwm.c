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
 * - PWM frequency is specified as 10 kHz, asymmetrical comaparation 
 * - modulating cosinus 1kHz
 * - TODO: deadtimes 
 *
 * Internal Connections SYSCFG:
 * EPWM 4A/4B -> D1 / E4 -> HSEC 57 / 59 -> J21_5 / J21_6 (generuje up, komplemtární a/b uměle, moduluje cos)
 * - INT_XBAR_4
 *
 *
 *
 * ePWM5A/B is configured to simulate an output of DC/DC
 * 
 * The configuration for this example is as follows
 * - PWM frequency is specified as 50 kHz
 * ePWM5A se sepne za ZERO a vypne při CMPA, ePWM5B se zapne na TBPRD a vypne na CMPB
 * after 3 secs it synchronises with external signal source (EPWM0B signal)
 * width of pulse 5% and phase shift to sync 10%
 * ePWM6A/B same functionality, checking multiple synchro
 *  
 * Internal Connections SYSCFG:
 * EPWM 5A/5B -> F2 / G2 -> HSEC 61 / 63 -> J20_7 / J20_8 (generuje 5% pulsy 50kHz)
 * GPIO65 -> H1 -> HSEC 86 -> J21_18 (input pro signal 50kHz)
 * EPWM 6A/6B -> E1 / F3 -> HSEC 58 / 60 -> J21_5 / J21_6 (same functionality, checking multiple synchro)
 * INPUT_XBAR_OUT4 -> EPWM5 SYNCI and EPWM6 SYNCI
 * 
 * pozn.: GPIO0 -> P1 -> na HSECu asi neni? jedniy HSEC_GPIO je tam 41, ale to je tam jak HSEC_5V0
 */
 

// Defines 
// Sysclk frequency 
#define DEVICE_SYSCLK_FREQ  (200000000UL)           //sysclk 200MHz

//PWM output at 10 kHz 
#define PWM_CLK   (10000u)                          //PWM output at 10 kHz  
#define PWM_PRD   (DEVICE_SYSCLK_FREQ / PWM_CLK)    //for period register for up mode
#define SAWS_IN_SIN  (10U)

//PWM output at 50kHz
#define PWM_CLK_2 (50000u)                          //PWM output at 50kHz
#define PWM_PRD_2   (DEVICE_SYSCLK_FREQ / PWM_CLK_2 / 2) //tbprd pro up down
#define PULSE_WIDTH (PWM_PRD_2 /10)       //dvacetina periody je puls 
#define OFFSET_TICS_2     (PWM_PRD_2/5)   // o 10% T posunuto oproti synchru 

#define APP_INT_IS_PULSE    (1U)    //pro interrupt type pulse


// Global variables and objects 
uint32_t gEpwmBaseAddr4 = CONFIG_EPWM4_BASE_ADDR;       //sinus generator
uint32_t gEpwmBaseAddr5 = CONFIG_EPWM5_BASE_ADDR;       //50kHz pwm to be synced
uint32_t gEpwmBaseAddr6 = CONFIG_EPWM6_BASE_ADDR;       //50kHz pwm to be synced 2

static uint32_t i = 0;                         //index pro sinus
static HwiP_Object  gEpwmHwiObject4;           //objekt 
static uint32_t vals[SAWS_IN_SIN];               //hodnoty pro sinus

// prototypy
static void App_genISR(void *handle);       //ISR pro generátor sinu


// Functions

/*
 * submissive_gen - sets up ePWM4 as a submissive generator of cosinus modulated PWM signal
 *                  with frequency 1kHz and PWM frequency 10kHz. ePWM4 is synced to ePWM3 rising 
                    edge, internally. EPWM4A/B -> D1 / E4 -> HSEC 57 / 59 -> J21_5 / J21_6   
 * Note: set up syscfg for epwm3 syncout, initialize epwms, enable global loads
 */
void submissive_gen(bool true_for_osc)
{
    DebugP_log("submissive_gen \n");    

    HwiP_Params  hwiPrms4;     //initialize interrupt parameters
    int32_t  status;            //status variable

    // EPWM4 register and enable interrupt: 
    HwiP_Params_init(&hwiPrms4);
    hwiPrms4.intNum      = CSLR_R5FSS0_CORE0_CONTROLSS_INTRXBAR0_OUT_4;
    hwiPrms4.isPulse     = APP_INT_IS_PULSE;
    hwiPrms4.callback    = &App_genISR;
    status               = HwiP_construct(&gEpwmHwiObject4, &hwiPrms4);
    DebugP_assert(status == SystemP_SUCCESS);

    //naplnim vals pro vypočet sinu, každou periodu 10 vzorků
    for (uint32_t y=0; y<SAWS_IN_SIN; y++)
    {
        vals[y] = (uint32_t)((cos(2.0*M_PI*((double)y/(double)SAWS_IN_SIN))+1.0)/2.0*PWM_PRD);   
    }

    //enable gloabl loads a Enable EPWM Interrupt v syscfgu, pak:
    EPWM_setClockPrescaler(gEpwmBaseAddr4, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);   //no prescalers

    EPWM_setTimeBasePeriod(gEpwmBaseAddr4, PWM_PRD);                    //set period for up mode
    EPWM_setTimeBaseCounterMode(gEpwmBaseAddr4, EPWM_COUNTER_MODE_UP);        //up mode
    EPWM_setTimeBaseCounter(gEpwmBaseAddr4, 0);                           //set counter to 0, init

    EPWM_setCounterCompareValue(gEpwmBaseAddr4, EPWM_COUNTER_COMPARE_A, 0);      //initial CMPA value

    //set interrupt on every period match (should make it even without syscfg)
    EPWM_setInterruptSource(gEpwmBaseAddr4, EPWM_INT_TBCTR_ZERO, EPWM_INT_MIX_TBCTR_ZERO);
    EPWM_enableInterrupt(gEpwmBaseAddr4);
    EPWM_setInterruptEventCount(gEpwmBaseAddr4, 1U); //interrupt každou periody
    
    EPWM_setActionQualifierAction(gEpwmBaseAddr4, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH,  EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO); //on 0 out HIGH
    EPWM_setActionQualifierAction(gEpwmBaseAddr4, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA); //on CMPA up count out LOW
    EPWM_setActionQualifierAction(gEpwmBaseAddr4, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO); //on 0 out LOW
    EPWM_setActionQualifierAction(gEpwmBaseAddr4, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA); //on CMPA up count out HIGH

    //FIXME: deadband nastavení - zatím bez deadbandu
    ////jestli to nebude kolidovat s deadband unit, tak bych to nechal na cmpa vše
    //EPWM_setRisingEdgeDeadBandDelayInput(gEpwmBaseAddr4, EPWM_DB_INPUT_EPWMA);
    //EPWM_setRisingEdgeDeadBandDelayInput(gEpwmBaseAddr4, EPWM_DB_INPUT_EPWMB);
    //EPWM_setDeadBandDelayPolarity(gEpwmBaseAddr4, EPWM_DB_RED, EPWM_DB_POLARITY_ACTIVE_HIGH);


    /* pseudocode:
     * EPWM4->TBPHS = phase_counts;                     // kolik ticků offset
     * EPWM4->TBCTL.PHSEN = 1;                          // povolit načítání TBPHS při SYNCI
     * EPWM4->EPWMSYNCINSEL.SEL = EPWM3_SYNCOUT_SEL;    // selekce zdroje SYNCI = EPWM3
    */
    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr4);            //Clear any pending interrupts if any


    DebugP_log("submissive_gen of cos 1kHz (fsw 10kHz) \n");

    //for exemplar display on oscilloscope: 
    if (true_for_osc)
    {
        DebugP_log("submissive_gen: press single on oscilloscope\n"); 
        ClockP_sleep(3);      //so you can see the waveform before sync starts
    }
    else {
        //nic
    }
    EPWM_enablePhaseShiftLoad(gEpwmBaseAddr4);  //need this
    EPWM_setPhaseShift(gEpwmBaseAddr4, 0);      //zero phase shift, sync on rising edge
    EPWM_setSyncInPulseSource(gEpwmBaseAddr4, EPWM_SYNC_IN_PULSE_SRC_SYNCOUT_EPWM3); //sync to epwm3

    DebugP_log("internally synced epwm4 with epwm3 (rising edges) \n");

}

/*
 * App_genISR - Interrupt Service Routine for ePWM4 to update CMPA value to generate 
 *               a cosinus modulated PWM signal. 
 * void handle:  Pointer to the ePWM instance in hwiparams
 */
static void App_genISR(void *handle)
{
    EPWM_setCounterCompareValue(gEpwmBaseAddr4, EPWM_COUNTER_COMPARE_A, vals[i]); //sin
    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr4);     // Clear any pending interrupts if any

    if (i < SAWS_IN_SIN-1) // indexuju s nulou, takže -1. prochazim pole. 
    {
        i ++;
    }
    else 
    {
        i = 0;
    }
}

/*
 * pwm_5p_off10 - sets up ePWM5 as a pwm signal with frequency 50kHz and pulse width 5% of period
 *                  after 3 seconds it synchronises with external signal source (EPWM0B signal f.ex.)
 *                  ePWM5A/B -> F2 / G2 -> HSEC 61 / 63 -> J20_7 / J20_8   
 *                  10% shift in comparison to source signal through gpio and xbar. 
 * /param true_for_shift - if true, phase shift of 10% is applied, else zero phase shift
 * Note: set up syscfg for inputxbar out4 to be connected to ePWM5 syncin, set initialize epwm5, e
 *       nable global loads, dont need interrupt, set comps and period as needed, well set everything there
 */
void pwm_5p_off10(bool true_for_shift, bool true_for_osc)
{
    DebugP_log("pwm_5p_off10\r\n");

    EPWM_setCounterCompareValue(gEpwmBaseAddr5, EPWM_COUNTER_COMPARE_A, PULSE_WIDTH); //set comp a
    EPWM_setCounterCompareValue(gEpwmBaseAddr5, EPWM_COUNTER_COMPARE_B, PWM_PRD_2 - PULSE_WIDTH); //set comp b
    EPWM_setTimeBasePeriod(gEpwmBaseAddr5, PWM_PRD_2); // set period, up mode, tzn je to pravdu perioda

    //všechno ostatní v syscfgu, aby to bylo pořádně nepřehlendé...

    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr5);            //Clear any pending interrupts if any

    // v syscfgu GPIO -> INPUT XBAR -> ePWM SYNCI
    // nebylo by spolehlivější? pin -> eCAP -> XBAR -> ePWM SYNCI
    // možná ano, ale tohle jednoduché funguje perfektně

    EPWM_enablePhaseShiftLoad(gEpwmBaseAddr5);  //need this
    if (true_for_shift)
    {
        EPWM_setPhaseShift(gEpwmBaseAddr5, OFFSET_TICS_2);    //phase shift 10% of period
    }
    else
    {
        EPWM_setPhaseShift(gEpwmBaseAddr5, 0);    //zero phase shift, sync on rising edge, but with offset in inputxbar
    }

    if(true_for_osc)
    {
        DebugP_log("pwm_5p_off10: press single on oscilloscope \n");
        ClockP_sleep(3);    //to see unsynced waveform press single on scope and then compare after 3secs
    }
    else {
    //nic
    }
    EPWM_setSyncInPulseSource(gEpwmBaseAddr5, EPWM_SYNC_IN_PULSE_SRC_INPUTXBAR_OUT4);  //inputxbar pouze 4 a 20!
    DebugP_log("pwm_5p_off10: pwm5 externally synced \n");

    //po synchronizaci na jedné desce je opožděna za synchronizačním signálem o 54ns 
    // po synchronizaci z jiné desky je zpoždení 54-59ns. hodiny nejsou uplně stejný, takže ripple
    // toto odpovídá 10 tikům systémových hodin 200MHz = 5ns na tik
}

/*
 * pwm_5p_off10_2 - sets up ePWM6 as a pwm signal with frequency 50kHz and pulse width 5% of period
 *                  after 3 seconds it synchronises with external signal source (EPWM0B signal f.ex.)
 *                  ePWM6A/B -> E1 / F3 -> HSEC 58 / 60 -> J21_5 / J21_6   
 *                  10% shift in comparison to source signal through gpio and xbar. for description
 *                  see pwm_5p_off10, it is the same. 
 * /param true_for_shift - if true, phase shift of 10% is applied, else zero phase shift
 * Note: set up syscfg for inputxbar out4 to be connected to ePWM6 syncin, set initialize epwm6,
 * enable global loads, dont need interrupt, set comps and period as needed, well set everything there
 */
void pwm_5p_off10_2(bool true_for_shift, bool true_for_osc)
{
    DebugP_log("pwm_5p_off10_2\r\n");
    EPWM_setCounterCompareValue(gEpwmBaseAddr6, EPWM_COUNTER_COMPARE_A, PULSE_WIDTH);
    EPWM_setCounterCompareValue(gEpwmBaseAddr6, EPWM_COUNTER_COMPARE_B, PWM_PRD_2 - PULSE_WIDTH);
    EPWM_setTimeBasePeriod(gEpwmBaseAddr6, PWM_PRD_2);
    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr6); 
    EPWM_enablePhaseShiftLoad(gEpwmBaseAddr6);
    if (true_for_shift) EPWM_setPhaseShift(gEpwmBaseAddr6, OFFSET_TICS_2);  
    else EPWM_setPhaseShift(gEpwmBaseAddr6, 0); 
    if(true_for_osc)
    {
        DebugP_log("pwm_5p_off10_2: press single on oscilloscope \n");
        ClockP_sleep(3);    
    }
    EPWM_setSyncInPulseSource(gEpwmBaseAddr6, EPWM_SYNC_IN_PULSE_SRC_INPUTXBAR_OUT4);
    DebugP_log("pwm_5p_off10_2: pwm6 externally synced \n");
}
