#define WITH_HAL


#ifdef WITH_HAL
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// HAL includes
#include <HAL/DAD_UART.h>

// Control software includes
#include "DAD_Interface_Handler.h"
#include "DAD_FSM.h"

// TODO test that it works
// TODO integrate HAL
    // done, untested UART
        // TODO integrate buffer
        // TODO implement transmits
    // TODO Timers
    // TODO callback functinality?


int main(void)
{
    /* Halting WDT  */
    MAP_WDT_A_holdTimer();

    // Application loop
    FSMstate state = STARTUP;
    while(true){
        DAD_FSM_control(&state);    // Handle everything
        MAP_GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);    // Debug - check that it's not hung up
        MAP_PCM_gotoLPM0();         // Go back to sleep until next interrupt
    }
}

#endif //WITH_HAL

#ifndef WITH_HAL
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// HAL includes
#include <HAL/DAD_UART.h>

int byteCount = 0;
bool started = false;
uint8_t bytes[4];
typedef enum {DISCON, CON_D, CON_ND, MSG} packetStatus;
typedef enum {TEMP, HUM, VIB, MIC, LOWBAT, ERR, STOP, START} packetType;

const eUSCI_UART_ConfigV1 uartConfig =
{
        EUSCI_A_UART_CLOCKSOURCE_SMCLK,          // SMCLK Clock Source
        78,                                     // BRDIV = 78
        2,                                       // UCxBRF = 2
        0,                                       // UCxBRS = 0
        EUSCI_A_UART_NO_PARITY,                  // No Parity
        EUSCI_A_UART_LSB_FIRST,                  // LSB First
        EUSCI_A_UART_ONE_STOP_BIT,               // One stop bit
        EUSCI_A_UART_MODE,                       // UART mode
        EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION,  // Oversampling
        EUSCI_A_UART_8_BIT_LEN                  // 8 bit data length
};

char *intToString(int num) {
    char str[5];
    sprintf(str, "%d", num);
    return str;
}

void writeSensorData(int port, int data, packetType type)
{
    MAP_UART_transmitData(EUSCI_A2_BASE, 'H');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'O');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'M');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'E');
    MAP_UART_transmitData(EUSCI_A2_BASE, '.');
    MAP_UART_transmitData(EUSCI_A2_BASE, 's');
    MAP_UART_transmitData(EUSCI_A2_BASE, port+49);
    MAP_UART_transmitData(EUSCI_A2_BASE, 'V');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'a');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'l');
    MAP_UART_transmitData(EUSCI_A2_BASE, '.');
    MAP_UART_transmitData(EUSCI_A2_BASE, 't');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'x');
    MAP_UART_transmitData(EUSCI_A2_BASE, 't');
    MAP_UART_transmitData(EUSCI_A2_BASE, '=');
    MAP_UART_transmitData(EUSCI_A2_BASE, '"');
    char *val = intToString(data);
    int i = 0;
    while (val[i] != '\0')
    {
        MAP_UART_transmitData(EUSCI_A2_BASE, val[i]);
        i++;
    }
    if(type == TEMP)
    {
        MAP_UART_transmitData(EUSCI_A2_BASE, 'F');
    }
    else if(type == HUM)
    {
        MAP_UART_transmitData(EUSCI_A2_BASE, '%');
    }
    MAP_UART_transmitData(EUSCI_A2_BASE, '"');
    MAP_UART_transmitData(EUSCI_A2_BASE, 255);
    MAP_UART_transmitData(EUSCI_A2_BASE, 255);
    MAP_UART_transmitData(EUSCI_A2_BASE, 255);
    return;
}

