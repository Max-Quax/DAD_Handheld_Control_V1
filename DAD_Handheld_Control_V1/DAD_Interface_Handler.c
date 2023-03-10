/*
 * DAD_Interface_Handler.c
 *
 *  Created on: Mar 9, 2023
 *      Author: Max
 */

#include "DAD_Interface_Handler.h"

static void *intToString(int num) {
    char str[PACKET_SIZE];
    sprintf(str, "%d", num);
    return str;
}

void handlePacket(DAD_UART_Struct* UARTptr)
{
    // Construct packet
    uint8_t packet[PACKET_SIZE];
    constructPacket(packet, UARTptr);

    // Interpret packet
    packetStatus PKstatus = (packetStatus)((packet[0] & STATUS_MASK) >> 3);
    packetType type = (packetType)(packet[0] & PACKET_TYPE_MASK);
    int port = (packet[0] & PACKET_SIZE) >> 5;
    int data;

    // Deal with packet
    switch(PKstatus)
    {
        case DISCON:
            sensorDisconnectHMI(port, UARTptr);
            break;
        case CON_D:
            switch(type)
            {
                case TEMP:
                    data = (packet[1] << 8) + packet[2];
                    writeToHMI(port, data%100, type, UARTptr);
                    break;
                case HUM:
                    data = (packet[1] << 8) + packet[2];
                    writeToHMI(port, data%100, type, UARTptr);
                    break;
                case VIB:
                    break;
                case MIC:
                    break;
            }
            break;
        case CON_ND:
            break;
        case MSG:
            break;
    }
}


// Constructs packet from data in UART HAL's ring buffer
    // Buffer was originally constructed as a FIFO ring buffer.
bool constructPacket(uint8_t* packet, DAD_UART_Struct* UARTptr){
    // Remove any "end packet" characters
    char c = 255;
    while(c == 255 && DAD_UART_HasChar(UARTptr)){
        c = DAD_UART_GetChar(UARTptr);                      // Get char at back of buffer
    }

    // Construct Packet
    if(DAD_UART_NumCharsInBuffer(UARTptr) > PACKET_SIZE){   // Check number of chars in buffer
        int i;
        for(i = 0; i < PACKET_SIZE; i++){
            packet[i] = c;
            c = DAD_UART_GetChar(UARTptr);
        }
        return true;
    }
    return false;
}

void writeToHMI(uint8_t port, uint8_t data, packetType type, DAD_UART_Struct* UARTptr)
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

void sensorDisconnectHMI(int port, DAD_UART_Struct* UARTptr)
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
