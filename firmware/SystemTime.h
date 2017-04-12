//
//  SystemTime
//
//  Counts seconds since last reset
//  Resets the watchdog timer
//
//  Uses AtMega328P 16 bit timer/counter 1
//
#ifndef SYSTEMTIME_H
#define SYSTEMTIME_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "CharString.h"

#define SYSTEMTIME_TICKS_PER_SECOND 4800

typedef uint32_t SystemTime_t;

// prototype for functions that clients supply to
// get notification when a tick occurs
typedef void (*SystemTime_TickNotification)(void);

extern void SystemTime_Initialize (void);

extern void SystemTime_registerForTickNotification (
    SystemTime_TickNotification notificationFcn);

extern void SystemTime_getCurrentTime (
    SystemTime_t *curTime);

// initializes futureTime to the current time plus
// the given number of seconds
extern void SystemTime_futureTime (
    const int secondsFromNow,
    SystemTime_t* futureTime);

// returns true if the current time is >= the given time
extern bool SystemTime_timeHasArrived (
    const SystemTime_t* time);

extern void SystemTime_commenceShutdown (void);
extern bool SystemTime_shuttingDown (void);

extern void SystemTime_task (void);

// writes current time as HH:MM:SS
extern void SystemTime_appendCurrentToString (
    CharString_t* timeString);

#endif  // SYSTEMTIME_H
