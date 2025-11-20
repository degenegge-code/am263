
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/HwiP.h>
#include <drivers/epwm.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

/*
 * Example Description
 * This example configures EPWM0's and EPWM1's EPWMA and EPWMB 
 *
 * The TB counter is in up-down count mode for this example.
 *
 * During the test, monitor EPWM0,1 outputs on an oscilloscope.
 *
 * On AM263x CC/ AM263Px CC with HSEC Dock,
 * Probe the following on the HSEC pins
 *  - EPWM 0A/0B : 49 / 51
 *  - EPWM 1A/1B : 53 / 55
 * ePWM0A se sepne za ZERO a vypne při CMPA - tzn při čítání NAHORU, dolu je no action
 * ePWM0B se zapne na TBPRD a vypne na CMPB - tzn při čítání DOLU, nahoru no asction
 * 
 * ePWM1A and B identical functions - but timebase shifted 5% behind 
 * nastavení těchto funkcí v .syscfg
 *
 */

#define APP_INT_IS_PULSE    (1U)
#define EPWM_FREQ 50000u    //50khz - to odpovídá i pile - toto nastaveno v syscfg, zdejší nepoužito

//H bridge:
// prd = F_ 1/f /2 /prsc , 400Mhz / 50kHz /2 /2 = 2000
#define EPWM_TIMER_TBPRD    1000// Period register, prd je vrcholová hodnota - ? - toto nastaveno v syscfg EPWM Time Base, zdejší dále nepoužito
#define EPWM_WIDTH          EPWM_TIMER_TBPRD/2 //čtvrztin periody je puls 

//USM:
#define OFFSET_TICS     EPWM_TIMER_TBPRD/10 // synchro s hbr ale o 5%T posunuto PŘED něj - toto nastaveno v syscfg EPWM Time Base Initial Counter Value, zdejší nepoužito


//proměnné:
volatile uint16_t compAVal0, compBVal0, compAVal1, compBVal1;         //CMPA CMPB pro epwm 0a1
static HwiP_Object  gEpwmHwiObject_0;           //objekt h bridge
static HwiP_Object  gEpwmHwiObject_1;           //objekt usm
uint32_t gEpwm0Base = CONFIG_EPWM0_BASE_ADDR;   //base adresu epwm H bridge si vezmi z konfigu
uint32_t gEpwm1Base = CONFIG_EPWM1_BASE_ADDR;   //base adresu epwm USM si vezmi z konfigu

//prototypy:
static void App_epwmIntrISR_0(void *handle);    //zatim je to jen pro clear int
static void App_epwmIntrISR_1(void *handle);


void epwm_updown(void *args)
{
    int32_t  status;            //status
    HwiP_Params  hwiPrms_0;     //initialize interrupt parameters
    HwiP_Params  hwiPrms_1;     //initialize interrupt parameters

    uint32_t epwmsMask = (1U << 0U) | (1U << 1U) ;      //nastav masku pro první dvě epwemky
                                                        // Check the syscfg for configurations !!!

    DebugP_log("EPWMTest Started ...\r\n");
    DebugP_log("EPWM Action Qualifier Module using tadytenbordel\r\n");

    SOC_setMultipleEpwmTbClk(epwmsMask, FALSE);     // Disabling tbclk sync for EPWM 0 for configuration // for 0 1 2 epwms


    // EPWM0 register and enable interrupt:
    HwiP_Params_init(&hwiPrms_0);
    hwiPrms_0.intNum      = CSLR_R5FSS0_CORE0_CONTROLSS_INTRXBAR0_OUT_0;
    hwiPrms_0.isPulse     = APP_INT_IS_PULSE;
    hwiPrms_0.callback    = &App_epwmIntrISR_0;
    status              =   HwiP_construct(&gEpwmHwiObject_0, &hwiPrms_0);
    DebugP_assert(status == SystemP_SUCCESS);

    // EPWM1 register and enable interrupt:
    HwiP_Params_init(&hwiPrms_1);
    hwiPrms_1.intNum      = CSLR_R5FSS0_CORE0_CONTROLSS_INTRXBAR0_OUT_1;
    hwiPrms_1.isPulse     = APP_INT_IS_PULSE;
    hwiPrms_1.callback    = &App_epwmIntrISR_1;
    status              = HwiP_construct(&gEpwmHwiObject_1, &hwiPrms_1);
    DebugP_assert(status == SystemP_SUCCESS);

    //set comp values HB:
    EPWM_setCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_A, EPWM_WIDTH);
    EPWM_setCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_B, EPWM_TIMER_TBPRD - EPWM_WIDTH);
    //set comp values USM:
    EPWM_setCounterCompareValue(gEpwm1Base, EPWM_COUNTER_COMPARE_A, EPWM_WIDTH);
    EPWM_setCounterCompareValue(gEpwm1Base, EPWM_COUNTER_COMPARE_B, EPWM_TIMER_TBPRD - EPWM_WIDTH);

    EPWM_clearEventTriggerInterruptFlag(gEpwm0Base);            //Clear any pending interrupts if any
    EPWM_clearEventTriggerInterruptFlag(gEpwm1Base);

    SOC_setMultipleEpwmTbClk(epwmsMask, TRUE);      //Enabling tbclk sync for EPWM 0 after configurations, i pro epwm1

    compAVal0 = EPWM_getCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_A);
    compAVal1 = EPWM_getCounterCompareValue(gEpwm1Base, EPWM_COUNTER_COMPARE_A);
    compBVal0 = EPWM_getCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_B);
    compBVal1 = EPWM_getCounterCompareValue(gEpwm1Base, EPWM_COUNTER_COMPARE_B);

    DebugP_log("epwms set\n");

}

//konec mainu
void epwm_updown_close(void)
{
    DebugP_log("epwm0: CMPA: %i, CMPB: %i \r\n", compAVal0, compBVal0); //kontrolní 
    DebugP_log("epwm1: CMPA: %i, CMPB: %i \r\n", compAVal1, compBVal1);

    DebugP_log("EPWM Action Qualifier Module Test Passed!!\r\n");
    DebugP_log("All epwm Tests have Passed!!\n");

}

//interrupt service rutina - vezmi cmpam hodnoty, narvi je do vypisu, vyčisti přerušení
static void App_epwmIntrISR_0(void *handle)
{
    EPWM_clearEventTriggerInterruptFlag(gEpwm0Base);     // Clear any pending interrupts if any
}

static void App_epwmIntrISR_1(void *handle)
{
    EPWM_clearEventTriggerInterruptFlag(gEpwm1Base);     // Clear any pending interrupts if any
}


