
#include <stdlib.h>
#include "ti_drivers_config.h"
#include "ti_board_config.h"

// prototypyzde 
//struktura podle examplu na jednu updown pwm
void epwm_updown(void *args);
void epwm_updown_close(void);           //konec epwm mainu
void ecap_poll_main(void *args);        //ecapture běh
uint16_t ecap_poll_close(void);         //konec ecapu
uint16_t ecap_poll_init(void);          //zacatek ecapu



int main(void)
{
    System_init();
    DebugP_log("sys inited\n");
    Board_init();
    DebugP_log("board inited\n");

    epwm_updown(NULL);
    ecap_poll_init();

    DebugP_log("running to infinity\n");

    //vezmu si jen desetisekundový okno
    for (uint16_t i=0; i<1000; i++)
    {
        ecap_poll_main(NULL);
        ClockP_usleep(10000);  // 10 ms
    }

    epwm_updown_close();
    ecap_poll_close();

    Board_deinit();
    DebugP_log("board closed\n");
    System_deinit();
    DebugP_log("sys closed\n");

    return 0;
}
