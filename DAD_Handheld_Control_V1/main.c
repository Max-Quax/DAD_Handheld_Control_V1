#include <ti/devices/msp432p4xx/driverlib/driverlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// HAL includes
#include <HAL/DAD_UART.h>

// Control software includes
#include "DAD_Interface_Handler.h"
#include "DAD_FSM.h"

    // TODO Test freq
    // TODO average intensity, moving averages
    // TODO implement transmits to RSA

int main(void)
{
    /* Halting WDT  */
    MAP_WDT_A_holdTimer();

    // Application loop
    FSMstate state = STARTUP;
    while(true){
        DAD_FSM_control(&state);                                // Handle everything
        MAP_GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);    // Debug - check that it's not hung up
        MAP_PCM_gotoLPM0();                                     // Go back to sleep until next interrupt

    }
}
