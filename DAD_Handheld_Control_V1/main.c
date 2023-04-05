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

// Requirements
// TODO get UART to MSP working
        // Time
        // configs
    // TODO combination sensor
    // TODO condition data
    // TODO transmit time

// Non-requirement priority
    // TODO timestamp
    // TODO raise the alarm when sensor hasn't said anything in a while
    // TODO make page-specific FFT faster

// Quality of life
    // TODO ensure singleton
    // TODO low power shutdown
    // TODO hot glue


//Test

// Issues with HMI
    // Sometimes starts up in stop mode
    // When switching to different FFT, keeps data from old FFT

int main(void)
{
    /* Halting WDT  */
    MAP_WDT_A_holdTimer();

    // Declare hardware interface struct
    DAD_Interface_Struct interfaceStruct;

    // Application loop
    FSMstate state = STARTUP;
    while(true){
        DAD_FSM_control(&state, &interfaceStruct);                                // Handle everything
        MAP_GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);    // Debug - check that it's not hung up
        MAP_PCM_gotoLPM0();                                     // Go back to sleep until next interrupt

    }
}
