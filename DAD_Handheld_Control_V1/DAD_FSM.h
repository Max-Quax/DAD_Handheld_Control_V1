/*
 * DAD_FSM.h
 *
 *  Created on: Mar 8, 2023
 *      Author: Max Engel
 */

#ifndef DAD_FSM_H_
#define DAD_FSM_H_

// Interface includes
#include "HAL/DAD_UART.h"

typedef enum {STARTUP, RSA_READ, PROCESS_DATA, WRITE_TO_PERIPH} FSMstate;   // TODO add states as necessary

// Initializes interfaces, timers necessary for FSM use
void DAD_FSM_init();

// Device control
void DAD_FSM_control(FSMstate *state);

// TODO ensure singleton
// TODO low power shutdown

#endif /* DAD_FSM_H_ */
