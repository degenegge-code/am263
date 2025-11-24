
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
float ecap_poll_f_hz(void); 
int32_t eqep_freq(void);
void eqep_close(void);
void eqep_speed_dir_init(void *args);


int main(void)
{
    System_init();
    Board_init();
    Drivers_open();    //open drivers for console, uart, init board
    Board_driversOpen();

    DebugP_log("board inited\n");

    epwm_updown(NULL);
    irc_out_go();
    ecap_poll_init();
    eqep_speed_dir_init(NULL);

    DebugP_log("running to infinity\n");

    ClockP_sleep(1);
    int32_t f_irc = eqep_freq();
    ClockP_sleep(1);
    float f = ecap_poll_f_hz();
    ClockP_sleep(1);



    DebugP_log("freq:  %f \n", f);
    DebugP_log("freq_irc  %i \n", f_irc);

    while (1) ClockP_sleep(1);
    
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
