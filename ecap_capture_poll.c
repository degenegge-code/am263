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
#include <drivers/gpio.h>

#define ECAP_CLK_HZ    (25000000.0f)    // set to actual ECAP timer clock (verify in SysConfig/TRM)
//vzal jsem to nakonec z syscfg z clocku Input Clock Frequency (Hz) 

//proměnné:
volatile float dbgF;
volatile uint32_t dbgT;
volatile uint16_t duty_int;

//prototypy:
static float computeFrequency(uint64_t periodTicks);
//ostaní v mainu


void ecap_poll_main(void *args)    //hlavnísymčka – nulová obsluha CPU 
{
    uint32_t cap1 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_1);
    uint32_t cap2 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_2);


    //pro cap1 na rising a cap2 na falling by mii to  mělo hodit střídu 25%, přičemž reset je na cap1 (v cap1 je perioda. v cap2 puls)
    float duty = 100* (float)cap1 / (float)cap2;
    duty_int = (uint16_t)duty;

    //if 
    // for debug only!!! — avoid costly prints in tight loop
    // DebugP_log("f=%.2f Hz, duty=%.1f%% (cap2=%u cap3=%u)\r\n", freq, duty, cap2, cap1);

    
}


//tohle asi líp:
static float computeFrequency(uint64_t periodTicks)
{
    if (periodTicks == 0ULL)
    {
        DebugP_log("hazim ti nulu z computefreq");
        return 0.0f;
    }

    return (ECAP_CLK_HZ / (float)periodTicks);
}

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
    DebugP_log("ecap_poll_close: duty %u \n", duty_int); 
    //DebugP_log("ecap_poll_close: f = %.2f Hz (ticks=%u)\r\n", dbgF, dbgT); // vypis toho f a ticku
    //formalitky
    Drivers_close();
    System_deinit();
    DebugP_log("ecap closed\n");

    return 0;
}


/*
 * Emulation Mode se týká chování periferie (v tomto případě eCAP modulu), když je k procesoru připojen debugger (např. přes JTAG) a procesor narazí na breakpoint
 * (bod přerušení) a zastaví se vykonávání kódu.
 * TSCTR (Time-Stamp Counter) zastavuju při breakpointu
 * eCAP Capture Mode:
 * Když přijde signál, modul "zachytí" (zkopíruje) aktuální hodnotu běžícího čítače (TSCTR) do speciálního registru (CAP1, CAP2, atd.). (Time-Stamp)
 * Continuous Capture, kruh
 * Event Polarity Rising / falling Edge – capture to cap1 on rising, capture to cap2 on falling (diff is pulse width)
 * Counter Reset (Volitelné): Pokud je tak nastaveno, může tato hrana vynulovat čítač
 * Capture stops at event se použivá v one-shotu
 * "Counter Reset on eCAP event 2" je reset TSCTR (Time-Stamp Counter) – tedy resetuju ten hlavní 32-bitový čítač eCAP modulu, tzn delta time: 
 * Event 1: Získá čas a vynuluje TSCTR, event 2: TSCTR začal počítat od nuly. Až přijde Event 2, hodnota v TSCTR je rovna přesně době trvání od Event 1. Hodnota v CAP2 
 * tedy rovnou obsahuje délku impulzu (např. 500). Software nemusí nic odčítat.
 * - pro Event 2 reset: Jakmile přijde druhá hrana, uloží se její čas a čítač TSCTR se okamžitě nastaví na 0, aby byl připraven měřit čas od této hrany k hraně třetí
 * 
 * Input X-BAR (Vstupní Crossbar) slouží k tomu, aby přivedl signál z venkovního pinu (GPIO) dovnitř do periferie (eCAP, ADC start, externí přerušení)
 * Fyzický signál (náběžná hrana) přijde na pin, GPIO Mux: Pin musí být nastaven jako vstup, Input X-BAR: Zde je nastaveno: INPUT7 SELECT = GPIO 24, eCAP Modul: eCAP 
 * "poslouchá" na lince INPUT7 a když tam přijde hrana, provede Capture - tim input7 si nejsem jistej
 * V registru CAP1 zůstává uložená poslední naměřená hodnota tak dlouho, dokud ji nepřepíše nová událost Event 1.
 **/