
#include <stdlib.h>
#include "ti_drivers_config.h"
#include "ti_board_config.h"
#include <drivers/epwm.h>
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

#define BOARD_1

// prototypyzde 
void epwm_updown(void *args);
void epwm_updown_close(void);           //konec epwm mainu
uint16_t ecap_poll_close(void);         //konec ecapu
uint16_t ecap_poll_init(void);          //zacatek ecapu
void irc_out_go(void);                  //irc, go!
float ecap_poll_f_hz(void); 
int32_t eqep_freq(void);
void eqep_close(void);
void eqep_speed_dir_init(void *args);
void pwm_conv_gen(void);
void submissive_gen(void);
void pwm_5p_off10(bool true_for_shift); //rozjede pwmku a po 3s sync s gpio65
void pwm_5p_off10_2(bool true_for_shift); //rozjede pwmku a po 3s sync s gpio65



int main(void)
{    
    System_init();
    Board_init();
    Drivers_open();    //open drivers for console, uart, init board
    Board_driversOpen();
    DebugP_log("board inited\n");

    #if defined (BOARD_1)

        epwm_updown(NULL);
        irc_out_go();
        ecap_poll_init();
        eqep_speed_dir_init(NULL);
        pwm_conv_gen();
        submissive_gen();

        pwm_5p_off10(false);
        pwm_5p_off10_2(false);

        DebugP_log("running to infinity\n");

        int32_t f_irc = eqep_freq();
        float f = ecap_poll_f_hz();

        DebugP_log("freq:  %f \n", f);
        DebugP_log("freq_irc  %i \n", f_irc);

        while (1) ClockP_sleep(1);

        ecap_poll_close();
        epwm_updown_close();
        eqep_close();

    #else
        //na boardu 2 jenom generovat neustále pulsiky
        epwm_updown(NULL); 

    #endif

       Board_driversClose();
       Drivers_close();
       Board_deinit();
       DebugP_log("board closed\n");
       System_deinit();
       DebugP_log("sys closed\n");

    return 0;
}

/*
 * ten pinout přes bally je lepší v https://www.ti.com/lit/ug/spruj86c/spruj86c.pdf?ts=1762989043602 
 * a konektory J na DOCKU v https://www.ti.com/lit/ug/spruj73/spruj73.pdf?ts=1763451183515&ref_url=https%253A%252F%252Fwww.ti.com%252Ftool%252FTMDSHSECDOCK-AM263¨
 *
 * Celej xample map:
 *
 * EPWM 0A/0B -> B2 / B1 -> HSEC 49 / 51 -> J20_1 / J20_2 (generuje updown pwm 50khz)
 * - GPIO 43,  GPIO 44
 * - INT XBAR0 (isr nic nedělá vlastně)
 * EPWM 1A/1B -> D3 / D2 -> HSEC 53 / 55 -> J20_3 / J20_4 (generuje předsazenou pwm k 0)
 * - GPIO 45, GPIO 46
 * - INT XBAR1 (isr nic nedělá vlastně)
 * ECAP - čtu epwm0 jako že to je převodník u / f, bez přerušení:
 * - epwm0 ball B1 -> gpio 43 -> epwmtoecap_INPUT_XBAR0 -> Capture input is InputXBar Output 0
 * "IRC": EPWM 2A/2B -> C2 / C1 -> HSEC 50 / 52 -> J21_1 / J21_2 (generuju 50/50 bez přerušení 200kHz, b je posunuto o 90°)
 * - ePWM2A -> GPIO47 -> INPUTXBAR1 -> PWMXBAR1 
 * - ePWM2B -> GPIO48 -> INPUTXBAR2 -> PWMXBAR2 
 * EQEP: EQEPxA Pin(EQEP0_A) B14 a EQEPxB Pin(EQEP0_B) A14 
 * - QEPA Source: Signal comes from PWM Xbar out 1, QEPB Source: Signal comes from PWM Xbar out 2
 * - INT_XBAR_2 (EQEP0_INT - isr get dir, get pos)
 * EPWM 3A/3B -> E3 / E2 -> HSEC 54 / 56 -> J21_3 / J21_4 (generuje up, komplemtární a/b uměle, moduluje sin)
 * - INT_XBAR_3 (přerušení na isr pwmky, modulace)
 * EPWM 4A/4B -> D1 / E4 -> HSEC 57 / 59 -> J21_5 / J21_6 (generuje up, komplemtární a/b uměle, moduluje cos, po 3s se synchronizuje s epwm4)
 * - INT_XBAR_4 (přerušení na isr pwmky, modulace)
 * EPWM 5A/5B -> F2 / G2 -> HSEC 61 / 63 -> J21_7 / J21_8 (generuje 5% pulsy 50kHz)
 * GPIO65 -> H1 -> HSEC 86 -> J21_18 (input pro signal 50kHz)
 * - CONFIG_INPUT_XBAR4 GPIO65 INPUT_XBAR_4
 */