void sensorDisconnect(int port)
{
    MAP_UART_transmitData(EUSCI_A2_BASE, 'H');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'O');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'M');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'E');
    MAP_UART_transmitData(EUSCI_A2_BASE, '.');
    MAP_UART_transmitData(EUSCI_A2_BASE, 's');
    MAP_UART_transmitData(EUSCI_A2_BASE, port+49);
    MAP_UART_transmitData(EUSCI_A2_BASE, 'V');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'a');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'l');
    MAP_UART_transmitData(EUSCI_A2_BASE, '.');
    MAP_UART_transmitData(EUSCI_A2_BASE, 't');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'x');
    MAP_UART_transmitData(EUSCI_A2_BASE, 't');
    MAP_UART_transmitData(EUSCI_A2_BASE, '=');
    MAP_UART_transmitData(EUSCI_A2_BASE, '"');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'N');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'O');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'N');
    MAP_UART_transmitData(EUSCI_A2_BASE, 'E');
    MAP_UART_transmitData(EUSCI_A2_BASE, '"');
    MAP_UART_transmitData(EUSCI_A2_BASE, 255);
    MAP_UART_transmitData(EUSCI_A2_BASE, 255);
    MAP_UART_transmitData(EUSCI_A2_BASE, 255);
    return;
}

void handlePacket()
{
    packetStatus status = (packetStatus)((bytes[0] & 24) >> 3);
    packetType type = (packetType)(bytes[0] & 7);
    int port = (bytes[0] & 224) >> 5;
    int data;
    //MAP_UART_transmitData(EUSCI_A2_BASE, b1);
    //MAP_UART_transmitData(EUSCI_A2_BASE, b2);
    //MAP_UART_transmitData(EUSCI_A2_BASE, b3);
    //MAP_UART_transmitData(EUSCI_A2_BASE, b4);
    switch(status)
    {
        case DISCON:
            sensorDisconnect(port);
            break;
        case CON_D:
            switch(type)
            {
                case TEMP:
                    data = (bytes[1] << 8) + bytes[2];
                    writeSensorData(port, data%100, type);
                    break;
                case HUM:
                    data = (bytes[1] << 8) + bytes[2];
                    writeSensorData(port, data%100, type);
                    break;
                case VIB:
                    break;
                case MIC:
                    break;
            }
            break;
        case CON_ND:
            break;
        case MSG:
            break;
    }
}

void packetIntake(uint8_t byte)
{
    if(byte == 255)
    {
        //UART_transmitData(EUSCI_A2_BASE, bytes[0]);
        //UART_transmitData(EUSCI_A2_BASE, bytes[1]);
        //UART_transmitData(EUSCI_A2_BASE, bytes[2]);
        //UART_transmitData(EUSCI_A2_BASE, bytes[3]);
        handlePacket();
    }
    else
    {
        bytes[0] = bytes[1];
        bytes[1] = bytes[2];
        bytes[2] = bytes[3];
        bytes[3] = byte;
    }

}

int main(void)
{
    /* Halting WDT  */
    MAP_WDT_A_holdTimer();

    /* Selecting P1.2 and P1.3 & P3.2 and P3.3 in UART mode */
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1,
            GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P3,
            GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);

    /* Setting DCO to 12MHz */
    CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_12);

    //![Simple UART Example]
    /* Configuring UART Module */
    MAP_UART_initModule(EUSCI_A0_BASE, &uartConfig);
    MAP_UART_initModule(EUSCI_A2_BASE, &uartConfig);

    /* Enable UART module */
    MAP_UART_enableModule(EUSCI_A0_BASE);
    MAP_UART_enableModule(EUSCI_A2_BASE);

    /* Enabling interrupts */
    MAP_UART_enableInterrupt(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
    MAP_Interrupt_enableInterrupt(INT_EUSCIA0);
    //MAP_Interrupt_enableSleepOnIsrExit();
    MAP_Interrupt_enableMaster();

    while(1)
    {
        MAP_PCM_gotoLPM0();
    }
}

/* EUSCI A0 UART ISR - Echoes data back to PC host */
void EUSCIA0_IRQHandler(void)
{
    uint32_t status = MAP_UART_getEnabledInterruptStatus(EUSCI_A0_BASE);

    if(status & EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG)
    {
        packetIntake(UART_receiveData(EUSCI_A0_BASE));
        //MAP_UART_transmitData(EUSCI_A2_BASE, UART_receiveData(EUSCI_A0_BASE));
    }
}
#endif // without hal
