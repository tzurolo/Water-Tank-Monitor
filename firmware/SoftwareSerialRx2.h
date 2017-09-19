//
//  Software Serial - Receiver
//
//  What it does:
//      Software implementation of UART Receiver
//      Currently hardcoded to listen for 8N1 on pin PD2 at 4800 baud
//      Has an 80-byte queue.
//
//  How to use it:
//     Call SoftwareSerialRx2_Initialize() once at the beginning of the
//     program (upon powerup).
//     During the program you can get the byte queue using
//     SoftwareSerial_rx2Queue() to check for and read incoming bytes.
//
//  Hardware resources used:
//    Serial data in - PA5
//    AtMega328P 16-bit Timer 2
//
#ifndef SOFTWARESERIALRX2_H
#define SOFTWARESERIALRX2_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "ByteQueue.h"

// comes up enabled by default
extern void SoftwareSerialRx2_Initialize (void);

extern void SoftwareSerialRx2_enable (void);
extern void SoftwareSerialRx2_disable (void);

extern ByteQueue_t* SoftwareSerial_rx2Queue (void);

#if BYTEQUEUE_HIGHWATERMARK_ENABLED
extern void SoftwareSerialRx2_reportHighwater (void);
#endif

#endif  /* SOFTWARESERIALRX2_H */

