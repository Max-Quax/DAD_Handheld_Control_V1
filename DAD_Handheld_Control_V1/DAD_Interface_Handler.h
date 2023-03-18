/*
 * DAD_Packet_Handler.h
 *
 *  Created on: Mar 9, 2023
 *      Author: Will Katz, Max Engel

    Meant to abstract the interfaces away.

 */

#ifndef DAD_INTERFACE_HANDLER_H_
#define DAD_INTERFACE_HANDLER_H_

// Standard includes
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// HAL Includes
#include "HAL/DAD_UART.h"
#include "HAL/DAD_microSD.h"

// Debug macros
// #define DEBUG
#define WRITE_TO_ONLY_ONE_FILE

// UART Macros
#define RSA_BAUD 9600
#define RSA_BUFFER_SIZE 1024
#define HMI_BAUD 9600
#define HMI_BUFFER_SIZE 1024
#define MAX_FILENAME_SIZE 12

// Timer Macros
#define FSM_TIMER_HANDLE TIMER_A0_BASE
#define FSM_TIMER_PERIOD 500            // Period in ms. Triggers an interrupt to kick off the FSM every so often.

// Packet Macros
#define STATUS_MASK 24
#define PACKET_TYPE_MASK 7
#define PORT_MASK 224
#define PACKET_SIZE 4                                       // Excludes end char 255
#define MESSAGE_LEN (sizeof(char)*(PACKET_SIZE) + 1)    // size of message
#define NUM_OF_PORTS 8                                      // Number of ports
#define SIZE_OF_FFT 512

typedef enum {DISCON, CON_D, CON_ND, MSG} packetStatus;
typedef enum {TEMP = 0b000, HUM = 0b001, VIB = 0b010, MIC = 0b011, LOWBAT = 0b100, ERR = 0b101, STOP = 0b110, START = 0b111} packetType;

typedef struct DAD_Interface_Struct_{
    // UART
    DAD_UART_Struct RSA_UART_struct;
    DAD_UART_Struct HMI_UART_struct;
    DAD_UART_Struct microSD_UART;

    // Timer Control
    Timer_A_UpModeConfig FSMtimerConfig;

    // For writing to periphs
    uint8_t currentPort;                        // Describes what port is currently being written to. Useful for deciding whether we need to open a different file
    char fileName[MAX_FILENAME_SIZE + 1];       // File name

    // Buffer pointers for buffering sound/vibration data
        // Buffers are loaded up with data.
        // Data from buffer is then sent all at once
    float freqBuf [NUM_OF_PORTS][SIZE_OF_FFT];

} DAD_Interface_Struct;

// Initializes UART, timers necessary
void initInterfaces(DAD_Interface_Struct* interfaceStruct);

// Read all elements from RSA UART buffer to peripherals
void handleRSABuffer(DAD_Interface_Struct* interfaceStruct);

// Build packet from data in UART buffer
static bool constructPacket(uint8_t* packet, DAD_UART_Struct* UARTptr);

// Handle an individual packet
static void handlePacket(DAD_Interface_Struct* interfaceStruct);

// Processes data packet, sends data to peripherals
static void handleData(uint8_t port, packetType type, uint8_t packet[PACKET_SIZE+1], DAD_Interface_Struct* interfaceStruct);

// Write single packet of data to HMI
static void writeToHMI(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct);

// Writes single packet of data to microSD
static void writeToMicroSD(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct);

// Handles packets of "message" type
static void handleMessage(packetType type, DAD_Interface_Struct* interfaceStruct);

// Report to HMI that sensor has been disconnected
static void sensorDisconnect(uint8_t port, packetType type, DAD_Interface_Struct* interfaceStruct);

// Add packet to frequency buffer
static bool addToFreqBuffer(uint8_t packet[PACKET_SIZE+1], DAD_Interface_Struct* interfaceStruct);

static void writeFreqToPeriphs(packetType type, DAD_Interface_Struct* interfaceStruct);

#ifdef DEBUG
static void logDebug(uint8_t* packet, DAD_Interface_Struct* interfaceStruct);
#endif

// TODO write commands to RSA
// TODO read command from HMI
// TODO read status from microSD

#endif /* DAD_INTERFACE_HANDLER_H_ */
