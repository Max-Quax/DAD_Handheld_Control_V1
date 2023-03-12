/*
 * DAD_Packet_Handler.h
 *
 *  Created on: Mar 9, 2023
 *      Author: Max

    Meant to abstract the interfaces away.
    End goal is to have control software deal with data at a packet level.

 */

#ifndef DAD_INTERFACE_HANDLER_H_
#define DAD_INTERFACE_HANDLER_H_

// HAL Includes
#include "HAL/DAD_UART.h"
#include "HAL/DAD_microSD.h"

// Macros
#define STATUS_MASK 24
#define PACKET_TYPE_MASK 7
#define PORT_MASK 224
#define PACKET_SIZE 5

typedef enum {DISCON, CON_D, CON_ND, MSG} packetStatus;
typedef enum {TEMP, HUM, VIB, MIC, LOWBAT, ERR, STOP, START} packetType;

// Report to HMI that sensor has been disconnected
void sensorDisconnectHMI(int port, DAD_UART_Struct* UARTptr);

// Write data to HMI
void writeToHMI(uint8_t port, uint8_t data, packetType type, DAD_UART_Struct* UARTptr);

// TODO write data to microSD

// Handle an individual packet
void handlePacket(DAD_UART_Struct* RSA_UARTptr, DAD_UART_Struct* HMI_UARTptr);

// Build packet from data in UART buffer
bool constructPacket(uint8_t* packet, DAD_UART_Struct* UARTptr);


#endif /* DAD_INTERFACE_HANDLER_H_ */
