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
#include <DAD_Utils/DAD_LUTs.h>

// HAL Includes
#include <HAL/DAD_UART.h>
#include <HAL/DAD_microSD.h>

// Configuration macros
#define LOG_INPUT
// #define WHAT_CAUSES_FSM_CHANGE
// #define REPORT_FAILURE
#define WRITE_TO_ONLY_ONE_FILE
// #define HIGH_FREQUENCY_POLLING
// #define FREQ_WRITE_TIME_TEST
#define WRITE_TO_MICRO_SD
// #define RECEIVE_HMI_FEEDBACK
// #define PRIORITIZE_FFT
#define THROTTLE_UI_OUTPUT          // Caps UI update rate. WARNING: MUTUALLY EXCLUSIVE WITH FREQ_WRITE_TIME_TEST

// UART Macros
#define RSA_BAUD            57600
#define RSA_BUFFER_SIZE     1500
#define HMI_BAUD            57600
#define HMI_BUFFER_SIZE     1024
#define MAX_FILENAME_SIZE   12

// FSM Timer Macros
#define FSM_TIMER_HANDLE TIMER_A0_BASE
#ifdef HIGH_FREQUENCY_POLLING
#define FSM_TIMER_PERIOD            50                       // Period in ms. Triggers an interrupt to kick off the FSM every so often.
#else
#define FSM_TIMER_PERIOD            750                     // Period in ms. Triggers an interrupt to kick off the FSM every so often.
#endif

// UI Update Timer Macros
#define UI_UPDATE_TIMER_HANDLE      TIMER_A3_BASE
#define UI_UPDATE_TIMER_PERIOD      500                     // Period in ms. Throttles everything on UI except FSM
#define UI_FFT_UPDATE_TIMER_HANDLE  TIMER_A1_BASE
#define UI_FFT_UPDATE_TIMER_PERIOD  2500                    // Period in ms. Throttles just FSM on UI

// Packet declarations
typedef enum {DISCON, CON_D, CON_ND, MSG} packetStatus;
typedef enum {TEMP = 0b000, HUM = 0b001, VIB = 0b010, MIC = 0b011, LOWBAT = 0b100, ERR = 0b101, STOP = 0b110, START = 0b111} packetType;
#define STATUS_MASK         24
#define PACKET_TYPE_MASK    7
#define PORT_MASK           224
#define PACKET_SIZE         4                               // Excludes end char 255
#define MESSAGE_LEN         37
#define NUM_OF_PORTS        8                               // Number of ports
#define SIZE_OF_FFT         512

// Talkback declarations
typedef enum {HOME = 0, PT1 = 1, PT2 = 2, PT3 = 3, PT4 = 4, PT5 = 5, PT6 = 6, PT7 = 7, PT8 = 8} HMIpage;
#define HMI_START_COMMAND   254
#define HMI_STOP_COMMAND    255

// Structure for requesting FFT updates to UI.
typedef struct FFTstruct_{
    bool requestedWriteToUI;
    packetType type;
} FFTstruct;

// Structure for encapsulating hardware interaction
    // Intended to be implemented as a singleton
typedef struct DAD_Interface_Struct_{
    // UART
    DAD_UART_Struct RSA_UART_struct;
    DAD_UART_Struct HMI_UART_struct;
    DAD_UART_Struct microSD_UART;

    // Timer Control
    Timer_A_UpModeConfig FSMtimerConfig;
    #ifdef THROTTLE_UI_OUTPUT
    Timer_A_UpModeConfig UIupdateTimer;
    Timer_A_UpModeConfig UIFFTupdateTimer;      // Limit FFT update to slower than rest of UI
    bool fftUpdateRequested[NUM_OF_PORTS];
    #endif
} DAD_Interface_Struct;

// Structure for recording non-hardware interfaces
    // UI feedback, FFT buffers and timing utils, LUTs for FFT writing
typedef struct DAD_utils_struct_{
    // For writing to periphs
    uint8_t currentPort;                        // Describes what port is currently being written to. Useful for deciding whether we need to open a different file
    char fileName[MAX_FILENAME_SIZE + 1];       // File name
    HMIpage page;
    uint8_t commandFromUI;

    // FFT Buffering/Handling
    #ifdef THROTTLE_UI_OUTPUT
    FFTstruct freqStructs[NUM_OF_PORTS];            // Struct for timing the sending of FFTs
    #endif
    uint8_t freqBuf [NUM_OF_PORTS][SIZE_OF_FFT];    // Buffer pointers for buffering sound/vibration data

    // Lookup tables
    DAD_LUT_Struct lutStruct;

}DAD_utils_struct;

// Initializes UART, timers necessary
void DAD_initInterfaces(DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Build packet from data in UART buffer
bool DAD_constructPacket(uint8_t* packet, DAD_UART_Struct* UARTptr);

// Checks whether type needs FFT, tells HMI whether to expect FFT
void DAD_Tell_UI_Whether_To_Expect_FFT(packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Write single packet of data to HMI
void DAD_writeToUI(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Writes single packet of data to microSD
void DAD_writeToMicroSD(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Add packet to frequency buffer
bool DAD_addToFreqBuffer(uint8_t packet[PACKET_SIZE+1], DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Write frequency data to UI and microSD
void DAD_writeFreqToPeriphs(packetType type, DAD_Interface_Struct* interfaceStruct);

// Write frequency data to just microSD
void DAD_writeFreqToMicroSD(packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Write frequency data to just UI
void DAD_writeFreqToUI(packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Logs all packets to microSD
#ifdef LOG_INPUT
void DAD_logDebug(uint8_t* packet, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);
#endif

#ifdef RECEIVE_HMI_FEEDBACK
// Read feedback from UI. Useful for deciding what FFT to send and what commands to run
HMIpage DAD_get_UI_Feedback(DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);
#endif

// TODO read status from microSD

#endif /* DAD_INTERFACE_HANDLER_H_ */
