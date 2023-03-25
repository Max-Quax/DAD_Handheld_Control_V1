/*
 * DAD_FSM.c
 *
 *  Created on: Mar 9, 2023
 *      Author: Max Engel
 */

#include <DAD_FSM.h>

void DAD_FSM_control(FSMstate *state, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct){
    // FSM
    #ifdef WHAT_CAUSES_FSM_CHANGE
    bool hasFinished;
    int numInBuffer;
    #endif

    switch (*state){

    case STARTUP:
        DAD_initInterfaces(interfaceStruct, utilsStruct);   // Initialize hardware interfaces necessary for FSM use
        *state = RSA_READ;
        DAD_Timer_Start(FSM_TIMER_HANDLE);                  // Start timer
        break;

    case RSA_READ:

        /*

        This state should only put packets in write buffer.
        Write buffer is currently just the UART's ring buffer.
        Packets in write buffer shall be written in HANDLE_PERIPH state

        */

        // When timer expires, start writing to HMI and microSD
        // or
        // Wait for buffer to fill
        if((DAD_Timer_Has_Finished(FSM_TIMER_HANDLE) && DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct) >= MIN_PACKETS_TO_PROCESS)
                || RSA_BUFFER_SIZE ==  DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct)){
            #ifdef WHAT_CAUSES_FSM_CHANGE
            numInBuffer = DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct);
            hasFinished = DAD_Timer_Has_Finished(FSM_TIMER_HANDLE);
            #endif
            *state = HANDLE_PERIPH;
        }

        break;
    case HANDLE_PERIPH:
        // Disable RSA UART interrupts, ignore all UART input until buffer empty
        DAD_UART_DisableInt(&interfaceStruct->RSA_UART_struct);
        DAD_UART_DisableInt(&interfaceStruct->HMI_UART_struct);
        DAD_Timer_Stop(FSM_TIMER_HANDLE, &interfaceStruct->FSMtimerConfig);

        // Handle all data in the RSA rx buffer
        handleRSABuffer(interfaceStruct, utilsStruct);

        // Using commands from HMI, talk back to RSA
        DAD_UART_EnableInt(&interfaceStruct->HMI_UART_struct);
            // TODO get command
            // TODO send command

        // Finished writing to HMI/microSD, start listening again
        *state = RSA_READ;
        DAD_UART_EnableInt(&interfaceStruct->RSA_UART_struct);

        // Restart timer
        DAD_Timer_Start(FSM_TIMER_HANDLE);
    break;

    }

    // TODO state for reading commands from HMI
    // TODO talk to RSA?

}
