/*
 * DAD_FSM.c
 *
 *  Created on: Mar 9, 2023
 *      Author: Max
 */

#include "DAD_FSM.h"

// Interface structs
static DAD_UART_Struct RSA_UART_struct;
static DAD_UART_Struct HMI_UART_struct;

void DAD_FSM_init(){
    // RSA Interface init
    DAD_UART_Set_Config(9600, EUSCI_A0_BASE, &RSA_UART_struct);
    DAD_UART_Init(&RSA_UART_struct, 1024);  // Init RSA with buffer size of 1024 bytes

    // HMI Interface init
    DAD_UART_Set_Config(9600, EUSCI_A2_BASE, &HMI_UART_struct);
    DAD_UART_Init(&HMI_UART_struct, 32);    // Init HMI with buffer size of 32 bytes

    // TODO microSD interface

    // TODO wakeup timers?
    // TODO time polling for real time data
}

void DAD_FSM_control(FSMstate *state){
    char* peekedChar;           // Pointer declared outside of switch bc of weird error

    switch (*state){
    case STARTUP:
        DAD_FSM_init();         // Initialize interfaces, timers necessary for FSM use
        *state = RSA_READ;
        break;
    case RSA_READ:
        DAD_UART_Peek(&RSA_UART_struct, peekedChar);
        if(*peekedChar == 255){ // If packet is ready, handle it
            handlePacket();     // read packet from RSA
            /* TODO

             For now, this writes to HMI.
             In the future, this state should only put packets in write buffer.
             Packets in write buffer shall be written in WRITE_TO_PERIPH state

            */
        }
        break;
    case WRITE_TO_PERIPH:
        // TODO write data to HMI
        // TODO write data to microSD
    break;
    }
}
