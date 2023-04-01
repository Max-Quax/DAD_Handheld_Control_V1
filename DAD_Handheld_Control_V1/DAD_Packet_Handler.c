/*
 * DAD_Packet_Handler.c
 *
 *  Created on: Mar 25, 2023
 *      Author: Max
 */

#include <DAD_Packet_Handler.h>

void handleRSABuffer(DAD_Interface_Struct* interfaceStruct){
    // Set GPIO flags for use in this run
    DAD_handle_UI_Feedback(interfaceStruct);

    #ifdef RECEIVE_HMI_FEEDBACK
    if(DAD_UART_HasChar(&interfaceStruct->HMI_RX_UART_struct))
        DAD_handle_UI_Feedback(interfaceStruct);
    #endif

    // Write everything to log
    #ifdef WRITE_TO_ONLY_ONE_FILE
    strcpy(interfaceStruct->fileName, "log.txt");
    DAD_microSD_openFile(interfaceStruct->fileName, &interfaceStruct->microSD_UART);
    #endif

    // Read individual packets from buffer
    while(PACKET_SIZE < DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct)){
        handlePacket(interfaceStruct);
    }

    // Flush out remaining chars from read buffer.
    while(DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct) > 0){
        DAD_UART_GetChar(&interfaceStruct->RSA_UART_struct);
    }

    // Handle UI output timers
    #ifdef THROTTLE_UI_OUTPUT
    if(DAD_Timer_Has_Finished(UI_UPDATE_TIMER_HANDLE))
        DAD_Timer_Restart(UI_UPDATE_TIMER_HANDLE, &interfaceStruct->UIupdateTimer);
    // If UI FFT output timer has expired, run all queued FFTs
    if(DAD_Timer_Has_Finished(UI_FFT_UPDATE_TIMER_PERIOD)){
        int fftToRun;
        for(fftToRun = 0; fftToRun < NUM_OF_PORTS; fftToRun++){
            if(interfaceStruct->freqStructs[fftToRun].requestedWriteToUI){
                DAD_writeFreqToUI(interfaceStruct->freqStructs[fftToRun].type, interfaceStruct);
                interfaceStruct->freqStructs[fftToRun].requestedWriteToUI = false;
            }
        }
        DAD_Timer_Restart(UI_FFT_UPDATE_TIMER_PERIOD, &interfaceStruct->UIFFTupdateTimer);
    }
    #endif

    #ifdef LOG_INPUT
    char* message = "EndBuf\n\n";
    DAD_UART_Write_Str(&interfaceStruct->microSD_UART, message);
    #endif

    // Set GPIO flags for state change
    DAD_handle_UI_Feedback(interfaceStruct);
}

static void handlePacket(DAD_Interface_Struct* interfaceStruct)
{
    // Construct packet
    uint8_t packet[PACKET_SIZE + 1];
    if(!DAD_constructPacket(packet, &interfaceStruct->RSA_UART_struct)){                                // If construct packet fails
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


    #ifdef LOG_INPUT
    // Debug - Log Packet
    DAD_logDebug(packet, interfaceStruct);
    if(packet[3] >= 253 ){
        int i = 0;  //just something to put a breakpoint on
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
            handleDisconnect(port, type, interfaceStruct);
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


static void handleDisconnect(uint8_t port, packetType type, DAD_Interface_Struct* interfaceStruct)
{
    #ifdef WRITE_TO_HMI
    // Write to HMI
    // Report sensor disconnected to HMI
    DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "HOME.s");
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, interfaceStruct->currentPort + 49);
    DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "Val.txt=\"NONE\"");
    // End of transmission
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);

    // Report Stop Sending FFT
        // HOME.f<sensornumber>.val=0
    DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, "HOME.f");
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, interfaceStruct->currentPort + 49);
    DAD_UART_Write_Str(&interfaceStruct->HMI_TX_UART_struct, ".val=0");
    // End of transmission
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    DAD_UART_Write_Char(&interfaceStruct->HMI_TX_UART_struct, 255);
    #endif

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


