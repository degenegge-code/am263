
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/HwiP.h>
#include <drivers/epwm.h>
#include <drivers/adc.h>
#include "ti_drivers_config.h"

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
 * Internal Connections \n
 * EPWM 1A/1B -> D3 / D2 -> HSEC 53 / 55 -> J20_3 / J20_4 
 *
 * ADC1 -> HSEC 12 -> ADC1_AIN0 -> J12_1
 * SOC 0 is triggered by ePWM0 CMPC and CMPD both in up and down, set in epwm syscfg
 * EPWM0 triggruje na každém CMPC, CMPD, kde si šahá pro hodnoty do ADC
 * - up to 16 SOCs can be triggered by ePWMs, 80 ns sample time 
 * IMPORTANT: ADC REF HAS TO BE SET ON-BOARD CORRECTLY (for this set SW9.1 1-2 and SW9.2 4-5)
 * 
 pozn:
 * V14 = ADC_VREFHI_G0, V13 = ADC_VREFLO_G0 nejsou na hsecu
 * V15 = ADC0_AIN0 (taky T5? tf) -> HSEC 9 -> J6_1 -> asi defaulktně nastavenej jako DAC
 * 
 */

#define APP_INT_IS_PULSE    (1U)
#define EPWM_FREQ 50000u    //50khz - to odpovídá i pile - toto nastaveno v syscfg, zdejší nepoužito

//H bridge:
// prd = F_ 1/f /2 /prsc , 400Mhz / 50kHz /2 /2 = 2000
#define EPWM_TIMER_TBPRD    1000u// Period register, prd je vrcholová hodnota - ? - toto nastaveno v 
                                // syscfg EPWM Time Base
#define EPWM_WIDTH          EPWM_TIMER_TBPRD/2u //čtvrztin periody je puls 

//ADC:
#define BUFF_LEN    16u
#define VREF        3.3

//USM:
#define OFFSET_TICS     EPWM_TIMER_TBPRD/10u // synchro s hbr ale o 5%T posunuto PŘED něj - toto 
                                            // nastaveno v syscfg EPWM Time Base Initial Counter 
                                            // Value, zdejší nepoužito

//proměnné:
volatile uint16_t compAVal0, compBVal0, compAVal1, compBVal1, compCVal0, compDVal0; 
static HwiP_Object  gEpwmHwiObject_0;           //objekt h bridge
static HwiP_Object  gEpwmHwiObject_1;           //objekt usm
uint32_t gEpwm0Base = CONFIG_EPWM0_BASE_ADDR;   //base adresu epwm H bridge si vezmi z konfigu
uint32_t gEpwm1Base = CONFIG_EPWM1_BASE_ADDR;   //base adresu epwm USM si vezmi z konfigu
static float gVals[BUFF_LEN];                 // proměnná čtení z adc, sliding average
static uint32_t i = 0;
uint32_t gAdcBaseAddr = CONFIG_ADC1_BASE_ADDR;
uint32_t gAdcResultBaseAddr = CONFIG_ADC1_RESULT_BASE_ADDR;

//prototypy:
static void App_epwmIntrISR_0(void *handle);    //zatim je to jen pro clear int
static void App_epwmIntrISR_1(void *handle);    //zatim je to jen pro clear int
float get_buffval(void);
void reset_buffval(void);


//funkce:

/*
 * epwm_updown - Configures and starts EPWM0 and EPWM1 in up-down count mode
 *               with Action Qualifier to generate PWM on EPWMA and EPWMB outputs.
 *               EPWM1 is time base shifted by 5% of period behind EPWM0.  
 *               EPWM0A/B -> C4 / C3 -> HSEC 49 / 51 -> J20_1 / J20_2 (H Bridge)
 *               EPWM1A/B -> D3 / D2 -> HSEC 53 / 55 -> J20_3 / J20_4 (USM)
 *               EPWM1 interrupts at bottom, every third
 *               EPWM0 interrupts at every CMPC/D, runs SOC for ADC1
 *
 * Note: Set up SYSCFG for EPWM0 and EPWM1, initialize epwms, enable global loads, interrupts,
 *       set comps and period as needed, prescaler to 1 implicit is 2?), up-down mode, all in syscfg.
 *
 */
