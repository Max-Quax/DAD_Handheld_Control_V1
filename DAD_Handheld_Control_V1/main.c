#include <ti/devices/msp432p4xx/driverlib/driverlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// HAL includes
#include <HAL/DAD_UART.h>

// Control software includes
#include <DAD_Utils/DAD_Interface_Handler.h>
#include <DAD_FSM.h>

    // TODO Test freq
    // TODO average intensity, moving averages
    // TODO implement transmits to RSA

    // TODO HMI talk back to MSP
    // TODO Integrate BT
    // TODO clean up code
        // unnecessary inferfaceStruct
        // functions in the wrong files

int main(void)
{
    /* Halting WDT  */
    MAP_WDT_A_holdTimer();

    // Declarations
    FSMstate state = STARTUP;
    DAD_Interface_Struct interfaceStruct;
    DAD_utils_struct utilsStruct;

    // Application loop
    while(true){
        DAD_FSM_control(&state, &interfaceStruct, &utilsStruct);    // Handle everything
        MAP_GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);        // Indicator light - check that it's not hung up
        MAP_PCM_gotoLPM0();                                         // Go back to sleep until next interrupt

    }
}
