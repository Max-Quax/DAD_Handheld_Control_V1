/*
 * DAD_Interface_Handler.c
 *
 *  Created on: Mar 9, 2023
 *      Authors: Max Engel, Will Katz
 */

#include <DAD_Utils/DAD_Interface_Handler.h>

// Initializes UART, timers necessary
void DAD_initInterfaces(DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct){
    // RSA Interface init
    DAD_UART_Set_Config(RSA_BAUD, EUSCI_A0_BASE, &(interfaceStruct->RSA_UART_struct));
    DAD_UART_Init(&interfaceStruct->RSA_UART_struct, RSA_BUFFER_SIZE);

    // HMI Interface init
    DAD_UART_Set_Config(HMI_BAUD, EUSCI_A2_BASE, &interfaceStruct->HMI_UART_struct);
    DAD_UART_Init(&interfaceStruct->HMI_UART_struct, HMI_BUFFER_SIZE);
    utilsStruct->page = HOME;
    utilsStruct->commandFromUI = HMI_START_COMMAND;
    #ifndef RECEIVE_HMI_FEEDBACK
    DAD_UART_DisableInt(&interfaceStruct->HMI_UART_struct);     // Disables HMI communication to Handheld mcu
    #endif

    // microSD Interface init, open log file
    utilsStruct->currentPort = STOP;                            // Describes what file is currently being written to
    strcpy(utilsStruct->fileName, "log.txt");
    DAD_microSD_InitUART(&interfaceStruct->microSD_UART);
    DAD_microSD_openFile(utilsStruct->fileName, &interfaceStruct->microSD_UART);

    // Wakeup Timer Init
    DAD_Timer_Initialize_ms(FSM_TIMER_PERIOD, FSM_TIMER_HANDLE, &interfaceStruct->FSMtimerConfig);

    #ifdef THROTTLE_UI_OUTPUT
    // UI Update Timers init and start
    DAD_Timer_Initialize_ms(UI_UPDATE_TIMER_PERIOD, UI_UPDATE_TIMER_HANDLE, &interfaceStruct->UIupdateTimer);
    DAD_Timer_Start(UI_UPDATE_TIMER_HANDLE);
    DAD_Timer_Initialize_ms(UI_FFT_UPDATE_TIMER_PERIOD, UI_FFT_UPDATE_TIMER_HANDLE, &interfaceStruct->UIFFTupdateTimer);
    DAD_Timer_Start(UI_FFT_UPDATE_TIMER_HANDLE);
    #endif

    // Init buffs with all zeros (memset was not cooperating. Skill issue?)
    int i, j;
    for(i = 0; i < NUM_OF_PORTS; i++){
        #ifdef THROTTLE_UI_OUTPUT
        utilsStruct->freqStructs[i].requestedWriteToUI = false;
        utilsStruct->freqStructs[i].type = START;
        #endif
        for(j = 0; j < SIZE_OF_FFT; j++)
            utilsStruct->freqBuf[i][j] = 0;
    }


    // Initialize Lookup tables
    DAD_Utils_initFreqLUT(&utilsStruct->lutStruct);

    // Initialize timer for testing write time
    #ifdef FREQ_WRITE_TIME_TEST
    DAD_Timer_Initialize_ms(60000, TIMER_A1_BASE, &(interfaceStruct->FSMtimerConfig));
    DAD_Timer_Start(TIMER_A1_BASE);
    #endif
}

// Constructs packet from data in UART HAL's ring buffer
    // Buffer was originally constructed as a FIFO ring buffer.
bool DAD_constructPacket(uint8_t packet[PACKET_SIZE], DAD_UART_Struct* UARTptr){
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
    return false;                                               // Couldn't construct a packet with current buffer
}


void DAD_writeToUI(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct)
{
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "HOME.s");
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, utilsStruct->currentPort+49);
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

void DAD_writeToMicroSD(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct){
#ifdef WRITE_TO_MICRO_SD
    // Construct message
    char message[MESSAGE_LEN];
    switch(type){
    case TEMP:
        sprintf(message, "p%d, Temp, %dF\n", utilsStruct->currentPort + 1, data);      // port added for debug
        break;
    case HUM:
        sprintf(message, "p%d, Hum, %d%%\n", utilsStruct->currentPort + 1, data);     // port added for debug
        break;
    case VIB:   // Frequency data should be written to microSD using DAD_writeFreqToPeriphs
    case MIC:
    default:
        sprintf(message, "Error: Writing error - %d %%, %d\n", data, utilsStruct->currentPort + 1);   // error catching
    }

    // Write data to microSD
    DAD_microSD_Write(message, &interfaceStruct->microSD_UART);
#endif
}



// Checks whether type needs FFT, tells HMI whether to expect FFT
void DAD_Tell_UI_Whether_To_Expect_FFT(packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct){
    // Assert whether HMI expects FFT data
            // HOME.f<sensornumber>.val=<0 or 1, depending on whether we want FFT>
    DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "HOME.f");
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, utilsStruct->currentPort + 49);

    (type == VIB || type == MIC) ? DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, ".val=1") :
            DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, ".val=0");

    // End of transmission
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);

    // Display "VIB" or "MIC" on the home screen
    if(type == VIB || type == MIC){
        // Tell HMI to write Freq
        DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "HOME.s");
        DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, utilsStruct->currentPort + 49);
        //Update text on home screen
        (type == VIB) ? DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "Val.txt=\"VIB\"") :
                DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, "Val.txt=\"MIC\"");
        // End of transmission
        DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
        DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
        DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
    }
}

