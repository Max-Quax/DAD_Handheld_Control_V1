/*
 * DAD_Interface_Handler.c
 *
 *  Created on: Mar 9, 2023
 *      Authors: Max Engel, Will Katz
 */

// DEMO
    // TODO HMI talk back to MSP
    // TODO Integrate BT

// ISSUES
    // Processing incoming packets may be fucky
        // Symptoms
            // Sensor 1 does not update
            // random sensors connect
            // disconnection is fucky
    // Blocking

#include "DAD_Interface_Handler.h"
#include <string.h>

// Initializes UART, timers necessary
void initInterfaces(DAD_Interface_Struct* interfaceStruct){
    // RSA Interface init
    DAD_UART_Set_Config(RSA_BAUD, EUSCI_A0_BASE, &(interfaceStruct->RSA_UART_struct));
    DAD_UART_Init(&interfaceStruct->RSA_UART_struct, RSA_BUFFER_SIZE);

    // HMI Interface init
    DAD_UART_Set_Config(HMI_BAUD, EUSCI_A2_BASE, &interfaceStruct->HMI_UART_struct);
    DAD_UART_Init(&interfaceStruct->HMI_UART_struct, HMI_BUFFER_SIZE);
    interfaceStruct->page = HOME;

    // microSD Interface init, open log file
    interfaceStruct->currentPort = STOP;                        // Describes what file is currently being written to
    strcpy(interfaceStruct->fileName, "log.txt");
    DAD_microSD_InitUART(&interfaceStruct->microSD_UART);
    DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);

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
    DAD_UART_DisableInt(&interfaceStruct->HMI_UART_struct);   // TODO remove this

    // Initialize Lookup tables
    DAD_Utils_initFreqLUT(&interfaceStruct->utils);

    // Initialize timer for testing write time
    #ifdef FREQ_WRITE_TIME_TEST
    DAD_Timer_Initialize_ms(60000, TIMER_A1_BASE, &(interfaceStruct->FSMtimerConfig));
    DAD_Timer_Start(TIMER_A1_BASE);
    #endif
}

void handleRSABuffer(DAD_Interface_Struct* interfaceStruct){
    #ifdef WRITE_TO_ONLY_ONE_FILE
    strcpy(interfaceStruct->fileName, "log.txt");
    DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);
    #endif

    // Read individual packets from buffer
    while(PACKET_SIZE < DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct)){
        handlePacket(interfaceStruct);

        // TODO write moving average/average intensity

    }

    // Flush out remaining chars from read buffer.
    while(DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct) > 0){
        DAD_UART_GetChar(&interfaceStruct->RSA_UART_struct);
    }

    #ifdef DEBUG
    char* message = "EndBuf\n\n";
    DAD_UART_Write_Str(&interfaceStruct->microSD_UART, message);
    #endif
}

static void handlePacket(DAD_Interface_Struct* interfaceStruct)
{
    // Construct packet
    uint8_t packet[PACKET_SIZE + 1];
    if(!constructPacket(packet, &interfaceStruct->RSA_UART_struct)){                                // If construct packet fails
        #ifdef REPORT_FAILURE
        // Go to log file
        strcpy(interfaceStruct->fileName, "log.txt");
        DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);
        interfaceStruct->currentPort = 255;

        // Log error to microSD.
        DAD_microSD_Write("Error: Failed to construct packet\n", &interfaceStruct->microSD_UART);   // A packet, misread, could have been.
        #endif
        return;
    }


    #ifdef DEBUG
    // Debug - Log Packet
    logDebug(packet, interfaceStruct);
    if(packet[3] >= 253 ){
        int i = 0;
    }
    #endif

    // Interpret packet
    packetStatus PKstatus = (packetStatus)((packet[0] & STATUS_MASK) >> 3);
    packetType type = (packetType)(packet[0] & PACKET_TYPE_MASK);
    uint8_t port = (packet[0] & PORT_MASK) >> 5;

    #ifdef WRITE_TO_ONLY_ONE_FILE
    interfaceStruct->currentPort = port;
    #endif

    // Deal with packet
    switch(PKstatus)
    {
        case DISCON:
            HMIsensorDisconnect(port, type, interfaceStruct);
            break;
        case CON_D:
            handleData(port, type, packet, interfaceStruct);
            break;
        case CON_ND:
            handle_CON_ND(type, interfaceStruct);
            break;
        case MSG:
            handleMessage(type, interfaceStruct);
            break;
        default:
            DAD_microSD_Write("bad packet\n", &interfaceStruct->microSD_UART);
    }
}


// Constructs packet from data in UART HAL's ring buffer
    // Buffer was originally constructed as a FIFO ring buffer.
