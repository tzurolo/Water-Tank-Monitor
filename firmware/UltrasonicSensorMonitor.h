//
//  Ultrasonic Sensor Monitor
//
//  Monitors the serial data feed from the MB7389 ultrasonic distance sensor
//
#ifndef ULTRASONICSENSORMONITOR_H
#define ULTRASONICSENSORMONITOR_H

#include <stdint.h>
#include <stdbool.h>

extern void UltrasonicSensorMonitor_Initialize (void);

extern bool UltrasonicSensorMonitor_haveValidSample (void);

// distance in units of CM
extern int16_t UltrasonicSensorMonitor_currentDistance (void);

extern void UltrasonicSensorMonitor_task (void);

#endif      // ULTRASONICSENSORMONITOR_H