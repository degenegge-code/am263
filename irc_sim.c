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
 * - ePWM1A -> GPIO45 -> INPUTXBAR0 -> PWMXBAR0 -> eQEP0A
 * - ePWM1B -> GPIO46 -> INPUTXBAR1 -> PWMXBAR1 -> eQEP0B
 */

 /* Defines */
/* Sysclk frequency */
#define DEVICE_SYSCLK_FREQ  (200000000U)
/* PWM output at 5 kHz */
#define PWM_CLK   5000U

/* Global variables and objects */
uint32_t gEpwmBaseAddr;


void irc_out_main(void)
{
    int32_t  status;            //status
    gEpwmBaseAddr = CONFIG_EPWM0_BASE_ADDR;

}
