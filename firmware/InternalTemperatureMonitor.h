//
//  Internal Temperature Monitor
//
//  Monitors the temperature of the AtMega328P using its internal temperature sensor
//
#ifndef INTERNALTEMPMONITOR_H
#define INTERNALTEMPMONITOR_H

#include <stdint.h>

extern void InternalTemperatureMonitor_Initialize (void);

// temperature in units of degree C
extern int16_t InternalTemperatureMonitor_currentTemperature (void);

extern void InternalTemperatureMonitor_task (void);

#endif      // INTERNALTEMPMONITOR_H