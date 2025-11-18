/*
 *  Copyright (C) 2024 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/HwiP.h>
#include <drivers/epwm.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

/*
 * Example Description
 * This example configures EPWM0 EPWM0A and EPWM0B.
 *
 * The TB counter is in up-down count mode for this example.
 *
 * During the test, monitor EPWM0 outputs on an oscilloscope.
 *
 * On AM263x CC/ AM263Px CC with HSEC Dock,
 * Probe the following on the HSEC pins
 *  - EPWM 0A/0B : 49 / 51
 *  - EPWM 1A/1B : 53 / 55
 * ePWM0A se sepne za ZERO a vypne při CMPA - tzn při čítání NAHORU, dolu je no action
 * ePWM0B se zapne na TBPRD a vypne na CMPB - tzn při čítání DOLU, nahoru no asction
 * to samé jsem chtěl udělat skrz C a D, ale:
 *
 *          COMPC A COMPD PODPORUJÍ POUZE EVENT TRIGGER!
 */

#define EPWM_FREQ 50000u    //50khz - to odpovídá i pile

//H bridge:
// prd = F_ 1/f /2 /prsc , 400Mhz / 50kHz /2 /2 = 2000
#define EPWM0_TIMER_TBPRD   1000// Period register, prd je vrcholová hodnota - tak proč to kurva chce v syscfgu 1000? 
#define EPWM0_WIDTH     EPWM0_TIMER_TBPRD/2 //čtvrztin periody je puls


//USM:
#define OFFSET_TICS     EPWM0_TIMER_TBPRD/10 // synchro s hbr ale o 5%T posunuto PŘED něj


//proměnné
volatile uint16_t compAVal0, compBVal0;   //CMPA CMPB
static HwiP_Object  gEpwmHwiObject_0; //objekt h bridge a usm
uint32_t gEpwm0Base = CONFIG_EPWM0_BASE_ADDR; //base adresu epwm H bridge si vezmi z konfigu

//prototypy:
static void App_epwmIntrISR_0(void *handle);

void epwm_main(void *args)
{
    int32_t  status;        //status
    HwiP_Params  hwiPrms_0; //initialize interrupt parameters

    //open drivers for console
    Drivers_open();
    Board_driversOpen();

    uint32_t epwmsMask = (1U << 0U) ;

    // Check the syscfg for configurations !!!

    DebugP_log("EPWMTest ABCD Started ...\r\n");
    DebugP_log("EPWM Action Qualifier Module using tadytenbordel\r\n");

    SOC_setMultipleEpwmTbClk(epwmsMask, FALSE);     // Disabling tbclk sync for EPWM 0 for configuration

    // EPWM0 register and enable interrupt:
    HwiP_Params_init(&hwiPrms_0);
    hwiPrms_0.intNum      = CSLR_R5FSS0_CORE0_CONTROLSS_INTRXBAR0_OUT_0;
    hwiPrms_0.isPulse     = APP_INT_IS_PULSE;
    hwiPrms_0.callback    = &App_epwmIntrISR_0;
    status              =   HwiP_construct(&gEpwmHwiObject_0, &hwiPrms_0);
    DebugP_assert(status == SystemP_SUCCESS);

    //set comp values HB
    EPWM_setCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_A, EPWM0_WIDTH);
    EPWM_setCounterCompareValue(gEpwm0Base, EPWM_COUNTER_COMPARE_B, EPWM0_TIMER_TBPRD - EPWM0_WIDTH);

    EPWM_clearEventTriggerInterruptFlag(gEpwm0Base);    //Clear any pending interrupts if any

    SOC_setMultipleEpwmTbClk(epwmsMask, TRUE); // Enabling tbclk sync for EPWM 0 after configurations, i pro epwm1

    //DebugP_log("epwm0: CMPA: %i, CMPB: %i \r\n", compAVal0, compBVal0);

    while(1)
    {
        ClockP_sleep(10);  //wait 100s a pracuj přes interrupty
    }

    DebugP_log("EPWM Action Qualifier Module Test Passed!!\r\n");
    DebugP_log("All Tests have Passed!!");

    Board_driversClose();
    Drivers_close();
}


//interrupt service rutina - vezmi cmpam hodnoty, narvi je do vypisu, vyčisti přerušení
static void App_epwmIntrISR_0(void *handle)
{
    EPWM_clearEventTriggerInterruptFlag(gEpwm0Base);     // Clear any pending interrupts if any
}