
#include <stdlib.h>
#include "ti_drivers_config.h"
#include "ti_board_config.h"

// prototypyzde (?)
void epwm_updown_main(void *args);
void epwm_main(void *args);

int main(void)
{
    System_init();
    Board_init();

    epwm_main(NULL);

    Board_deinit();
    System_deinit();

    return 0;
}
