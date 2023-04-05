/*
 * DAD_Interface_Handler.c
 *
 *  Created on: Mar 9, 2023
 *      Authors: Max Engel, Will Katz
 */

#include <DAD_Utils/DAD_Interface_Handler.h>

// Initializes UART, timers necessary
void DAD_initInterfaces(DAD_Interface_Struct* interfaceStruct){
    // Init RSA UART
    DAD_UART_Set_Config(RSA_BAUD, RSA_RX_UART_HANDLE, &(interfaceStruct->RSA_UART_struct));
    if(!DAD_UART_Init(&interfaceStruct->RSA_UART_struct, RSA_BUFFER_SIZE))
        while(1);

    // HMI Interface init
        // Note - HMI comms split in two bc MSP drivers sometimes have issues switching from RX to TX
        // Issue can be mitigated with timers. Exact cause of issue undiagnosed. Using 2 different UART channels instead.
    DAD_UART_Set_Config(HMI_BAUD, HMI_RX_UART_HANDLE, &interfaceStruct->HMI_RX_UART_struct);
    DAD_UART_Set_Config(HMI_BAUD, HMI_TX_UART_HANDLE, &interfaceStruct->HMI_TX_UART_struct);
    if(!DAD_UART_Init(&interfaceStruct->HMI_RX_UART_struct, HMI_BUFFER_SIZE))
        while(1);
    if(!DAD_UART_Init(&interfaceStruct->HMI_TX_UART_struct, 1))
        while(1);
    interfaceStruct->currentHMIPage = 0;
    MAP_UART_disableInterrupt(interfaceStruct->HMI_TX_UART_struct.moduleInst, EUSCI_A_UART_RECEIVE_INTERRUPT);
    DAD_GPIO_Init(&interfaceStruct->gpioStruct);

    // microSD Interface init, open log file
    interfaceStruct->sensorPortOrigin = STOP;                        // Describes what file is currently being written to
    strcpy(interfaceStruct->fileName, "log.txt");
    if(!DAD_microSD_InitUART(&interfaceStruct->microSD_UART))
        while(1);
    DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);

    // Wakeup Timer Init
    DAD_Timer_Initialize_ms(FSM_TIMER_PERIOD, FSM_TIMER_HANDLE, &interfaceStruct->FSMtimerConfig);

    // Init buffs with all zeros (memset was not cooperating. Skill issue?)
    int i, j;
    for(i = 0; i < NUM_OF_PORTS; i++){
        for(j = 0; j < SIZE_OF_FFT; j++)
            interfaceStruct->lutStruct.freqBuf[i][j] = 0;
    }

    // Initialize timer for testing write time
    #ifdef FREQ_WRITE_TIME_TEST
    DAD_Timer_Initialize_ms(60000, TIMER_A1_BASE, &(interfaceStruct->FSMtimerConfig));
    DAD_Timer_Start(TIMER_A1_BASE);
    #endif

    //Init Utils
    DAD_Utils_initFreqLUT(&interfaceStruct->lutStruct);
    for(i = 0; i < NUM_OF_PORTS; i++)
        DAD_Calc_InitStruct(&interfaceStruct->calcStruct[i]);
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


void DAD_writeSlowDataToUI(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct)
{
    DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "HOME.s");
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, interfaceStruct->sensorPortOrigin+49);

    // Message conditioning
    if(type == TEMP){
        DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "Val.txt=\"");

        // Write data to HMI
        char val[7] = "";
        sprintf(val, "%d", data);
        DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, val);

        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 'F');
    }
    else if(type == HUM){
        DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "Val2.txt=\"");

        // Write data to HMI
        char val[7] = "";
        sprintf(val, "%d", data);
        DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, val);

        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, '%');
    }
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, '\"');

    // End of transmission
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
}

