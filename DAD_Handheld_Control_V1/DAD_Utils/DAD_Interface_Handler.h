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
#include <HAL/DAD_GPIO.h>

// Configuration macros
#define LOG_INPUT
// #define WHAT_CAUSES_FSM_CHANGE
// #define REPORT_FAILURE
#define WRITE_TO_ONLY_ONE_FILE
// #define HIGH_FREQUENCY_POLLING
// #define FREQ_WRITE_TIME_TEST
#define WRITE_TO_HMI
#define WRITE_TO_MICRO_SD
#define GET_GPIO_FEEDBACK
//#define RECEIVE_HMI_FEEDBACK
// #define PRIORITIZE_FFT
// #define THROTTLE_UI_OUTPUT          // Caps UI update rate. WARNING: MUTUALLY EXCLUSIVE WITH FREQ_WRITE_TIME_TEST
// #define DELAY_UART_TRANSITION          // Ensure device is never tyransmitting and receiving on the same channel at any moment
// TODO remove code for unused configs

// UART Macros
#define RSA_BAUD            57600
#define RSA_BUFFER_SIZE     1700
#define HMI_BAUD            57600
#define HMI_BUFFER_SIZE     10
#define MAX_FILENAME_SIZE   12
#ifndef PORT_2_2_AS_RSA
#define RSA_RX_UART_HANDLE  EUSCI_A0_BASE
#else
#define RSA_RX_UART_HANDLE  EUSCI_A1_BASE
#endif
#define HMI_TX_UART_HANDLE  EUSCI_A2_BASE
#define HMI_RX_UART_HANDLE  EUSCI_A3_BASE


// FSM Timer Macros
#define FSM_TIMER_HANDLE TIMER_A0_BASE
#ifdef HIGH_FREQUENCY_POLLING
#define FSM_TIMER_PERIOD            50                       // Period in ms. Triggers an interrupt to kick off the FSM every so often.
#else
#define FSM_TIMER_PERIOD            750                     // Period in ms. Triggers an interrupt to kick off the FSM every so often.
#endif

// UART Timer Macros
#ifdef DELAY_UART_TRANSITION                                // Ensure device is never tyransmitting and receiving on the same channel at any moment
#define UART_DELAY_HANDLE           FSM_TIMER_HANDLE        // Same as FSM timer, as they are never used at the same time
#define UART_DELAY_US               1000
#endif

// UI Update Timer Macros
#define UI_UPDATE_TIMER_HANDLE      TIMER_A3_BASE
#define UI_UPDATE_TIMER_PERIOD      500                     // Period in ms. Throttles everything on UI except FSM
#define UI_FFT_UPDATE_TIMER_HANDLE  TIMER_A1_BASE
#define UI_FFT_UPDATE_TIMER_PERIOD  2500                    // Period in ms. Throttles just FSM on UI

// Packet Macros
#define STATUS_MASK         24
#define PACKET_TYPE_MASK    7
#define PORT_MASK           224
#define PACKET_SIZE         4                               // Excludes end char 255
#define MESSAGE_LEN         37
#define NUM_OF_PORTS        8                               // Number of ports
#define SIZE_OF_FFT         512
#define HMI_MSG_TYPE_MASK   0b11000000                      // Masks for what type each message is
#define HMI_MSG_DATA_MASK   0b00111111
#define HMI_MSG_START_CMD   254
#define HMI_MSG_STOP_CMD    255


typedef enum {DISCON, CON_D, CON_ND, MSG} packetStatus;
typedef enum {TEMP = 0b000, HUM = 0b001, VIB = 0b010, MIC = 0b011, LOWBAT = 0b100, ERR = 0b101, STOP = 0b110, START = 0b111} packetType;
//typedef enum {HOME = 0, PT1 = 1, PT2 = 2, PT3 = 3, PT4 = 4, PT5 = 5, PT6 = 6, PT7 = 7, PT8 = 8} HMIpage;
typedef enum {HOUR = 0, MIN = 1, SEC = 3, OTHER = 4} HMI_msgType;

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
    DAD_UART_Struct HMI_TX_UART_struct;
    DAD_UART_Struct HMI_RX_UART_struct;
    DAD_UART_Struct microSD_UART;

    // Timer Control
    Timer_A_UpModeConfig FSMtimerConfig;
    #ifdef THROTTLE_UI_OUTPUT
    Timer_A_UpModeConfig UIupdateTimer;
    Timer_A_UpModeConfig UIFFTupdateTimer;      // Limit FFT update to slower than rest of UI
    bool fftUpdateRequested[NUM_OF_PORTS];
    #endif
    #ifdef DELAY_UART_TRANSITION                // Ensure device is never tyransmitting and receiving on the same channel at any moment
    Timer_A_UpModeConfig UART_Switch_Timer;     // Same as FSM timer, as they are never used at the same time
    #endif

    // For writing to periphs
    // TODO replace currentport with gpio
    uint8_t currentPort;                        // Describes what port is currently being written to. Useful for deciding whether we need to open a different file
    char fileName[MAX_FILENAME_SIZE + 1];       // File name

    // HMI
    DAD_GPIO_Struct gpioStruct;
    uint8_t         currentHMIPage;
    bool            startStop;                  // True when start

    // Lookup tables
    DAD_utilsStruct utils;

} DAD_Interface_Struct;

// Initializes UART, timers necessary
void DAD_initInterfaces(DAD_Interface_Struct* interfaceStruct);

// Build packet from data in UART buffer
bool DAD_constructPacket(uint8_t* packet, DAD_UART_Struct* UARTptr);

// Checks whether type needs FFT, tells HMI whether to expect FFT
void DAD_Tell_UI_Whether_To_Expect_FFT(packetType type, DAD_Interface_Struct* interfaceStruct);

// Write single packet of data to HMI
void DAD_writeToUI(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct);

// Writes single packet of data to microSD
void DAD_writeToMicroSD(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct);

// Add packet to frequency buffer
bool DAD_addToFreqBuffer(uint8_t packet[PACKET_SIZE+1], DAD_Interface_Struct* interfaceStruct);

// Write frequency data to UI and microSD
void DAD_writeFreqToPeriphs(packetType type, DAD_Interface_Struct* interfaceStruct);

// Write frequency data to just microSD
void DAD_writeFreqToMicroSD(packetType type, DAD_Interface_Struct* interfaceStruct);

// Write frequency data to just UI
void DAD_writeFreqToUI(packetType type, DAD_Interface_Struct* interfaceStruct);

// Function for delaying transition btwn rx and tx
void DAD_delayRxTx();

// Find out which FFT to run
void DAD_handle_UI_Feedback(DAD_Interface_Struct* interfaceStruct);

#ifdef LOG_INPUT
void DAD_logDebug(uint8_t* packet, DAD_Interface_Struct* interfaceStruct);
#endif

#endif /* DAD_INTERFACE_HANDLER_H_ */
