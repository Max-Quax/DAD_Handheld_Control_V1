/*
 * DAD_RTC.h
 *
 *  Created on: Apr 9, 2023
 *      Author: Max
 */

#ifndef DAD_RTC_H_
#define DAD_RTC_H_

// DriverLib Includes
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

// Std includes
#include <stdio.h>

#define RTC_OUTPUT_STR_LEN 25   // Length of RTC string otput

// Init/start RTC
void DAD_RTC_init(RTC_C_Calendar* calendarStruct);

// Return date/time as a string
void DAD_RTC_getTime(char* currentTime);

#endif /* DAD_RTC_H_ */
