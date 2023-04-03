/*
 * DAD_FSM.c
 *
 *  Created on: Mar 9, 2023
 *      Author: Max Engel
 */

#include <DAD_FSM.h>

void DAD_FSM_control(FSMstate *state, DAD_Interface_Struct* interfaceStruct){
    // FSM
    switch (*state){

    case STARTUP:
        DAD_initInterfaces(interfaceStruct);       // Initialize hardware interfaces necessary for FSM use
        *state = RSA_READ;
        DAD_Timer_Start(FSM_TIMER_HANDLE);          // Start timer
        break;

    case RSA_READ:

        /*

        This state should only put packets in write buffer.
        Write buffer is currently just the UART's ring buffer.
        Packets in write buffer shall be written in HANDLE_PERIPH state

        */

        if(!interfaceStruct->startStop){
            *state = STOP_STATE;
        }

        // When timer expires, start writing to HMI and microSD
        // or
        // Wait for buffer to fill
        else if((DAD_Timer_Has_Finished(FSM_TIMER_HANDLE) && DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct) >= MIN_PACKETS_TO_PROCESS)
                || RSA_BUFFER_SIZE ==  DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct)){
            *state = HANDLE_PERIPH;
        }
        break;
    case HANDLE_PERIPH:
        // Disable RSA UART rx interrupts, ignore all UART input until buffer empty
        DAD_UART_DisableInt(&interfaceStruct->RSA_UART_struct);
        DAD_UART_DisableInt(&interfaceStruct->HMI_RX_UART_struct);
        DAD_Timer_Stop(FSM_TIMER_HANDLE, &interfaceStruct->FSMtimerConfig);

        // Handle all data in the RSA rx buffer
        handleRSABuffer(interfaceStruct);

        // Finished writing to HMI/microSD, start listening again
        *state = RSA_READ;
        DAD_UART_EnableInt(&interfaceStruct->RSA_UART_struct);
        DAD_UART_EnableInt(&interfaceStruct->HMI_RX_UART_struct);

        // Restart timer
        DAD_Timer_Start(FSM_TIMER_HANDLE);
        break;
    case STOP_STATE:
        handleStop(interfaceStruct);
        if(interfaceStruct->startStop){
           *state = RSA_READ;
        }
    }
}
