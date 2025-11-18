
#include <stdlib.h>
#include "ti_drivers_config.h"
#include "ti_board_config.h"

// prototypyzde 
//struktura podle examplu na jednu updown pwm
void epwm_updown_main(void *args);

int main(void)
{
    System_init();
    Board_init();

    epwm_updown_main(NULL);

    Board_deinit();
    System_deinit();

    return 0;
}
