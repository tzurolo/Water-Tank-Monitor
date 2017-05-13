//
//  Battery Monitor
//
//  Monitors the state of the UPS's battery
//
#ifndef BATTERYMONITOR_H
#define BATTERYMONITOR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    bs_unknown,
    bs_lowVoltage,          // low
    bs_goodVoltage,         // good
    bs_fullVoltage          // full
} BatteryMonitor_batteryStatus;

extern void BatteryMonitor_Initialize (void);

extern bool BatteryMonitor_haveValidSample (void);

extern BatteryMonitor_batteryStatus BatteryMonitor_currentStatus (void);

// voltage in units of 1/100 volt
extern int16_t BatteryMonitor_currentVoltage (void);

extern void BatteryMonitor_task (void);

#endif      // BATTERYMONITOR_H