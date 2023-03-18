/*
 * DAD_Interface_Handler.c
 *
 *  Created on: Mar 9, 2023
 *      Authors: Max Engel, Will Katz
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

    // Init buffs with all zeros
    int i, j;
    for(i = 0; i < NUM_OF_PORTS; i++){
        for(j = 0; j < SIZE_OF_FFT; j++){
            interfaceStruct->freqBuf[i][j] = 0;
        }
    }

    // Disables HMI communication to Handheld mcu
    DAD_UART_DisableInt(&(interfaceStruct->HMI_UART_struct));   // TODO remove this
}

void handleRSABuffer(DAD_Interface_Struct* interfaceStruct){
    // Read individual packets from buffer
    while(PACKET_SIZE + 1 <= DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct)){
        handlePacket(interfaceStruct);

        // TODO write moving average/average intensity

    }

    // Flush out remaining chars from read buffer.
    while(DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct) > 0){
        DAD_UART_GetChar(&interfaceStruct->RSA_UART_struct);
    }
}

static void *intToString(int num) {
    char str[PACKET_SIZE+1];
    sprintf(str, "%d", num);
    return str;
}

// Constructs packet from data in UART HAL's ring buffer
    // Buffer was originally constructed as a FIFO ring buffer.
static bool constructPacket(uint8_t packet[PACKET_SIZE], DAD_UART_Struct* UARTptr){
    // Remove any "end packet" characters
    char c = 255;
    while(c == 255 && DAD_UART_HasChar(UARTptr)){
        c = DAD_UART_GetChar(UARTptr);                      // remove char at front of buffer
    }

    #ifdef DEBUG
    if(DAD_UART_NumCharsInBuffer(UARTptr) <= PACKET_SIZE);  // Check number of chars in buffer
    #endif

    // Construct Packet
    if(DAD_UART_NumCharsInBuffer(UARTptr) > PACKET_SIZE){   // Check number of chars in buffer
        int i;
        packet[0] = c;                      // Fencepost to keep from reading too far
        for(i = 1; i < PACKET_SIZE; i++){
            c = DAD_UART_GetChar(UARTptr);
            packet[i] = c;
            if (packet[i] == 255)
                return false;
        }
        return true;

    }
    return false;
}

static void handlePacket(DAD_Interface_Struct* interfaceStruct)
{
    // Construct packet
    uint8_t packet[PACKET_SIZE];
    if(!constructPacket(packet, &interfaceStruct->RSA_UART_struct)){                            // If construct packet fails
        // Go to log file
        strcpy(interfaceStruct->fileName, "log.txt");
        DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);
        interfaceStruct->currentPort = 255;

        // Log error to microSD.
        DAD_microSD_Write("Error: Failed to construct packet\n", &interfaceStruct->microSD_UART); // A packet, misread, could have been.
        //logDebug(packet, interfaceStruct);
        return;
    }

    #ifdef DEBUG
    // Debug - Log Packet
    logDebug(packet, interfaceStruct);
    #endif

    // Interpret packet
    packetStatus PKstatus = (packetStatus)((packet[0] & STATUS_MASK) >> 3);
    packetType type = (packetType)(packet[0] & PACKET_TYPE_MASK);
    int port = (packet[0] & PORT_MASK) >> 5;

    // Deal with packet
    switch(PKstatus)
    {
        case DISCON:
            sensorDisconnect(port, type, interfaceStruct);
            break;
        case CON_D:
            handleData(port, type, packet, interfaceStruct); // TODO this causes program to break
            break;
        case CON_ND:

            // TODO watchdog for sensors that haven't updated in a while
                // intended for debugging, updating HMI if sensor isn't sending anymore

            //DAD_microSD_Write("CON_ND\n", &interfaceStruct->microSD_UART);    // TODO
            //

            break;
        case MSG:
            handleMessage(type, interfaceStruct);
            break;
        default:
            DAD_microSD_Write("bad packet\n", &interfaceStruct->microSD_UART);
    }
}

static void handleData(uint8_t port, packetType type, uint8_t packet[PACKET_SIZE], DAD_Interface_Struct* interfaceStruct){

    #ifndef WRITE_TO_ONLY_ONE_FILE                       // Disables writing to multiple files
    // Check that we are writing to the right file
    if(interfaceStruct->currentPort != port ){           // Compare data type to the file that is currently being written to
        // Set port
        sprintf(interfaceStruct->fileName, "port%d.csv", port+1);
        interfaceStruct->currentPort = port;

        // Open the correct file
        DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);
    }
    #endif

    #ifdef WRITE_TO_ONLY_ONE_FILE
    interfaceStruct->currentPort = port;
    #endif


    // Write data to periphs
    switch(type)
    {
        uint16_t data;

        case TEMP:
            // TODO condition data
            data = (packet[1] << 8) + packet[2];
            writeToHMI(data, type, interfaceStruct);
            writeToMicroSD(data, type, interfaceStruct);
            break;
        case HUM:
            // TODO condition data
            data = ((packet[1] << 8) + packet[2])%100;
            writeToHMI(data, type, interfaceStruct);
            writeToMicroSD(data, type, interfaceStruct);
            break;
        case VIB:
            // Fall through to mic. Same code
        case MIC:
            // Add packet to buffer,
            addToFreqBuffer(packet, interfaceStruct);
            // If second to last packet has been received, write to peripherals
            if(packet[3]*2 == SIZE_OF_FFT - 4)              // Note - second to last packet bc "last packet" would require receiving a byte of 0xFF, which would result in an invalid packet
                writeFreqToPeriphs(type, interfaceStruct);
            break;
    }
}

static void writeToHMI(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct)
{
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "HOME.s");
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, interfaceStruct->currentPort+49);
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "Val.txt=\"");

    char *val = intToString(data);
    int i = 0;
    while (val[i] != '\0') {
        DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, val[i]);
        i++;
    }

    // Message conditioning
    if(type == TEMP)
        DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 'F');
    else if(type == HUM)
        DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, '%');

    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, '\"');

    // End of transmission
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    return;
}

static void writeToMicroSD(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct){
    // Construct message
    char message[MESSAGE_LEN];
    switch(type){
    case TEMP:
        sprintf(message, "%d F, %d\n", data, interfaceStruct->currentPort + 1);   // port added for debug
        break;
    case HUM:
        sprintf(message, "%d %%, %d\n", data, interfaceStruct->currentPort + 1);   // port added for debug
        break;
    case VIB:
        sprintf(message, "%d dB, %d\n", data, interfaceStruct->currentPort + 1);   // port added for debug
        break;
    case MIC:
        sprintf(message, "%d dB, %d\n", data, interfaceStruct->currentPort + 1);   // port added for debug
        break;
    default:
        sprintf(message, "Error: Writing error - %d %%, %d\n", data, interfaceStruct->currentPort + 1);   // error catching
    }

    // Write data to microSD
    DAD_microSD_Write(message, &interfaceStruct->microSD_UART);
}

static void sensorDisconnect(uint8_t port, packetType type, DAD_Interface_Struct* interfaceStruct)
{
    // Write to HMI
    // Report sensor disconnected to HMI
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "HOME.s");
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, port+49);
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "Val.txt=\"NONE\"");
    // End of transmission
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);

    // Write to microSD
    // Construct message
    char message[17];
    switch(type){
    case TEMP:
        sprintf(message, "Temp disc @p%d\n", port);
        break;
    case HUM:
        sprintf(message, "Hum disc @p%d\n", port);
        break;
    case VIB:
        sprintf(message, "Vib disc @p%d\n", port);
        break;
    case MIC:
        sprintf(message, "Mic disc @p%d\n", port);
        break;
    default:
        sprintf(message, "Disconnect Error @p%d\n", port);
    }
    // Write to data file
    DAD_microSD_Write(message, &interfaceStruct->microSD_UART);
    // Open the log file
    strcpy(interfaceStruct->fileName, "log.txt");
    DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);
    // Write to log
    DAD_microSD_Write(message, &interfaceStruct->microSD_UART);
    interfaceStruct->currentPort = 255; // Record that current file is log.txt
}

static bool addToFreqBuffer(uint8_t packet[PACKET_SIZE], DAD_Interface_Struct* interfaceStruct){
    // If not allocated, calloc();
    uint8_t port = interfaceStruct->currentPort;
    uint16_t index = packet[PACKET_SIZE - 1];               // Promote to 16 bit for multiplication

    // TODO condition data
    // Add packet to buffer
    if(index * 2 + 1 < SIZE_OF_FFT && port < NUM_OF_PORTS){
        interfaceStruct->freqBuf[port][index*2] = packet[1];    // just unconditioned data for now
        interfaceStruct->freqBuf[port][index*2+1] = packet[2];  // just unconditioned data for now

        return true;
    }
    // TODO deal with dropped frequency packets
        // Currently assumes no packet is dropped
        // Currently assumes last packet in order is the last received
        // Currently assumes all data in buffer is up to data
    return false;
}

static void writeFreqToPeriphs(packetType type, DAD_Interface_Struct* interfaceStruct){
    if(interfaceStruct->currentPort < NUM_OF_PORTS){
        DAD_microSD_Write("\n\nFFT Start\n", &interfaceStruct->microSD_UART);
        int i;
        for(i = 0; i < SIZE_OF_FFT-2; i++){
            // TODO figure out how to send freq data to HMI
            writeToHMI(interfaceStruct->freqBuf[interfaceStruct->currentPort][i], type, interfaceStruct);
            writeToMicroSD(interfaceStruct->freqBuf[interfaceStruct->currentPort][i], type, interfaceStruct);
        }
        DAD_microSD_Write("FFT End\n\n", &interfaceStruct->microSD_UART);
    }
}

static void handleMessage(packetType type, DAD_Interface_Struct* interfaceStruct){
    // Open the microSD log file
    strcpy(interfaceStruct->fileName, "log.txt");
    DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);
    interfaceStruct->currentPort = 255;

    // TODO timestamp
    // TODO write message to HMI
    switch(type){
        case LOWBAT:
            DAD_microSD_Write("RSA Low Battery detected\n", &interfaceStruct->microSD_UART);
            break;
        case ERR:
            DAD_microSD_Write("Error: RSA Error reported\n", &interfaceStruct->microSD_UART);
            // TODO expand on this?
            break;
        case STOP:
            DAD_microSD_Write("Data collection halted\n", &interfaceStruct->microSD_UART);
            break;
        case START:
            DAD_microSD_Write("Data collection started\n", &interfaceStruct->microSD_UART);
            break;
    }
}

#ifdef DEBUG
// Write everything to log file
static void logDebug(uint8_t* packet, DAD_Interface_Struct* interfaceStruct){
    // Open log file
    if(strcmp(interfaceStruct->fileName, "log.txt") != 0){  // Debug - write everything to log
        strcpy(interfaceStruct->fileName, "log.txt");
        DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);
        interfaceStruct->currentPort = 255;
    }

    // Write to log file
    int i;
    char message[2];
    for(i = 0; i < PACKET_SIZE; i++){
        sprintf(message, "%d ", packet[i]);
        DAD_UART_Write_Str(&interfaceStruct->microSD_UART, message);
    }
    DAD_UART_Write_Str(&interfaceStruct->microSD_UART, "\n");
}
#endif
