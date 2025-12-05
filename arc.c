
#include <drivers/epwm.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/HwiP.h>


/*
žádné CPU cykly pro čtení SSI enkodéru — EPWM generuje clock, SPI v módu slave nasbírá 16bitové slovo, DMA (UDMA) to přehodí do paměti
Senzor je RS-422 (differenciální). AM263 má jen 3.3 V CMOS, takže mezi nimi musí být převodníky:
    Driver (MCU -> sensor clock): SN65LBC179 / AM26LS31 / jiný RS-422 driver
    Receiver (sensor -> MCU data): SN65HVD3x / SN65LBC178 / jiný RS-422 receiver
EPWM -> PWMXBAR -> GPIO_OUT (tento pin bude EPWM output; z něj jde signál přes RS-422 driver do senzoru a drátem i na SPI IN zpět dovnitř
Data+ / Data− z senzoru → RS-422 receiver → připojit na pin, který je pinmuxem přiřazen jako SPIx_SOMI / SPI_MISO.
SPI musí být v slave režimu
prostě
Diferenciální enkodér SSI -> Line Receiver (RS-422) -> Unipolární GPIO piny AM263Px -> Software/SPI pro implementaci protokolu.
*/


// Defines 
// Sysclk frequency 
#define DEVICE_SYSCLK_FREQ  (200000000U)

//PWM output at 200 kHz as a clock for SSI
#define PWM_CLK_SSI   (200000u)
#define PWM_PRD_SSI   (DEVICE_SYSCLK_FREQ / PWM_CLK_SSI/2)  //no prsc and up-down 

// Global variables and objects 
uint32_t gEpwmBaseAddrCLK = CONFIG_EPWM8_BASE_ADDR;

//Funkce:

/*
 * clk_out_go - differential clock
                EPWM 8A/8B -> G3 / H2 -> HSEC ?? / ?? -> J???? / J???? (hodiny 200kHz)
 */
 
void clk_out_go(void)
{
    DebugP_log("clk_out_go \n");

    EPWM_setClockPrescaler(gEpwmBaseAddrCLK, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);  //no prescalers
    EPWM_setTimeBasePeriod(gEpwmBaseAddrCLK, PWM_PRD_SSI);                     // set period, up mode
    EPWM_setTimeBaseCounterMode(gEpwmBaseAddrCLK, EPWM_COUNTER_MODE_UP_DOWN); 
    EPWM_setTimeBaseCounter(gEpwmBaseAddrCLK, 0);                          //set counter to 0, init

    EPWM_setActionQualifierAction(gEpwmBaseAddrCLK, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(gEpwmBaseAddrCLK, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);
    EPWM_setActionQualifierAction(gEpwmBaseAddrCLK, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(gEpwmBaseAddrCLK, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);

    EPWM_clearEventTriggerInterruptFlag(gEpwmBaseAddrCLK);            //Clear any pending interrupts if any

    DebugP_log("clk runs at %u \n", PWM_CLK_SSI);
}