static bool constructPacket(uint8_t packet[PACKET_SIZE], DAD_UART_Struct* UARTptr){
    // Remove any "end packet" characters
    char c = 255;
    while(c == 255 && DAD_UART_HasChar(UARTptr)){
        c = DAD_UART_GetChar(UARTptr);                          // remove char at front of buffer
    }

    // Construct Packet
    uint8_t numCharsPacked;                                     // Keeps track of number of characters packed into packet
    while (DAD_UART_NumCharsInBuffer(UARTptr) >= PACKET_SIZE){  // Check number of chars in buffer
        numCharsPacked = 0;

        do
        {
            numCharsPacked++;
            packet[0] = packet[1];
            packet[1] = packet[2];
            packet[2] = packet[3];
            packet[3] = c;
            c = DAD_UART_GetChar(UARTptr);
        }while(c != 255 && DAD_UART_NumCharsInBuffer(UARTptr) > PACKET_SIZE);
        if(c == 255 && numCharsPacked >= PACKET_SIZE)
        {
            return true;
        }
        c = DAD_UART_GetChar(UARTptr);                          // get next char
    }

    // Not enough bytes to make a full packet
    // Flush out loose bytes
//    while(DAD_UART_NumCharsInBuffer(UARTptr) > 0)
//        DAD_UART_GetChar(UARTptr);
//
    return false;






//    // Remove any "end packet" characters
//    char c = 255;
//    while(c == 255 && DAD_UART_HasChar(UARTptr)){
//        c = DAD_UART_GetChar(UARTptr);                      // remove char at front of buffer
//    }
//
//    #ifdef DEBUG
//    if(DAD_UART_NumCharsInBuffer(UARTptr) <= PACKET_SIZE);  // Check number of chars in buffer
//    #endif
//
//    // Construct Packet
//    if(DAD_UART_NumCharsInBuffer(UARTptr) > PACKET_SIZE){   // Check number of chars in buffer
//        int i;
//        packet[0] = c;                                      // Fencepost to keep from reading too far
//        for(i = 1; i < PACKET_SIZE; i++){
//            DAD_UART_Peek(UARTptr, &c);
//            if(c != 255){
//                packet[i] = c;
//                DAD_UART_GetChar(UARTptr);
//            }
//            else
//                return false;
//        }
//        return true;
//    }
//
//
//    // Flush out loose bytes
//    while(DAD_UART_NumCharsInBuffer(UARTptr) > 0)
//        DAD_UART_GetChar(UARTptr);
//
//    return false;

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


    // Write data to periphs
    switch(type)
    {
        uint16_t data;

        case TEMP:
            // TODO condition data
            data = ((packet[1] << 8) + packet[2]) % 110;
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
            if(packet[1]*2 == SIZE_OF_FFT - 4)              // Note - second to last packet bc "last packet" would require receiving a byte of 0xFF, which would result in an invalid packet
                writeFreqToPeriphs(type, interfaceStruct);
            break;
    }
}

// Handles packets of "connected, no data" type
static void handle_CON_ND(packetType type, DAD_Interface_Struct* interfaceStruct){
    // TODO check sensor still responding
    HMIexpectFFT(type, interfaceStruct);
}

static void writeToHMI(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct)
{
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "HOME.s");
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, interfaceStruct->currentPort+49);
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "Val.txt=\"");

    // Write data to HMI
    char val[7] = "";
    sprintf(val, "%d", data);
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, val);

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
#ifdef WRITE_TO_MICRO_SD
    // Construct message
    char message[MESSAGE_LEN];
    switch(type){
    case TEMP:
        sprintf(message, "p%d, Temp, %dF\n", interfaceStruct->currentPort + 1, data);      // port added for debug
        break;
    case HUM:
        sprintf(message, "p%d, Hum, %d%%\n", interfaceStruct->currentPort + 1, data);     // port added for debug
        break;
    case VIB:   // Frequency data should be written to microSD using writeFreqToPeriphs
    case MIC:
    default:
        sprintf(message, "Error: Writing error - %d %%, %d\n", data, interfaceStruct->currentPort + 1);   // error catching
    }

    // Write data to microSD
    DAD_microSD_Write(message, &interfaceStruct->microSD_UART);
#endif
}


static void HMIsensorDisconnect(uint8_t port, packetType type, DAD_Interface_Struct* interfaceStruct)
{
    // Write to HMI
    // Report sensor disconnected to HMI
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "HOME.s");
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, interfaceStruct->currentPort + 49);
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "Val.txt=\"NONE\"");
    // End of transmission
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);

    // Report Stop Sending FFT
        // HOME.f<sensornumber>.val=0
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "HOME.f");
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, interfaceStruct->currentPort + 49);
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, ".val=0");
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

// Checks whether type needs FFT, tells HMI whether to expect FFT
static void HMIexpectFFT(packetType type, DAD_Interface_Struct* interfaceStruct){
    // Assert whether HMI expects FFT data
            // HOME.f<sensornumber>.val=<0 or 1, depending on whether we want FFT>
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "HOME.f");
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, interfaceStruct->currentPort + 49);

    (type == VIB || type == MIC) ? DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, ".val=1") :
            DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, ".val=0");

    // End of transmission
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
}

