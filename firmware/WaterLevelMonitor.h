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

typedef enum WaterLevelMonitorState_enum {
    wlms_initial,
    wlms_resuming,
    wlms_waitingForSensorData,
    wlms_waitingForConnection,
    wlms_sendingSampleData,
    wlms_waitingForHostCommand,
    wlms_waitingForReadyToSendReply,
    wlms_sendingReplyData,
    wlms_waitingForCellularCommDisable,
    wlms_poweringDown,
    wlms_done
} WaterLevelMonitorState;

// sets up control pins. called once at power-up
extern void WaterLevelMonitor_Initialize (void);

// called in each iteration of the mainloop
extern void WaterLevelMonitor_task (void);

extern bool WaterLevelMonitor_taskIsDone (void);

extern void WaterLevelMonitor_resume (void);

extern WaterLevelMonitorState WaterLevelMonitor_state (void);

#endif  // WATERLEVELMONITOR_H