void DAD_writeSlowDataToMicroSD(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct){
#ifdef WRITE_TO_MICRO_SD
    // Construct message
    char message[MESSAGE_LEN];
    switch(type){
    case TEMP:
        sprintf(message, "p%d, Temp, %dF\n", interfaceStruct->sensorPortOrigin + 1, data);      // port added for debug
        break;
    case HUM:
        sprintf(message, "p%d, Hum, %d%%\n", interfaceStruct->sensorPortOrigin + 1, data);     // port added for debug
        break;
    case VIB:   // Frequency data should be written to microSD using DAD_writeFreqToPeriphs
    case MIC:
    default:
        sprintf(message, "Error: Writing error - %d %%, %d\n", data, interfaceStruct->sensorPortOrigin + 1);   // error catching
    }

    // Write data to microSD
    DAD_microSD_Write(message, &interfaceStruct->microSD_UART);
#endif
}

void DAD_writeMovingAvgToUI(uint16_t data, packetType type, DAD_Interface_Struct* interfaceStruct){
    DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "HOME.s");
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, interfaceStruct->sensorPortOrigin+49);


    // Message conditioning
    if(type == TEMP){
        DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "Avg.txt=\"");

        // Write data to HMI
        char val[7] = "";
        sprintf(val, "%g", DAD_Calc_MovingAvg(data, type, &interfaceStruct->calcStruct[interfaceStruct->sensorPortOrigin]));
        DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, val);

        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 'F');
    }
    else if(type == HUM){
        DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "Avg2.txt=\"");

        // Write data to HMI
        char val[7] = "";
        sprintf(val, "%g", DAD_Calc_MovingAvg(data, type, &interfaceStruct->calcStruct[interfaceStruct->sensorPortOrigin]));
        DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, val);
        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, '%');
    }
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, '\"');

    // End of transmission
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
}

bool DAD_addToFreqBuffer(uint8_t packet[PACKET_SIZE], DAD_Interface_Struct* interfaceStruct){
    uint8_t port = interfaceStruct->sensorPortOrigin;
    uint16_t index = packet[1];                                 // Promote to 16 bit for multiplication

    // TODO condition data
    // Add packet to buffer
    if(index * 2 + 1 < SIZE_OF_FFT && port < NUM_OF_PORTS){
        interfaceStruct->lutStruct.freqBuf[port][index*2] = packet[2];    // just unconditioned data for now
        interfaceStruct->lutStruct.freqBuf[port][index*2+1] = packet[3];  // just unconditioned data for now

        return true;
    }
    // TODO deal with dropped frequency packets
        // Currently assumes no packet is dropped
        // Currently assumes last packet in order is the last received
        // Currently assumes all data in buffer is up to date
    return false;
}

void DAD_writeFreqToPeriphs(packetType type, DAD_Interface_Struct* interfaceStruct){
    uint8_t port = interfaceStruct->sensorPortOrigin;
    DAD_Tell_UI_Whether_To_Expect_FFT(type, interfaceStruct);


    #ifdef FREQ_WRITE_TIME_TEST
    DAD_Timer_Restart(TIMER_A1_BASE,  &interfaceStruct->FSMtimerConfig);
    #endif

    if(interfaceStruct->sensorPortOrigin < NUM_OF_PORTS && DAD_GPIO_getPage(&interfaceStruct->gpioStruct) == port + 1){
        // Write preamble to microSD
        char microSDmsg[17];
        (type == VIB) ? sprintf(microSDmsg, "\np%d, Vib, ", port + 1):
                        sprintf(microSDmsg, "\np%d, Mic, ", port + 1);
        DAD_microSD_Write(microSDmsg, &interfaceStruct->microSD_UART);

        // Write preamble to HMI
        char hmiMsg[15];
        sprintf(hmiMsg, "addt %d,0,%d", HMI_FFT_ID, SIZE_OF_FFT-1);
        DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, hmiMsg);
        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);

        uint8_t data;
        uint16_t i;

        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 0);

        for(i = 0; i < SIZE_OF_FFT-2; i++){
            data = interfaceStruct->lutStruct.freqBuf[port][i];
            DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, data);

            // Write to microSD
            DAD_microSD_Write(DAD_Utils_getMicroSDStr(data, &interfaceStruct->lutStruct), &interfaceStruct->microSD_UART);
        }
        DAD_microSD_Write("FFT End\n\n", &interfaceStruct->microSD_UART);
    }
    // Don't write to HMI
    else if(interfaceStruct->sensorPortOrigin < NUM_OF_PORTS){
        // Write preamble to microSD
        char microSDmsg[17];
        (type == VIB) ? sprintf(microSDmsg, "\np%d, Vib, ", port + 1):
                        sprintf(microSDmsg, "\np%d, Mic, ", port + 1);
        DAD_microSD_Write(microSDmsg, &interfaceStruct->microSD_UART);

        uint8_t data;
        uint16_t i;
        for(i = 0; i < SIZE_OF_FFT-2; i++){
            data = interfaceStruct->lutStruct.freqBuf[port][i];
            DAD_microSD_Write(DAD_Utils_getMicroSDStr(data, &interfaceStruct->lutStruct), &interfaceStruct->microSD_UART);
        }
        DAD_microSD_Write("FFT End\n\n", &interfaceStruct->microSD_UART);
    }

    #ifdef FREQ_WRITE_TIME_TEST
    float timeElapsed;
    timeElapsed = DAD_Timer_Stop(TIMER_A1_BASE,  &interfaceStruct->FSMtimerConfig);
    if(true);
    #endif

    DAD_displayAvgIntensity(type, interfaceStruct);
}

