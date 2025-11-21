#include <drivers/epwm.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/HwiP.h>


/*
 *
 * ePWM2A is configured to simulate motor encoder signals with freqof 200 kHz on
 * both A and B pins with 90 degree phase shift (so as to run this example without
 * motor).
 * The configuration for this example is as follows
 * - PWM frequency is specified as 200 000Hz
 * Internal Connections \n
 * - ePWM2A -> GPIO47 -> INPUTXBAR1 -> PWMXBAR1 -> eQEP0A
 * - ePWM2B -> GPIO48 -> INPUTXBAR2 -> PWMXBAR2 -> eQEP0B
 *
 * trying everything through this only, WITHOUT syscfg
 */
 
    /*NASTAVENÍ PWMKY TAKTO, ABY ODPOVÍDALA IRC ČIDLU
     *  
     * PRD      |    /\        /\        /\
     *          |   /  \      /  \      /  \
     * CMPB     |--x----x----x----x----x----x
     *          | /      \  /      \  /      \
     * ZERO     |/________\/________\/________\/>
     *          |
     *          |
     *          |¯¯¯¯¯¯¯¯¯|          |¯¯¯¯¯¯¯¯¯|
     * EPWMA    |         |__________|         |>
     *          |
     *          |
     *          |    |¯¯¯¯¯¯¯¯¯|          |¯¯¯¯¯>
     * EPWMB    |____|         |__________|
     *
     */


// Defines 
// Sysclk frequency 
#define DEVICE_SYSCLK_FREQ  (200000000U)

//PWM output at 200 kHz
#define PWM_CLK   (200000u)
#define PWM_PRD   ((DEVICE_SYSCLK_FREQ / PWM_CLK / 2 / 4))  //no prsc and up-down 200 kHz, fastest. FIXME:  z nějakýho důvodu tam musim přidat dělení 4 - s těmi je to 200kHz

// Global variables and objects 
uint32_t gEpwmBaseAddr = CONFIG_EPWM2_BASE_ADDR;
uint32_t epwmInstance = 2;

//upřimně neumim udlat nějakej pin assign...
//stejně to musim aspoň pustit v syscfgu, jakože přidat další epwmku,a by to brtalo jmena těch maker atd. 
// takže to, kvuli čemu mam syscfg teď: povolit global loady nastavení a nastavení výstupního pinu (instance)

void irc_out_go(void)
{
    DebugP_log("irc_out_go");

    
    SOC_setEpwmTbClk( epwmInstance, FALSE); //FIXME: tohle nic nedělá

    EPWM_setClockPrescaler(gEpwmBaseAddr, 1, 1);
    EPWM_setTimeBasePeriod(gEpwmBaseAddr, PWM_PRD);
    EPWM_setTimeBaseCounterMode(gEpwmBaseAddr, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setTimeBaseCounter(gEpwmBaseAddr, 0);

    EPWM_setCounterCompareValue(gEpwmBaseAddr, EPWM_COUNTER_COMPARE_A, 0);      
    EPWM_setCounterCompareValue(gEpwmBaseAddr, EPWM_COUNTER_COMPARE_B, PWM_PRD/2); 

    EPWM_setActionQualifierAction(gEpwmBaseAddr, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH,  EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(gEpwmBaseAddr, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);
    EPWM_setActionQualifierAction(gEpwmBaseAddr, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB);
    EPWM_setActionQualifierAction(gEpwmBaseAddr, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPB);


    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddr);            //Clear any pending interrupts if any

    SOC_setEpwmTbClk(epwmInstance, TRUE);  //FIXME: tohle nic nedělá

    DebugP_log("irc pulses at 200kHz \n");
}
