

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

#define ECAP_CLK_HZ    (200000000.0f)    // set to actual ECAP timer clock (verify in SysConfig/TRM)
#define ECAP_CLK_NS    (uint16_t)(1000000000.f/ECAP_CLK_HZ)

//proměnné:
volatile float dbgF;
volatile uint32_t dbgT;
volatile uint16_t duty_int;
volatile float T1, T2, T3, T4;
volatile uint32_t f2, f4;
uint32_t gEcapBaseAddr = CONFIG_ECAP0_BASE_ADDR;


//prototypy:
static float computeFrequency(uint64_t periodTicks);
static float computePeriodms(uint64_t periodTicks);
uint32_t ecap_poll_prd_ns(void);

//ostaní v mainu


void ecap_risefall_main(void *args)        //pro cap1 na rising a cap2 na falling by mii to  mělo hodit střídu 25%, přičemž reset je na cap1 (v cap1 je perioda. v cap2 puls)
{
    uint32_t cap1 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_1);
    uint32_t cap2 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_2);

    float duty = 100* (float)cap1 / (float)cap2;
    duty_int = (uint16_t)duty;

    // for debug only!!! — avoid costly prints in tight loop
    // DebugP_log("f=%.2f Hz, duty=%.1f%% (cap2=%u cap3=%u)\r\n", freq, duty, cap2, cap1);  
}

/*
 * nastAVENO fall rise fall rise, reset on 4, tzn cap1 - puls, cap2 - perioda, cap3 - perioda, cap4 - puls (nnebo možná místo period zbytky period)
 * počitám periody a pulsy, jejich delky
 *
 */ 
void ecap_try4_main(void *args)   
{
    uint32_t cap1 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_1);
    T1 = computePeriodms(cap1);

    uint32_t cap2 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_2);
    f2 = computeFrequency(cap2);
    T2 = computePeriodms(cap2);

    uint32_t cap3 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_3);
    T3 = computePeriodms(cap3);

    uint32_t cap4 = ECAP_getEventTimeStamp(CONFIG_ECAP0_BASE_ADDR, ECAP_EVENT_4);
    T4 = computePeriodms(cap4);
    f4 = computeFrequency(cap4);
}


volatile uint16_t cap2 = 0;
volatile uint16_t cap3 = 0;
/*
 * Init všeho, co projekt a SysConfig vytvořil, all něco mam uřž v tom mainu, tak to tu není. 
 * Pozn.: ECAP je už zkonfigurován SysConfigem – zde pouze časovač a capture engine spuštěnáí
 */
void ecap_poll_init(void)
{
    DebugP_log("ecap_poll_init \r\n");
    ECAP_reArm(gEcapBaseAddr);
}

void ecap_poll_close(void)
{
    ECAP_stopCounter(CONFIG_ECAP0_BASE_ADDR);
    DebugP_log("ecap_poll_close \r\n");
}

uint32_t ecap_poll_prd_ns(void)
{
    cap2 = ECAP_getEventTimeStamp(gEcapBaseAddr, ECAP_EVENT_2);
    cap3 = ECAP_getEventTimeStamp(gEcapBaseAddr, ECAP_EVENT_3);
    return  ((cap2+cap3)*ECAP_CLK_NS);
}

float ecap_poll_f_hz(void)
{
    return 1e9 / (float)ecap_poll_prd_ns();
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

static float computePeriodms(uint64_t periodTicks)
{
    if (periodTicks == 0ULL)
    {
        DebugP_log("hazim ti nulu z compute T");
        return 0.0f;
    }
    return (1000.f / ((float)periodTicks / ECAP_CLK_HZ));
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