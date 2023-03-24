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
#include <stdio.h>
#include <stdlib.h>

// Utils
#include <string.h>
#include "DAD_Utils.h"

// HAL Includes
#include "HAL/DAD_UART.h"
#include "HAL/DAD_microSD.h"

// Configuration macros
#define LOG_INPUT
// #define WHAT_CAUSES_FSM_CHANGE
// #define REPORT_FAILURE
#define WRITE_TO_ONLY_ONE_FILE
// #define HIGH_FREQUENCY_POLLING
// #define FREQ_WRITE_TIME_TEST
#define USE_LUT
#define WRITE_TO_MICRO_SD
//#define RECEIVE_HMI_FEEDBACK
// #define PRIORITIZE_FFT

// UART Macros
#define RSA_BAUD 57600
#define RSA_BUFFER_SIZE 1500
#define HMI_BAUD 57600
#define HMI_BUFFER_SIZE 1024
#define MAX_FILENAME_SIZE 12

// Timer Macros
#define FSM_TIMER_HANDLE TIMER_A0_BASE
#define FFT_TIMER_HANDLE TIMER_A3_BASE
#ifdef HIGH_FREQUENCY_POLLING
#define FSM_TIMER_PERIOD 50                                 // Period in ms. Triggers an interrupt to kick off the FSM every so often.
#else
#define FSM_TIMER_PERIOD 750                                // Period in ms. Triggers an interrupt to kick off the FSM every so often.
#endif
#define FFT_TIMER_PERIOD 3000                               // Period in ms. Triggers an interrupt to kick off the FSM every so often.

// Packet Macros
#define STATUS_MASK 24
#define PACKET_TYPE_MASK 7
#define PORT_MASK 224
#define PACKET_SIZE 4                                       // Excludes end char 255
#define MESSAGE_LEN 37
#define NUM_OF_PORTS 8                                      // Number of ports
#define SIZE_OF_FFT 512


typedef enum {DISCON, CON_D, CON_ND, MSG} packetStatus;
typedef enum {TEMP = 0b000, HUM = 0b001, VIB = 0b010, MIC = 0b011, LOWBAT = 0b100, ERR = 0b101, STOP = 0b110, START = 0b111} packetType;
typedef enum {HOME = 0, PT1 = 1, PT2 = 2, PT3 = 3, PT4 = 4, PT5 = 5, PT6 = 6, PT7 = 7, PT8 = 8} HMIpage;

// Structure for encapsulating hardware interaction
    // Intended to be implemented as a singleton
typedef struct DAD_Interface_Struct_{
    // UART
    DAD_UART_Struct RSA_UART_struct;
    DAD_UART_Struct HMI_UART_struct;
    DAD_UART_Struct microSD_UART;

    // Timer Control
    Timer_A_UpModeConfig FSMtimerConfig;
    Timer_A_UpModeConfig FFTupdateTimer;

    // For writing to periphs
    uint8_t currentPort;                        // Describes what port is currently being written to. Useful for deciding whether we need to open a different file
    char fileName[MAX_FILENAME_SIZE + 1];       // File name
    HMIpage page;

    // Buffer pointers for buffering sound/vibration data
        // Buffers are loaded up with data.
        // Data from buffer is then sent all at once
    uint8_t freqBuf [NUM_OF_PORTS][SIZE_OF_FFT];

    // Lookup tables
    DAD_utilsStruct utils;

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

// Handles packets of "connected, no data" type
static void handle_CON_ND(packetType type, DAD_Interface_Struct* interfaceStruct);

// Handles packets of "message" type
static void handleMessage(packetType type, DAD_Interface_Struct* interfaceStruct);

// Report to HMI that sensor has been disconnected
static void HMIsensorDisconnect(uint8_t port, packetType type, DAD_Interface_Struct* interfaceStruct);

// Checks whether type needs FFT, tells HMI whether to expect FFT
static void HMIexpectFFT(packetType type, DAD_Interface_Struct* interfaceStruct);

// Write single packet of data to HMI
static void writeToHMI(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct);

// Writes single packet of data to microSD
static void writeToMicroSD(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct);

// Add packet to frequency buffer
static bool addToFreqBuffer(uint8_t packet[PACKET_SIZE+1], DAD_Interface_Struct* interfaceStruct);

static void writeFreqToPeriphs(packetType type, DAD_Interface_Struct* interfaceStruct);

// Find out which FFT to run
static HMIpage getPage(DAD_Interface_Struct* interfaceStruct);

#ifdef LOG_INPUT
static void logDebug(uint8_t* packet, DAD_Interface_Struct* interfaceStruct);
#endif

// TODO write commands to RSA
// TODO read command from HMI
// TODO read status from microSD

#endif /* DAD_INTERFACE_HANDLER_H_ */
