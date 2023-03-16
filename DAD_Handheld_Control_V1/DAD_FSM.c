/*
 * DAD_FSM.c
 *
 *  Created on: Mar 9, 2023
 *      Author: Max Engel
 */

#include "DAD_FSM.h"
#include "DAD_Interface_Handler.h"

static DAD_Interface_Struct interfaceStruct;

void DAD_FSM_control(FSMstate *state){
    // FSM
    switch (*state){

    case STARTUP:
        initInterfaces(&interfaceStruct);       // Initialize hardware interfaces necessary for FSM use
        *state = RSA_READ;
        DAD_Timer_Start(FSM_TIMER_HANDLE);      // Start timer
        break;

    case RSA_READ:
        // TODO decide what initiates a state change
            // Wait for buffer to fill or vibe/sound packets to finish sending
            // or
            // when timer expires, start writing to HMI and microSD
        if(DAD_Timer_Has_Finished(FSM_TIMER_HANDLE) &&
                PACKET_SIZE*4 < DAD_UART_NumCharsInBuffer(&interfaceStruct.RSA_UART_struct)){
            *state = WRITE_TO_PERIPH;
        }

        // Debug block
//        c = DAD_UART_GetChar(&RSA_UART_struct);
//        DAD_UART_Write_Char(&microSD_UART, c);             // Debug - Write to microSD
//        DAD_UART_Write_Char(&microSD_UART, ',');                    // Debug - Write to microSD
        /* TODO

        This state should only put packets in write buffer.
        Write buffer is currently just the UART's ring buffer.
        Packets in write buffer shall be written in WRITE_TO_PERIPH state

        */

        break;
    case WRITE_TO_PERIPH:
        // Disable RSA UART interrupts, ignore all UART input until buffer empty
        DAD_UART_DisableInt(&(interfaceStruct.RSA_UART_struct));

        // Handle all data in the RSA rx buffer
        handleRSABuffer(&interfaceStruct);

        // Finished writing to HMI/microSD, start listening again
        *state = RSA_READ;
        DAD_UART_EnableInt(&(interfaceStruct.RSA_UART_struct));
        DAD_Timer_Start(FSM_TIMER_HANDLE);
    break;

    }

    // TODO state for reading commands from HMI
    // TODO talk to RSA?

}