bool DAD_addToFreqBuffer(uint8_t packet[PACKET_SIZE], DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct){
    uint8_t port = utilsStruct->currentPort;
    uint16_t index = packet[1];                                 // Promote to 16 bit for multiplication

    // TODO condition data
    // Add packet to buffer
    if(index * 2 + 1 < SIZE_OF_FFT && port < NUM_OF_PORTS){
        utilsStruct->freqBuf[port][index*2] = packet[2];    // just unconditioned data for now
        utilsStruct->freqBuf[port][index*2+1] = packet[3];  // just unconditioned data for now

        return true;
    }
    // TODO deal with dropped frequency packets
        // Currently assumes no packet is dropped
        // Currently assumes last packet in order is the last received
        // Currently assumes all data in buffer is up to date
    return false;
}

#ifndef THROTTLE_UI_OUTPUT
void DAD_writeFreqToPeriphs(packetType type, DAD_Interface_Struct* interfaceStruct){
    uint8_t port = interfaceStruct->currentPort;
    DAD_Tell_UI_Whether_To_Expect_FFT(type, interfaceStruct);
    #ifdef FREQ_WRITE_TIME_TEST
    DAD_Timer_Restart(TIMER_A1_BASE,  &interfaceStruct->FSMtimerConfig);
    #endif
    #ifdef RECEIVE_HMI_FEEDBACK
    if(interfaceStruct->currentPort < NUM_OF_PORTS && DAD_get_UI_Page(interfaceStruct) == port + 1){
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


#ifdef THROTTLE_UI_OUTPUT
// Write frequency data to just microSD
void DAD_writeFreqToMicroSD(packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct){
    uint8_t port = utilsStruct->currentPort;
    DAD_Tell_UI_Whether_To_Expect_FFT(type, interfaceStruct, utilsStruct);
    #ifdef FREQ_WRITE_TIME_TEST
    DAD_Timer_Restart(TIMER_A1_BASE,  &interfaceStruct->FSMtimerConfig);
    #endif
    #ifdef RECEIVE_HMI_FEEDBACK
    if(interfaceStruct->currentPort < NUM_OF_PORTS && DAD_get_UI_Page(interfaceStruct) == port + 1){
    #else
    if(utilsStruct->currentPort < NUM_OF_PORTS){
    #endif
        // Write preamble to microSD
        char microSDmsg[17];
        (type == VIB) ? sprintf(microSDmsg, "\np%d, Vib, ", port + 1):
                        sprintf(microSDmsg, "\np%d, Mic, ", port + 1);
        DAD_microSD_Write(microSDmsg, &interfaceStruct->microSD_UART);

        uint16_t i;
        for(i = 0; i < SIZE_OF_FFT-2; i++){
            // Write to microSD
            DAD_microSD_Write(DAD_Utils_getMicroSDStr(utilsStruct->freqBuf[port][i], &utilsStruct->lutStruct), &interfaceStruct->microSD_UART);
        }
        DAD_microSD_Write("FFT End\n\n", &interfaceStruct->microSD_UART);
    }
    #ifdef FREQ_WRITE_TIME_TEST
    float timeElapsed;
    timeElapsed = DAD_Timer_Stop(TIMER_A1_BASE,  &interfaceStruct->FSMtimerConfig);
    if(true);
    #endif
}

void DAD_writeFreqToUI(packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct){
    uint8_t port = utilsStruct->currentPort;
    DAD_Tell_UI_Whether_To_Expect_FFT(type, interfaceStruct, utilsStruct);
    #ifdef FREQ_WRITE_TIME_TEST
    DAD_Timer_Restart(TIMER_A1_BASE,  &interfaceStruct->FSMtimerConfig);
    #endif
    #ifdef RECEIVE_HMI_FEEDBACK
    if(interfaceStruct->currentPort < NUM_OF_PORTS && DAD_get_UI_Page(interfaceStruct) == port + 1){
    #else
    if(utilsStruct->currentPort < NUM_OF_PORTS){
    #endif
        uint8_t data;
        uint16_t i;
        for(i = 0; i < SIZE_OF_FFT-2; i++){
            data = utilsStruct->freqBuf[port][i];
            // Write to HMI
            DAD_UART_Write_Str(&interfaceStruct->HMI_UART_struct, DAD_Utils_getHMIStr(data, &utilsStruct->lutStruct));
            DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
            DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
            DAD_UART_Write_Char(&interfaceStruct->HMI_UART_struct, 255);
        }
    }
    #ifdef FREQ_WRITE_TIME_TEST
    float timeElapsed;
    timeElapsed = DAD_Timer_Stop(TIMER_A1_BASE,  &interfaceStruct->FSMtimerConfig);
    if(true);
    #endif
}
#endif



#ifdef LOG_INPUT
// Write everything to log file
void DAD_logDebug(uint8_t* packet, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct){
    // Open log file
    if(strcmp(utilsStruct->fileName, "log.txt") != 0){  // Debug - write everything to log
        strcpy(utilsStruct->fileName, "log.txt");
        DAD_microSD_openFile(utilsStruct->fileName, &interfaceStruct->microSD_UART);
        utilsStruct->currentPort = 255;
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

#ifdef RECEIVE_HMI_FEEDBACK
// Find out what page user is on
HMIpage DAD_get_UI_Feedback(DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct){
    char c;
    while(DAD_UART_HasChar(&interfaceStruct->HMI_UART_struct)){
        c = DAD_UART_GetChar(&interfaceStruct->HMI_UART_struct);
        utilsStruct->page = c;  // TODO step through to make sure this is converting right

        // Save whether HMI wants MSP to send start/stop commands
        if(c == HMI_START_COMMAND || c == HMI_STOP_COMMAND){
            utilsStruct->commandFromUI = c;
        }
    }
    return utilsStruct->page;
}
#endif
