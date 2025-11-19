
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

//hypoteza s tim, že mam moc rychlej polling je asi mylná, nefunguje to stejně i s 100ms ibez smyčky atd

int main(void)
{
    System_init();
    DebugP_log("sys inited\n");
    Board_init();
    DebugP_log("board inited\n");

    epwm_updown(NULL);
    ecap_poll_init();

    DebugP_log("running to infinity\n");

    //vezmu si jen cca jedenáctisekundový okno
    for (uint16_t i=0; i<100; i++)
    {
        ecap_poll_main(NULL);
        ClockP_usleep(111111);  // 111 ms
    }

    epwm_updown_close();
    ecap_poll_close();

    Board_deinit();
    DebugP_log("board closed\n");
    System_deinit();
    DebugP_log("sys closed\n");

    return 0;
}
