/*
 * ECAP Capture Mode
 * ----------------
 *    ECAP runs at 200 MHz and timestamps the edges (rising and/or falling) into 4 Capture registers
 * This example reads a PWM wave and determines its frequency and duty cycle.
 * Configuring the input via GPI and InputXbar into the ECAP for
 *     Falling, Rising, Falling, Rising
 * INPUT :
 *                 _________           _________           _________
 *                 |       |           |       |           |
 *         ________|       |___________|       |___________|
 *                         ^           ^       ^           ^
 * EDGE EVENT :            1.F         2.R     3.F         4.R
 * DATA :          <--NA--><-OFF time-><--ON--><-OFF time->
 * ECAP will be configured to reset counter on each edge recieved, and capture for one shot.
 * So the timestamp registers hold the data as follows
 *     2. OFF time
 *     3. ON time
 *     4. OFF time
 *
 *
*/

#include <stdint.h>
#include <stdio.h>
#include "ti_drivers_config.h"
#include <drivers/ecap.h>
#include "drivers/hw_include/hw_types.h"
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/HwiP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

#define ECAP_CLK_HZ    (100000000.0f)    // SYSCLK ECAP clk – lze též načíst z Clock_getCpuFreq().  100 MHz typicky u AM263x (?)

//prototypy:
static float computeFrequency(uint32_t periodTicks);
//ostaní v mainu



void ecap_poll_main(void *args)    //hlavnísymčka – nulová obsluha CPU 
{
       // uint32_t cap1 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_1);
        uint32_t cap2 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_2);

        uint32_t periodTicks = cap2;     //prd = rozdíl timestamps (SysConfig musí mít reset-on-event2 vyp/zap dle potřeby) -většinou reset-on-event2 = periodTicks = CAP2 directly
        float freq = computeFrequency(periodTicks); // get my freq

        DebugP_log("f = %.2f Hz (ticks=%u)\r\n", freq, periodTicks); //rather turn off, makes moshpits

        ClockP_usleep(10000);  // 10 ms,  nneblokující pauza ?
}

void ecap_poll_init(void)
{
    // Init všeho, co projekt a SysConfig vytvořil, all něco mam uřž v tom mainu, tak to tu není. 
    //system...
    Drivers_open();

    // Pozn.: ECAP je už zkonfigurován SysConfigem – zde pouze časovač a capture engine spuštěnáí

    //ECAP_enableTimeStampCounter(CONFIG_ECAP0_BASE_ADDR);    // Start ECAP časovače
    ECAP_startCounter(CONFIG_ECAP0_BASE_ADDR);
    ECAP_enableCaptureMode(CONFIG_ECAP0_BASE_ADDR);    // enable capture jednotky
}

uint16_t ecap_poll_close(void)
{
    //formalitky
    Drivers_close();
    System_deinit();
    return 0;
}

//tohle asi líp:
static float computeFrequency(uint32_t periodTicks)
{
    if(periodTicks == 0)
        return 0.0f;
    return (ECAP_CLK_HZ / (float)periodTicks);
}