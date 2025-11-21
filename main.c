
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
void eqep_speed_dir_main(void *args);    //eqep meri 


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

    ClockP_sleep(2);
    eqep_speed_dir_main(NULL);
    ClockP_sleep(2);

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
