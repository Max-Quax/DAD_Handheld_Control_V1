/*
 * DAD_Interface_Handler.c
 *
 *  Created on: Mar 9, 2023
 *      Authors: Will Katz, Max Engel
 */

#include "DAD_Interface_Handler.h"
#include <string.h>

// Initializes UART, timers necessary
void initInterfaces(DAD_Interface_Struct* interfaceStruct){
    // RSA Interface init
    DAD_UART_Set_Config(RSA_BAUD, EUSCI_A0_BASE, &(interfaceStruct->RSA_UART_struct));
    DAD_UART_Init(&interfaceStruct->RSA_UART_struct, RSA_BUFFER_SIZE);

    // HMI Interface init
        // TODO test that this works
    DAD_UART_Set_Config(HMI_BAUD, EUSCI_A2_BASE, &interfaceStruct->HMI_UART_struct);
    DAD_UART_Init(&interfaceStruct->HMI_UART_struct, HMI_BUFFER_SIZE);

    // microSD Interface init, open log file
    interfaceStruct->currentPort = STOP;                        // Describes what file is currently being written to
    strcpy(interfaceStruct->fileName, "log.txt");
    DAD_microSD_InitUART(&interfaceStruct->microSD_UART);
    DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);
//    char fileName[] = "msptest.csv"; // debug
//    char *message[] = {"header one", "header two"};
//    DAD_microSD_Write_CSV(fileName, message, 2, &microSD_UART);

    // Wakeup Timer Init
    DAD_Timer_Initialize_ms(FSM_TIMER_PERIOD, FSM_TIMER_HANDLE, &(interfaceStruct->FSMtimerConfig));
}

void writeToPeriphs(DAD_Interface_Struct* interfaceStruct){
    // Read individual packets from buffer
    while(PACKET_SIZE <= DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct)){
        handlePacket(interfaceStruct);

        // TODO handle vib/sound
    }

    // Flush out remaining chars from buffer
    while(DAD_UART_HasChar(&interfaceStruct->RSA_UART_struct)){
        DAD_UART_GetChar(&interfaceStruct->RSA_UART_struct);
    }
}

static void *intToString(int num) {
    char str[PACKET_SIZE+1];
    sprintf(str, "%d", num);
    return str;
}


static void handlePacket(DAD_Interface_Struct* interfaceStruct)
{
    // Construct packet
    uint8_t packet[PACKET_SIZE+1];
    constructPacket(packet, &interfaceStruct->RSA_UART_struct);

    // Interpret packet
    packetStatus PKstatus = (packetStatus)((packet[0] & STATUS_MASK) >> 3);
    packetType type = (packetType)(packet[0] & PACKET_TYPE_MASK);
    int port = (packet[0] & PORT_MASK) >> 5;
    int data;

    // Deal with packet
    switch(PKstatus)
    {
        case DISCON:
            sensorDisconnectHMI(port, &interfaceStruct->HMI_UART_struct);
            break;
        case CON_D:
            switch(type)
            {
                case TEMP:
                    data = (packet[1] << 8) + packet[2];
                    writeToHMI(port, data, type, &interfaceStruct->HMI_UART_struct);
                    writeToMicroSD(port, data, type, PKstatus, interfaceStruct);
                    break;
                case HUM:
                    data = (packet[1] << 8) + packet[2];
                    writeToHMI(port, data%100, type, &interfaceStruct->HMI_UART_struct);
                    writeToMicroSD(port, data%100, type, PKstatus, interfaceStruct);
                    break;
                case VIB:
                    break;
                case MIC:
                    break;
            }
            break;
        case CON_ND:
            //DAD_microSD_Write("CON_ND\n", &interfaceStruct->microSD_UART);  // TODO
            break;
        case MSG:
            //DAD_microSD_Write("MSG\n", &interfaceStruct->microSD_UART);             // TODO
            break;
        default:
            DAD_microSD_Write("bad packet\n", &interfaceStruct->microSD_UART);
    }
}

// Constructs packet from data in UART HAL's ring buffer
    // Buffer was originally constructed as a FIFO ring buffer.
static bool constructPacket(uint8_t* packet, DAD_UART_Struct* UARTptr){
    // Remove any "end packet" characters
    char c = 255;
    while(c == 255 && DAD_UART_HasChar(UARTptr)){
        c = DAD_UART_GetChar(UARTptr);                      // Get char at back of buffer
    }

    // Construct Packet
    if(DAD_UART_NumCharsInBuffer(UARTptr) >= PACKET_SIZE){   // Check number of chars in buffer
        int i;
        for(i = 0; i < PACKET_SIZE; i++){
            packet[i] = c;
            c = DAD_UART_GetChar(UARTptr);
        }
        return true;
    }
    return false;
}

static void writeToHMI(uint8_t port, uint8_t data, packetType type, DAD_UART_Struct* UARTptr)
{
    DAD_UART_Write_Str(UARTptr, "HOME.s");
    DAD_UART_Write_Char(UARTptr, port+49);
    DAD_UART_Write_Str(UARTptr, "Val.txt=\"");

    char *val = intToString(data);
    int i = 0;
    while (val[i] != '\0') {
        DAD_UART_Write_Char(UARTptr, val[i]);
        i++;
    }

    if(type == TEMP)
        DAD_UART_Write_Char(UARTptr, 'F');
    else if(type == HUM)
        DAD_UART_Write_Char(UARTptr, '%');

    DAD_UART_Write_Char(UARTptr, '\"');

    // End of transmission
    DAD_UART_Write_Char(UARTptr, 255);
    DAD_UART_Write_Char(UARTptr, 255);
    DAD_UART_Write_Char(UARTptr, 255);
    return;
}

static void writeToMicroSD(uint8_t port, uint8_t data, packetType type, packetStatus status, DAD_Interface_Struct* interfaceStruct){
    if(status == CON_D){
        // Check that we are writing to the right file
        if(interfaceStruct->currentPort != port ){           // Compare data type to the file that is currently being written to

            // Set port
            sprintf(interfaceStruct->fileName, "port%d.csv", port+1);
            interfaceStruct->currentPort = port;

            // Open the correct file
            DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);
        }
    }
    else if(strcmp(interfaceStruct->fileName, "log.txt") != 0){
        // Open the correct file
        sprintf(interfaceStruct->fileName, "log.txt");
        DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);

        // Set port
        interfaceStruct->currentPort = 255;
    }

    // Construct message
    char message[MESSAGE_LEN];
    //sprintf(message, "%d,%d", (int)packetType, data);
    sprintf(message, "%d,%d\n", data, data);   // debug

    // Write data to microSD
    DAD_microSD_Write(message, &interfaceStruct->microSD_UART);
}

static void sensorDisconnectHMI(int port, DAD_UART_Struct* UARTptr)
{
    // Report sensor disconnected to HMI
    DAD_UART_Write_Str(UARTptr, "HOME.s");
    DAD_UART_Write_Char(UARTptr, port+49);
    DAD_UART_Write_Str(UARTptr, "Val.txt=\"NONE\"");

    // End of transmission
    DAD_UART_Write_Char(UARTptr, 255);
    DAD_UART_Write_Char(UARTptr, 255);
    DAD_UART_Write_Char(UARTptr, 255);
    return;
}
