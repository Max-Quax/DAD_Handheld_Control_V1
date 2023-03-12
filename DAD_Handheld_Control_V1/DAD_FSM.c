/*
 * DAD_FSM.c
 *
 *  Created on: Mar 9, 2023
 *      Author: Max
 */

#include "DAD_FSM.h"
#include "DAD_Interface_Handler.h"

// Interface structs
static DAD_UART_Struct RSA_UART_struct;
static DAD_UART_Struct HMI_UART_struct;
static DAD_UART_Struct microSD_UART;
static Timer_A_UpModeConfig FSMtimerConfig;

void DAD_FSM_init(){
    // RSA Interface init
    DAD_UART_Set_Config(9600, EUSCI_A0_BASE, &RSA_UART_struct);
    DAD_UART_Init(&RSA_UART_struct, 1024);  // Init RSA with buffer size of 1024 bytes

    // HMI Interface init
        // TODO test that this works
    DAD_UART_Set_Config(9600, EUSCI_A2_BASE, &HMI_UART_struct);
    DAD_UART_Init(&HMI_UART_struct, 32);    // Init HMI with buffer size of 32 bytes

    // microSD Interface init
    DAD_microSD_InitUART(&microSD_UART);
    unsigned char fileName[] = "msptest.csv";
    unsigned char *message[] = {"header one", "header two"};
    DAD_microSD_Write_CSV(fileName, message, 2, &microSD_UART);

    // Wakeup Timer Start
    DAD_Timer_Initialize_ms(FSM_TIMER_PERIOD, FSM_TIMER_HANDLE, *FSMtimerConfig);
    DAD_Timer_Start(FSM_TIMER_HANDLE);
}

void DAD_FSM_control(FSMstate *state){
    unsigned char c = NULL;       // Pointer declared outside of switch bc of weird error

    // FSM
    switch (*state){
    case STARTUP:
        DAD_FSM_init();                     // Initialize interfaces, timers necessary for FSM use
        *state = RSA_READ;
        break;
    case RSA_READ:
        c = DAD_UART_GetChar(&RSA_UART_struct);
        DAD_UART_Write_Char(&microSD_UART, c);             // Debug - Write to microSD
        DAD_UART_Write_Char(&microSD_UART, ',');                    // Debug - Write to microSD

//        if(c == 255){ // If packet is ready, handle it
//            handlePacket(&RSA_UART_struct, &HMI_UART_struct);       // read packet from RSA

            /* TODO

             For now, this writes to HMI/microSD.
             In the future, this state should only put packets in write buffer.
             Packets in write buffer shall be written in WRITE_TO_PERIPH state

            */
        }

        // when timer expires, start writing to HMI and microSD
        if(DAD_Timer_Has_Finished(FSM_TIMER_HANDLE)){
            //state = WRITE_TO_PERIPH;
            // Start timer
        }
        break;
    case WRITE_TO_PERIPH:
        // TODO write data to HMI
        // TODO write data to microSD
    break;
    }
}
