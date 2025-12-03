
#include <stdlib.h>
#include "ti_drivers_config.h"
#include "ti_board_config.h"
#include <drivers/epwm.h>
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"


// prototypyzde 
void epwm_updown(void *args);
void epwm_updown_close(void);           //konec epwm mainu
uint16_t ecap_poll_close(void);         //konec ecapu
uint16_t ecap_poll_init(void);          //zacatek ecapu
void irc_out_go(void);                  //irc, go!
float ecap_poll_f_hz(void);         //frekvence z ecapu
int32_t eqep_freq(void);               //frekvence z eqepu
void eqep_close(void);                 //konec eqepu
void eqep_speed_dir_init(void *args);   //zacatek eqepu
void pwm_conv_gen(void);               //pwm konvertoru generátor
void submissive_gen(bool true_for_osc);              //pwm konvertoru generátor
void pwm_5p_off10(bool true_for_shift, bool true_for_osc); //rozjede pwmku a po 3s sync s gpio65, když true_for_osc
void pwm_5p_off10_2(bool true_for_shift, bool true_for_osc); //rozjede pwmku a po 3s sync s gpio65, když true_for_osc
void adc_debug(void);       //prints adc values



int main(void)
{    
    System_init();                      // Initialize the system
    Board_init();                       // Initialize the board

    Drivers_open();                     //open drivers for console, uart, etc.
    Board_driversOpen();                //open board drivers, pinmux, etc.
    DebugP_log("board inited\n");

    //inits and starts:
    epwm_updown(NULL);
    irc_out_go();
    ecap_poll_init();
    pwm_conv_gen();
    submissive_gen(true);
    pwm_5p_off10(true, true);
    float f = ecap_poll_f_hz();
    DebugP_log("freq:  %f \n", f);
    pwm_5p_off10_2(true, true);
    eqep_speed_dir_init(NULL);
    int32_t f_irc = eqep_freq();
    ClockP_sleep(1);
    DebugP_log("freq_irc  %i \n", f_irc); 
    adc_debug();

    ////posledni zkoušene:
    //while (1) {
        ClockP_sleep(1);
    //}


    //DebugP_log("running to infinity\n");

    //closes:
    ecap_poll_close();
    epwm_updown_close();
    eqep_close();
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
 * EQEP: EQEP0_A / EQEP0_B -> B14 / A14 -> gpio 130 / 131 -> HSEC 102 / 100 -> J21_25 / J21_24
 * - INT_XBAR_2 (EQEP0_INT - isr get dir, get pos)
 * EPWM 3A/3B -> E3 / E2 -> HSEC 54 / 56 -> J21_3 / J21_4 (generuje up, komplemtární a/b uměle, moduluje sin)
 * - INT_XBAR_3 (přerušení na isr pwmky, modulace)
 * EPWM 4A/4B -> D1 / E4 -> HSEC 57 / 59 -> J20_5 / J20_6 (generuje up, komplemtární a/b uměle, moduluje cos)
 * - INT_XBAR_4 (přerušení na isr pwmky, modulace)
 * ePWM5A/B -> F2 / G2 -> HSEC 61 / 63 -> J20_7 / J20_8 (generuje 5% pulsy 50kHz) 
 * GPIO65 -> H1 -> HSEC 86 -> J21_18 (input pro signal PM 50kHz)
 * - CONFIG_INPUT_XBAR4 GPIO65 INPUT_XBAR_4
 * ePWM6A/B -> E1 / F3 -> HSEC 58 / 60 -> J21_5 / J21_6  (generuje 5% pulsy 50kHz)
 * ADC1 -> HSEC 12 -> ADC1_AIN0 -> J12_1 
 * - EPWM0 SOCs ADC1
 * - SW9.1 1-2 a SW9.2 4-5 je VREFLO
 */