#ifndef THROTTLE_UI_OUTPUT
// Processes data packet, sends data to peripherals
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
            DAD_writeToUI(data, type, interfaceStruct);
            DAD_writeToMicroSD(data, type, interfaceStruct);
            break;
        case HUM:
            // TODO condition data
            data = ((packet[1] << 8) + packet[2]) % 110;
            DAD_writeToUI(data, type, interfaceStruct);
            DAD_writeToMicroSD(data, type, interfaceStruct);
            break;
        case VIB:
            // Fall through to mic. Same code
        case MIC:
            // Add packet to buffer,
            DAD_addToFreqBuffer(packet, interfaceStruct);
            // If second to last packet has been received, write to peripherals
            if(packet[1]*2 == SIZE_OF_FFT - 4 && DAD_Timer_Has_Finished(UI_UPDATE_TIMER_HANDLE))              // Note - second to last packet bc "last packet" would require receiving a byte of 0xFF, which would result in an invalid packet
                DAD_writeFreqToPeriphs(type, interfaceStruct);
            break;
    }
}
#endif


#ifdef THROTTLE_UI_OUTPUT
// Processes data packet, sends data to peripherals
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
            if(DAD_Timer_Has_Finished(UI_UPDATE_TIMER_HANDLE))
                DAD_writeToUI(data, type, interfaceStruct);
            DAD_writeToMicroSD(data, type, interfaceStruct);
            break;
        case HUM:
            // TODO condition data
            data = ((packet[1] << 8) + packet[2]) % 110;
            if(DAD_Timer_Has_Finished(UI_UPDATE_TIMER_HANDLE))
                DAD_writeToUI(data, type, interfaceStruct);
            DAD_writeToMicroSD(data, type, interfaceStruct);
            break;
        case VIB:
            // Fall through to mic. Same code
        case MIC:
            // Add packet to buffer,
            DAD_addToFreqBuffer(packet, interfaceStruct);
            // If second to last packet has been received, write to peripherals
            if(packet[1]*2 == SIZE_OF_FFT - 4)              // Note - second to last packet bc "last packet" would require receiving a byte of 0xFF, which would result in an invalid packet
                // Always write data to microSD
                DAD_writeFreqToMicroSD(type, interfaceStruct);
                // Request a write to UI. Granted at end of buffer handling if timer has expired
                interfaceStruct->freqStructs[port].requestedWriteToUI = true;
                interfaceStruct->freqStructs[port].type = type;
            break;
    }
}
#endif


// Handles packets of "connected, no data" type
static void handle_CON_ND(packetType type, DAD_Interface_Struct* interfaceStruct){
    // TODO check sensor still responding
    DAD_Tell_UI_Whether_To_Expect_FFT(type, interfaceStruct);
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

// Handles incoming packets when stop is asserted
void handleStop(DAD_Interface_Struct* interfaceStruct){
    #ifdef GET_GPIO_FEEDBACK

    uint8_t packet[PACKET_SIZE + 1];
    packetStatus PKstatus;
    packetType type;
    uint8_t port;

    // Read individual packets from buffer
    while(PACKET_SIZE < DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct)){
        // Construct packet
        if(!DAD_constructPacket(packet, &interfaceStruct->RSA_UART_struct)){                                // If construct packet fails
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
        #ifdef LOG_INPUT
        // Debug - Log Packet
        DAD_logDebug(packet, interfaceStruct);
        #endif

        // Interpret packet
        PKstatus = (packetStatus)((packet[0] & STATUS_MASK) >> 3);
        type = (packetType)(packet[0] & PACKET_TYPE_MASK);
        port = (packet[0] & PORT_MASK) >> 5;

        #ifdef WRITE_TO_ONLY_ONE_FILE
        interfaceStruct->currentPort = port;
        #endif

        // Deal with packet
        switch(PKstatus)
        {
            case DISCON:
                handleDisconnect(port, type, interfaceStruct);
                break;
            case CON_D:
                // Throw out data.
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

    // Flush out remaining chars from read buffer.
    while(DAD_UART_NumCharsInBuffer(&interfaceStruct->RSA_UART_struct) > 0){
        DAD_UART_GetChar(&interfaceStruct->RSA_UART_struct);
    }

    // Update GPIO flags
    DAD_handle_UI_Feedback(interfaceStruct);
    #endif
}


