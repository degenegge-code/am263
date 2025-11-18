
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
    Board_init();

    epwm_updown(NULL);
    ecap_poll_init();

    //nekonečno použiju z ecapu:
    while (1) 
    {
        ecap_poll_main(NULL);
    }

    epwm_updown_close();
    ecap_poll_close();

    Board_deinit();
    System_deinit();

    return 0;
}
