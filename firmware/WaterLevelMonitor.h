//
//  Water Level Monitor
//
//  Main logic of monitor
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

extern bool WaterLevelMonitor_taskIsDone (void);

extern void WaterLevelMonitor_resume (void);

#endif  // WATERLEVELMONITOR_H
