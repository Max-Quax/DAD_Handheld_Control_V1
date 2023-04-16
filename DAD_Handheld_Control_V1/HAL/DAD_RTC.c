/*
 * DAD_RTC.c
 *
 *  Created on: Apr 9, 2023
 *      Author: Max
 */

#include <DAD_RTC.h>

// TODO test/debug

// Init/start RTC
void DAD_RTC_init(RTC_C_Calendar* calendarStruct){
    MAP_CS_initClockSignal(CS_BCLK, CS_REFOCLK_SELECT, CS_CLOCK_DIVIDER_1);

    MAP_GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_PJ,
            GPIO_PIN0 | GPIO_PIN1, GPIO_PRIMARY_MODULE_FUNCTION);
    CS_setExternalClockSourceFrequency(32000,48000000);
    CS_startLFXT(CS_LFXT_DRIVE3);


    MAP_RTC_C_initCalendar(calendarStruct, RTC_C_FORMAT_BINARY);
    MAP_RTC_C_startClock();
}

// Return date/time as a string
void DAD_RTC_getTime(char* currentTime){
    RTC_C_Calendar cal = MAP_RTC_C_getCalendarTime();

    // DD/MM/YY HR:MIN:SEC
    sprintf(currentTime, "%02d/%02d/%04d %02d:%02d:%02d",
            cal.dayOfmonth, cal.month, cal.year, cal.hours, cal.minutes, cal.seconds);
}
