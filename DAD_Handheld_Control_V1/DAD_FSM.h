/*
 * DAD_FSM.h
 *
 *  Created on: Mar 8, 2023
 *      Author: Max
 */

#ifndef DAD_FSM_H_
#define DAD_FSM_H_

// Interface includes
#include "HAL/DAD_UART.h"

#define FSM_TIMER_HANDLE TIMER_A0_BASE
#define FSM_TIMER_PERIOD 500            // Period in ms

typedef enum {STARTUP, RSA_READ, PROCESS_DATA, WRITE_TO_PERIPH} FSMstate;   // TODO add states as necessary

// Initializes interfaces, timers necessary for FSM use
void DAD_FSM_init();

// Device control
void DAD_FSM_control(FSMstate *state);

// TODO ensure singleton
// TODO low power shutdown

#endif /* DAD_FSM_H_ */