static bool addToFreqBuffer(uint8_t packet[PACKET_SIZE], DAD_Interface_Struct* interfaceStruct){
    uint8_t port = interfaceStruct->currentPort;
    uint16_t index = packet[1];                                 // Promote to 16 bit for multiplication

    // TODO condition data
    // Add packet to buffer
    if(index * 2 + 1 < SIZE_OF_FFT && port < NUM_OF_PORTS){
        interfaceStruct->freqBuf[port][index*2] = packet[2];    // just unconditioned data for now
        interfaceStruct->freqBuf[port][index*2+1] = packet[3];  // just unconditioned data for now

        return true;
    }
    // TODO deal with dropped frequency packets
        // Currently assumes no packet is dropped
        // Currently assumes last packet in order is the last received
        // Currently assumes all data in buffer is up to date
    return false;
}

#ifdef USE_LUT
static void writeFreqToPeriphs(packetType type, DAD_Interface_Struct* interfaceStruct){
    uint8_t port = interfaceStruct->currentPort;
    HMIexpectFFT(type, interfaceStruct);
    #ifdef FREQ_WRITE_TIME_TEST
    DAD_Timer_Restart(TIMER_A1_BASE,  &interfaceStruct->FSMtimerConfig);
    #endif
    #ifdef RECEIVE_HMI_FEEDBACK
    if(interfaceStruct->currentPort < NUM_OF_PORTS && getPage(interfaceStruct) == port + 1){
    #else
    if(interfaceStruct->currentPort < NUM_OF_PORTS){
    #endif
        // Write preamble to microSD
        char microSDmsg[17];
        (type == VIB) ? sprintf(microSDmsg, "\np%d, Vib, ", port + 1):
                        sprintf(microSDmsg, "\np%d, Mic, ", port + 1);
        DAD_microSD_Write(microSDmsg, &interfaceStruct->microSD_UART);

        uint8_t data;
        uint16_t i;
        for(i = 0; i < SIZE_OF_FFT-2; i++){
            data = interfaceStruct->freqBuf[port][i];

            // Write to HMI
            DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, DAD_Utils_getHMIStr(data, &interfaceStruct->utils));
            DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
            DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
            DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);

            // Write to microSD
            DAD_microSD_Write(DAD_Utils_getMicroSDStr(data, &interfaceStruct->utils), &interfaceStruct->microSD_UART);
        }
        DAD_microSD_Write("FFT End\n\n", &interfaceStruct->microSD_UART);
    }
    #ifdef FREQ_WRITE_TIME_TEST
    float timeElapsed;
    timeElapsed = DAD_Timer_Stop(TIMER_A1_BASE,  &interfaceStruct->FSMtimerConfig);
    if(true);
    #endif
}
#endif

#ifndef USE_LUT
static void writeFreqToPeriphs(packetType type, DAD_Interface_Struct* interfaceStruct){
    HMIexpectFFT(packetType type, DAD_Interface_Struct* interfaceStruct)
    #ifdef FREQ_WRITE_TIME_TEST
    DAD_Timer_Restart(TIMER_A1_BASE,  &interfaceStruct->FSMtimerConfig);
    #endif

    if(interfaceStruct->currentPort < NUM_OF_PORTS){
        uint8_t port = interfaceStruct->currentPort;
        char microSDmsg[17];

        (type == VIB) ? sprintf(microSDmsg, "\np%d, Vib, ", port + 1):
                sprintf(microSDmsg, "\np%d, Mic, ", port + 1);

        DAD_microSD_Write(microSDmsg, &interfaceStruct->microSD_UART);


        char debugMsg [25];
        uint8_t data;
        uint16_t i;
        for(i = 0; i < SIZE_OF_FFT-2; i++){
            data = interfaceStruct->freqBuf[port][i];

            // Write to HMI
            sprintf(debugMsg, "add %d,0,%d", HMI_FFT_ID, data);
            DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, debugMsg);
            DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
            DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
            DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);

            // Write to microSD
            sprintf(microSDmsg, "%d,", data);
            DAD_microSD_Write(microSDmsg, &interfaceStruct->microSD_UART);
        }
        DAD_microSD_Write("FFT End\n\n", &interfaceStruct->microSD_UART);
    }
    #ifdef FREQ_WRITE_TIME_TEST
    float timeElapsed;
    timeElapsed = DAD_Timer_Stop(TIMER_A1_BASE,  &interfaceStruct->FSMtimerConfig);
    if(true);
    #endif
}
#endif

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

// Find out what page user is on
static HMIpage getPage(DAD_Interface_Struct* interfaceStruct){
    while(DAD_UART_HasChar(&interfaceStruct->HMI_UART_struct)){
        interfaceStruct->page = DAD_UART_GetChar(&interfaceStruct->HMI_UART_struct);
    }
    return interfaceStruct->page;
}
