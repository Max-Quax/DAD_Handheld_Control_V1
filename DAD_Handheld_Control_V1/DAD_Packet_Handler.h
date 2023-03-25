/*
 * DAD_Buffer_Handler.h
 *
 *  Created on: Mar 25, 2023
 *      Author: Max
 *
 *      At a high level, packets are pulled from the RSA (bluetooth) buffer and processed here.
 *      If this project is ever implemented in another system, this is the lowest level that is expected to be reused.
 *      All lower levels are geared towards interacting with our specific hardware
 *
 */

// TODO timestamp
// TODO throttle ui
// TODO Avg and moving average
// TODO talkback to RSA
// TODO raise the alarm when sensor hasn't said anything in a while

#ifndef DAD_PACKET_HANDLER_H_
#define DAD_PACKET_HANDLER_H_

// Standard includes
#include <stdio.h>
#include <stdlib.h>

// Util includes
#include <DAD_Utils/DAD_Interface_Handler.h>

// Read all elements from RSA UART buffer to peripherals
void handleRSABuffer(DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Handle an individual packet
static void handlePacket(DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Report to HMI that sensor has been disconnected
static void handleDisconnect(uint8_t port, packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Processes data packet, sends data to both HMI (UIy and microSD
static void handleData(uint8_t port, packetType type, uint8_t packet[PACKET_SIZE+1], DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Handles packets of "connected, no data" type
static void handle_CON_ND(packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

// Handles packets of "message" type
static void handleMessage(packetType type, DAD_Interface_Struct* interfaceStruct, DAD_utils_struct* utilsStruct);

#endif /* DAD_PACKET_HANDLER_H_ */
