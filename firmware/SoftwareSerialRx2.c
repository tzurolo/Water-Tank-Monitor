//
//  Software Serial Receiver
//
//   Uses AtMega328P 8-bit Timer 2
//
//  Pin usage:
//      PD2 - serial data in
//
//  How it works:
//    Upon initializaation it sets a pin change interrupt on the serial
//    data input pin. When the interrupt fires a timer is used to clock
//    a state machine to read each bit and push the resulting byte into
//    the queue.
//

#include "SoftwareSerialRx2.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define SERIAL_RX_DDR      DDRD
#define SERIAL_RX_PORT     PORTD
#define SERIAL_RX_INPORT   PIND
#define SERIAL_RX_PIN      PD2
#define SERIAL_RX_PCMSK    PCMSK2
#define SERIAL_RX_PCINT    PCINT18
#define SERIAL_RX_PCIE     PCIE2

// with 8 prescale this 1/4800 second
#define BIT_CLOCK_TIME ((F_CPU / 8) / 4800)

typedef enum RxState_enum {
    rs_idle,
    rs_waitingForStartBit,
    rs_readingDataBits,
    rs_waitingForStopBit
} RxState;
// state variables
static volatile bool isEnabled;
static volatile RxState rxState;
static ByteQueueElement dataByte;
static uint8_t bitMask;
ByteQueue_define(16, rxQueue, static);

static bool rxBit (void)
{
    return (SERIAL_RX_INPORT & (1 << SERIAL_RX_PIN)) != 0;
}

void SoftwareSerialRx2_Initialize (void)
{
    // make rx pin an input and enable pullup
    SERIAL_RX_DDR &= (~(1 << SERIAL_RX_PIN));
    SERIAL_RX_PORT |= (1 << SERIAL_RX_PIN);

    // set up timer2 to fire interrupt 4800 Hz (baud clock)
    TCCR2A = 2; // set CTC mode (WGM21:20)
    TCCR2B = 2; // set CTC mode (WGM22), CS = prescale by 8
    OCR2A = BIT_CLOCK_TIME;
    TCNT2 = 0;  // start the time counter at 0
    TIFR2 |= (1 << OCF2A);  // "clear" the timer compare flag

    // enable pin change interrupts
    PCICR |= (1 << SERIAL_RX_PCIE);

    SoftwareSerialRx2_enable();
}

void SoftwareSerialRx2_enable (void)
{
    if (!isEnabled) {
        rxState = rs_idle;
        // set up pin change interrupt to detect start bit
        SERIAL_RX_PCMSK |= (1 << SERIAL_RX_PCINT);
        isEnabled = true;
    }
}

void SoftwareSerialRx2_disable (void)
{
    if (isEnabled) {
        TIMSK2 &= ~(1 << OCIE2A);   // disable timer compare match interrupt
        SERIAL_RX_PCMSK &= ~(1 << SERIAL_RX_PCINT);
        isEnabled = false;
    }
}

ByteQueue_t* SoftwareSerial_rx2Queue (void)
{
    return &rxQueue;
}

ISR(PCINT2_vect, ISR_BLOCK)
{
    if (!rxBit()) {
        TIFR2 |= (1 << OCF2A);  // "clear" the timer compare flag
        TIMSK2 |= (1 << OCIE2A);// enable timer compare match interrupt
        TCNT2 = 0;  // start the time counter at 0
        // set timer for 1/2 bit time to get in the middle of the start bit
        OCR2A = BIT_CLOCK_TIME / 2;

        // disable this interrupt
        SERIAL_RX_PCMSK &= ~(1 << SERIAL_RX_PCINT);
        rxState = rs_waitingForStartBit;
    }
}

ISR(TIMER2_COMPA_vect, ISR_BLOCK)
{
    switch (rxState) {
        case rs_waitingForStartBit :
            if (!rxBit()) {
                // rx bit is low - got start bit
                dataByte = 0;
                bitMask = 1;
                OCR2A = BIT_CLOCK_TIME;
                rxState = rs_readingDataBits;
            } else {
                // expected low for start bit but didn't get it
                TIMSK2 &= ~(1 << OCIE2A);// disable timer compare match interrupt
                // re-enable pin change interrupt
                SERIAL_RX_PCMSK |= (1 << SERIAL_RX_PCINT);
            }
            break;
        case rs_readingDataBits :
            if (rxBit()) {
                dataByte |= bitMask;
            }
            if (bitMask == 0x80) {
                rxState = rs_waitingForStopBit;
            } else {
                bitMask <<= 1;
            }
            break;
        case rs_waitingForStopBit :
            if (rxBit()) {
                // got stop bit.
                ByteQueue_push(dataByte, &rxQueue);
            }
            TIMSK2 &= ~(1 << OCIE2A);// disable timer compare match interrupt
            rxState = rs_idle;

            // re-enable pin change interrupt
            SERIAL_RX_PCMSK |= (1 << SERIAL_RX_PCINT);
            break;
        default :
            break;
    }
}
