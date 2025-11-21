
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


int main(void)
{
    System_init();
    DebugP_log("sys inited\n"); //why do you work nigga?
    Board_init();
    Drivers_open();    //open drivers for console, uart, init board
    Board_driversOpen();

    DebugP_log("board inited\n");

    epwm_updown(NULL);

    irc_out_go();
    
    DebugP_log("running to infinity\n");

    while (1) 
    {
        ClockP_sleep(2);
    }

    epwm_updown_close();

    //DebugP_log("freq:  %f \n", f);

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