void epwm_updown(void *args)
{
    int32_t  status;            //status
    HwiP_Params  hwiPrms_0;     //initialize interrupt parameters
    HwiP_Params  hwiPrms_1;     //initialize interrupt parameters

    uint32_t epwmsMask = (1U << 0U) | (1U << 1U) ;      //nastav masku pro první dvě epwemky
                                                        // Check the syscfg for configurations !!!

    DebugP_log("EPWMTest Started ...\r\n");
    DebugP_log("EPWM Action Qualifier Module using tadytodle\r\n");

    SOC_setMultipleEpwmTbClk(epwmsMask, FALSE);     // Disabling tbclk sync for EPWM 0 for configuration

    SOC_enableAdcReference(0); //this doesnt seem to be that important
    ADC_enableConverter(gAdcBaseAddr);    //already enabled in syscfg
    //reset_buffval();    //for first run debug read

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

    EPWM_setCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_C, 3*EPWM_TIMER_TBPRD/4);    //uprostřed pulsu
    EPWM_setCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_D, EPWM_TIMER_TBPRD/4);    

    SOC_setMultipleEpwmTbClk(epwmsMask, TRUE);      //Enabling tbclk sync for EPWM 0,1 after confg

    //get comp values for debug output:
    compAVal0 = EPWM_getCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_A);
    compAVal1 = EPWM_getCounterCompareValue(gEpwm1Base, EPWM_COUNTER_COMPARE_A);
    compBVal0 = EPWM_getCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_B);
    compBVal1 = EPWM_getCounterCompareValue(gEpwm1Base, EPWM_COUNTER_COMPARE_B);
    compCVal0 = EPWM_getCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_C);
    compDVal0 = EPWM_getCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_D);
    DebugP_log("epwms set\n");

}

/*
 * epwm_updown_close - Closes the EPWM0 and EPWM1 drivers, shows debug info
 */
void epwm_updown_close(void)
{
    DebugP_log("epwm0: CMPA: %i, CMPB: %i \r\n", compAVal0, compBVal0); //kontrolní vypis
    DebugP_log("       CMPC: %i, CMPD: %i \r\n", compCVal0, compDVal0); //kontrolní vypis

    DebugP_log("epwm1: CMPA: %i, CMPB: %i \r\n", compAVal1, compBVal1);

    DebugP_log("EPWM Action Qualifier Module Test Passed!!\r\n");
    DebugP_log("All epwm Tests have Passed!!\n");

}

/* 
 * App_epwmIntrISR_0 - ISR for EPWM0, reads ADC1 to buffer, clears interrup
 */
static void App_epwmIntrISR_0(void *handle)
{
    uint32_t raw = ADC_readResult(gAdcResultBaseAddr, ADC_SOC_NUMBER0);
    //původně: chyba: měl by být left aligned, ale dělá se automatický postprocessing??? gVals[i] = (float)(raw>>4) * VREF / 4095.0;  
    gVals[i] = (float)raw * VREF / 4095.0;   //dej to bufferu a přepočitej

    if (i<(BUFF_LEN-1)) i++;
    else i=0;
    EPWM_clearEventTriggerInterruptFlag(gEpwm0Base);     // Clear any pending interrupts if any
}

/* 
 * App_epwmIntrISR_1 - ISR for EPWM1, clears ET interrupt
 */
static void App_epwmIntrISR_1(void *handle)
{
    EPWM_clearEventTriggerInterruptFlag(gEpwm1Base);     // Clear any pending interrupts if any
}

/*
 * get_buffval - returns the filtered adc value
 */
float get_buffval(void)
{
    float sum=0;
    for (uint32_t j=0; j<BUFF_LEN; j++) sum+=gVals[j];
    return (sum/BUFF_LEN);
}

/*
 * reset_buffval - resets the adc buffer
 */
void reset_buffval(void)
{
    i=0;
    for (uint32_t j=0; j<BUFF_LEN; j++) gVals[j]=0;
}

/*
 * adc_debug - prints the filtered adc value
 */
void adc_debug(void)
{
    DebugP_log("ADC read value filtered (SA %u len): %f\r\n", BUFF_LEN, get_buffval());
    uint32_t raw = ADC_readResult(gAdcResultBaseAddr, ADC_SOC_NUMBER0);
    DebugP_log("ADC - last raw value %u \r\n", raw);
    float nig = (float)raw * VREF / 4095.0;  
    DebugP_log("ADC - last přepočítaná value %f \r\n", nig);
    ClockP_sleep(1);

}

