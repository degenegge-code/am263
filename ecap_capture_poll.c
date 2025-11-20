/*
 * pomocí přivedení pulsů z epwm0 simuluju výstup převodníku u / f
 * pomocí ecapu vypočítám frekvenci
 */

#include <stdint.h>
#include <stdio.h>
#include <drivers/ecap.h>
#include "drivers/hw_include/hw_types.h"
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include "ti_drivers_config.h"


#define ECAP_CLK_HZ    (200000000.0f)    // set to actual ECAP timer clock (verify in SysConfig/TRM)
#define ECAP_CLK_NS    (uint16_t)(1000000000.f/ECAP_CLK_HZ)

//proměnné:

uint32_t gEcapBaseAddr = CONFIG_ECAP0_BASE_ADDR;
volatile uint16_t cap2ircirc = 0;
volatile uint16_t cap3ircirc = 0;

//prototypy:
uint32_t ecap_poll_prd_ns(void);

//ostaní v mainu

/*
 * Init všeho, co projekt a SysConfig vytvořil, all něco mam uřž v tom mainu, tak to tu není. 
 * Pozn.: ECAP je už zkonfigurován SysConfigem – zde pouze časovač a capture engine spuštěnáí
 * 
 * Syscfg:
 * all 4 events, f r f r edges, enable reset on every one, capture stops at 4, disable sync-in and interrupts. 
 * goes through xbar0.
 * 
 * epwm0 ball B1 -> gpio 43 -> CONFIG_INPUT_XBAR0 -> Capture input is InputXBar Output 0 -> WORKS
 * 
 */
void ecap_poll_init(void)
{
    DebugP_log("ecap_poll_init \r\n");
    ECAP_reArm(gEcapBaseAddr);
}

void ecap_poll_close(void)
{
    ECAP_stopCounter(gEcapBaseAddr);
    DebugP_log("ecap_poll_close \r\n");
}

uint32_t ecap_poll_prd_ns(void)
{
    cap2ircirc = ECAP_getEventTimeStamp(gEcapBaseAddr, ECAP_EVENT_2);
    cap3ircirc = ECAP_getEventTimeStamp(gEcapBaseAddr, ECAP_EVENT_3);
    return  ((cap2ircirc+cap3ircirc)*ECAP_CLK_NS);
}

float ecap_poll_f_hz(void)
{
    return 1e9 / (float)ecap_poll_prd_ns();
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