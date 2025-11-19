/*
 * ECAP Capture Mode - example part
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
 * Capture input is InputXBar Output 1
 * generuju pwmku na jeden pin a beru ten samý pin jako vstup pro ecap
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

#define ECAP_CLK_HZ    (100000000.0f)    // SYSCLK ECAP clk – lze též načíst z Clock_getCpuFreq().  100 MHz typicky u AM263x (???)

//proměnné:
volatile float dbgF;
volatile uint32_t dbgT;


//prototypy:
static float computeFrequency(uint32_t periodTicks);
//ostaní v mainu



void ecap_poll_main(void *args)    //hlavnísymčka – nulová obsluha CPU 
{
        uint32_t cap2 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_2);

        uint32_t periodTicks = cap2;     //prd = rozdíl timestamps (SysConfig musí mít reset-on-event2 vyp/zap dle potřeby) -většinou reset-on-event2 = periodTicks = CAP2 directly
        float freq = computeFrequency(periodTicks); // get my freq
        // jen jako test bez blokace:
        dbgF = freq;       // uloží do RAM, žádné blokace
        dbgT = periodTicks;
        //pak si to vyzvednu na konci, no a co
}
/*  
 *  ecap_poll_main původní:
 *
 *        // uint32_t cap1 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_1); //unused
 *        uint32_t cap2 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_2);
 *        uint32_t periodTicks = cap2;     //prd =  = CAP2 directly
 *        float freq = computeFrequency(periodTicks); // get my freq
 *        DebugP_log("f = %.2f Hz (ticks=%u)\r\n", freq, periodTicks); //rather turn off, makes moshpits
 *        ClockP_usleep(10000);  
 *
 * toto moshpituje. pokaždé. když se pustí výpis na konzoli, tak to přeruší generování pulsů epwm. 
 * pokud zakomentim výpis, tak to běží normálně v pohodě, i když pořád beru (a nepouživám teda) hodnoty z ecapu.
 * spinlock, debug_log nelze použít jinde než v initu, možnost UARTu v DMA řežimu? 
 * 
 */


/*
 * Init všeho, co projekt a SysConfig vytvořil, all něco mam uřž v tom mainu, tak to tu není. 
 * Pozn.: ECAP je už zkonfigurován SysConfigem – zde pouze časovač a capture engine spuštěnáí
 */
void ecap_poll_init(void)
{
    ECAP_startCounter(CONFIG_ECAP0_BASE_ADDR);
    ECAP_enableCaptureMode(CONFIG_ECAP0_BASE_ADDR);    // enable capture jednotky

    DebugP_log("ecap inited\n");
}

uint16_t ecap_poll_close(void)
{
    DebugP_log("ecap_poll_close: f = %.2f Hz (ticks=%u)\r\n", dbgF, dbgT); // vypis toho f a ticku
    //formalitky
    Drivers_close();
    System_deinit();
    DebugP_log("ecap closed\n");

    return 0;
}

//tohle asi líp:
static float computeFrequency(uint32_t periodTicks)
{
    if(periodTicks == 0)
        return 0.0f;
    return (ECAP_CLK_HZ / (float)periodTicks);
}