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
    #ifdef WHAT_CAUSES_FSM_CHANGE
    bool hasFinished;
    int numInBuffer;
    #endif

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
        //(DAD_Timer_Has_Finished(FSM_TIMER_HANDLE) && MIN_READ_COUNT <
        // if(RSA_BUFFER_SIZE*.75 < DAD_UART_NumCharsInBuffer(&interfaceStruct.RSA_UART_struct)){

        if((DAD_Timer_Has_Finished(FSM_TIMER_HANDLE) && DAD_UART_NumCharsInBuffer(&interfaceStruct.RSA_UART_struct) >= MIN_PACKETS_TO_PROCESS)
                || RSA_BUFFER_SIZE ==  DAD_UART_NumCharsInBuffer(&interfaceStruct.RSA_UART_struct)){
            #ifdef WHAT_CAUSES_FSM_CHANGE
            numInBuffer = DAD_UART_NumCharsInBuffer(&interfaceStruct.RSA_UART_struct);
            hasFinished = DAD_Timer_Has_Finished(FSM_TIMER_HANDLE);
            #endif
            *state = HANDLE_PERIPH;
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
    case HANDLE_PERIPH:
        // Disable RSA UART interrupts, ignore all UART input until buffer empty
        DAD_UART_DisableInt(&interfaceStruct.RSA_UART_struct);
        DAD_UART_DisableInt(&interfaceStruct.HMI_UART_struct);
        DAD_Timer_Stop(FSM_TIMER_HANDLE, &interfaceStruct.FSMtimerConfig);

        // Handle all data in the RSA rx buffer
        handleRSABuffer(&interfaceStruct);
//        char message[8] = "";
//        while(DAD_UART_NumCharsInBuffer(&interfaceStruct.RSA_UART_struct) > 0){
//            sprintf(message, "%d, ", DAD_UART_GetChar(&interfaceStruct.RSA_UART_struct));
//            DAD_microSD_Write(message, &interfaceStruct.microSD_UART);
//        }
//        DAD_microSD_Write("\n\n", &interfaceStruct.microSD_UART);

        // Finished writing to HMI/microSD, start listening again
        *state = RSA_READ;
        DAD_UART_EnableInt(&interfaceStruct.RSA_UART_struct);
        DAD_UART_EnableInt(&interfaceStruct.HMI_UART_struct);
        DAD_Timer_Start(FSM_TIMER_HANDLE);
    break;

    }

    // TODO state for reading commands from HMI
    // TODO talk to RSA?

}