// Checks whether type needs FFT, tells HMI whether to expect FFT
void DAD_Tell_UI_Whether_To_Expect_FFT(packetType type, DAD_Interface_Struct* interfaceStruct){
    #ifdef WRITE_TO_HMI
    // Assert whether HMI expects FFT data
            // HOME.f<sensornumber>.val=<0 or 1, depending on whether we want FFT>
    DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "HOME.f");
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, interfaceStruct->sensorPortOrigin + 49);

    (type == VIB || type == MIC) ? DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, ".val=1") :
            DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, ".val=0");

    // End of transmission
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    #endif
}

void DAD_displayAvgIntensity(packetType type, DAD_Interface_Struct* interfaceStruct){
    // Display avg intensity
    if(type == VIB || type == MIC){
        // Tell HMI to write Freq
        DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "HOME.s");
        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, interfaceStruct->sensorPortOrigin + 49);
        //Update text on home screen
        #ifdef AVG_INTENSITY
        char avgIntensityMsg[20];
        sprintf(avgIntensityMsg, "Val.txt=\"%ddB\"", (int)DAD_Calc_AvgIntensity(interfaceStruct->lutStruct.freqBuf[interfaceStruct->sensorPortOrigin], type));
        DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, avgIntensityMsg);
        #else
        (type == VIB) ? DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "Val.txt=\"VIB\"") :
                DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "Val.txt=\"MIC\"");
        #endif
        // End of transmission
        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
        DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    }
}

#ifdef LOG_INPUT
// Write everything to log file
void DAD_logDebug(uint8_t* packet, DAD_Interface_Struct* interfaceStruct){
    // Open log file
    if(strcmp(interfaceStruct->fileName, "log.txt") != 0){  // Debug - write everything to log
        strcpy(interfaceStruct->fileName, "log.txt");
        DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);
        interfaceStruct->sensorPortOrigin = 255;
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

// Find out what page user is on, find out
void DAD_handle_UI_Feedback(DAD_Interface_Struct* interfaceStruct){
    interfaceStruct->currentHMIPage = DAD_GPIO_getPage(&interfaceStruct->gpioStruct);
    interfaceStruct->startStop = DAD_GPIO_getStartStop(&interfaceStruct->gpioStruct);
}


// Write a command to the UI
void DAD_writeCMDToUI(char* msg, HMI_color color, DAD_Interface_Struct* interfaceStruct){
    // Update message color
    DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "HOME.msg.pco=");
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, (uint8_t)(((uint16_t)color & 0xFF00) >> 8));
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, (uint8_t)(color & 0x00FF));

    // Write message
    DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "HOME.msg.txt");
    DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, msg);
    // End of transmission
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
}
