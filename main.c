
#include <stdlib.h>
#include "ti_drivers_config.h"
#include "ti_board_config.h"
#include <drivers/epwm.h>
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

// prototypyzde 
//struktura podle examplu na jednu updown pwm
void epwm_updown(void *args);
void epwm_updown_close(void);           //konec epwm mainu
uint16_t ecap_poll_close(void);         //konec ecapu
uint16_t ecap_poll_init(void);          //zacatek ecapu

float ecap_poll_f_hz(void);


int main(void)
{
    System_init();
    DebugP_log("sys inited\n");
    Board_init();

    //open drivers for console, uart
    Drivers_open();
    Board_driversOpen();

    DebugP_log("board inited\n");

    epwm_updown(NULL);

    DebugP_log("running to infinity\n");

    ecap_poll_init();
    ClockP_sleep(2);
    float f=ecap_poll_f_hz();
    ClockP_sleep(2);

    ecap_poll_close();

    epwm_updown_close();

    DebugP_log("freq:  %f \n", f);

    Board_driversClose();
    Drivers_close();

    Board_deinit();
    DebugP_log("board closed\n");
    System_deinit();
    DebugP_log("sys closed\n");

    return 0;
}


/*
 * mam epwemku -> vezmu, na jakym je ballu (např G1) -> zjistim, jaký gpio tomu odpovídá (např GPIO61) -> tohle gpio narvu do Input Xbar (do jeho outputu)
 * tadyten XbarInput (např input xbar 0) narvu do eCapu A TO JE VŠE, nekonfiguruju gpio nebo něco takováho

 * čím se to spravilo? težko říct, blbš jsem měl přepočty, ale po importu examplu s isr to začlo fungovat
 */