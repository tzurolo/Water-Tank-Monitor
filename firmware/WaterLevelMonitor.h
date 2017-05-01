//
//  LED Lighting UPS Monitor
//
//  Continually polls the LED Lighting UPS for current status and
//  sends updates through the TCPIP Console
//
#ifndef WATERLEVELMONITOR_H
#define WATERLEVELMONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "CharString.h"

// sets up control pins. called once at power-up
extern void WaterLevelMonitor_Initialize (void);

// called in each iteration of the mainloop
extern void WaterLevelMonitor_task (void);

#endif  // WATERLEVELMONITOR_H
