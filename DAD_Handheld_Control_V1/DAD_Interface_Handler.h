/*
 * DAD_Packet_Handler.h
 *
 *  Created on: Mar 9, 2023
 *      Author: Will Katz, Max Engel

    Meant to abstract the interfaces away.
    End goal is to have control software deal with data at a packet level.

 */

#ifndef DAD_INTERFACE_HANDLER_H_
#define DAD_INTERFACE_HANDLER_H_

// Standard includes
#include <string.h>
#include <stdio.h>

// HAL Includes
#include "HAL/DAD_UART.h"
#include "HAL/DAD_microSD.h"

// UART Macros
#define RSA_BAUD 9600
#define RSA_BUFFER_SIZE 1024
#define HMI_BAUD 9600
#define HMI_BUFFER_SIZE 1024

// Timer Macros
#define FSM_TIMER_HANDLE TIMER_A0_BASE
#define FSM_TIMER_PERIOD 500            // Period in ms

// Packet Macros
#define STATUS_MASK 24
#define PACKET_TYPE_MASK 7
#define PORT_MASK 224
#define PACKET_SIZE 5
#define MESSAGE_LEN (sizeof(char)*(PACKET_SIZE - 1) + 1)

typedef enum {DISCON, CON_D, CON_ND, MSG} packetStatus;
typedef enum {TEMP, HUM, VIB, MIC, LOWBAT, ERR, STOP, START} packetType;

typedef struct DAD_Interface_Struct_{
    DAD_UART_Struct RSA_UART_struct;
    DAD_UART_Struct HMI_UART_struct;
    DAD_UART_Struct microSD_UART;
    Timer_A_UpModeConfig FSMtimerConfig;
    uint8_t currentPort;  // Describes what file is currently being written to
    char fileName[13];
} DAD_Interface_Struct;

// Initializes UART, timers necessary
void initInterfaces(DAD_Interface_Struct* interfaceStruct);

// Write all elements from RSA UART buffer to peripherals
void writeToPeriphs(DAD_Interface_Struct* interfaceStruct);

// Build packet from data in UART buffer
static bool constructPacket(uint8_t* packet, DAD_UART_Struct* UARTptr);

// Handle an individual packet
static void handlePacket(DAD_Interface_Struct* interfaceStruct);

// Write single packet of data to HMI
static void writeToHMI(uint8_t port, uint8_t data, packetType type, DAD_UART_Struct* UARTptr);

// Writes single packet of data to microSD
static void writeToMicroSD(uint8_t port, uint8_t data, packetType type, packetStatus status, DAD_Interface_Struct* interfaceStruct);

// Report to HMI that sensor has been disconnected
static void sensorDisconnectHMI(int port, DAD_UART_Struct* UARTptr);

// TODO write commands to RSA

#endif /* DAD_INTERFACE_HANDLER_H_ */
