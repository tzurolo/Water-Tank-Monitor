//
//  Water Level Monitor
//
//  Main logic of monitor
//      use the TCPIP Console to communicate with the remote host
//
//
#include "WaterLevelMonitor.h"

#include "SystemTime.h"
#include "EEPROMStorage.h"
//#include "Thingspeak.h"
#include "CommandProcessor.h"
#include "Console.h"
#include "StringUtils.h"
#include "CellularComm_SIM800.h"
#include "TCPIPConsole.h"
#include "CommandProcessor.h"

typedef enum SendDataStatus_enum {
    sds_sending,
    sds_completedSuccessfully,
    sds_completedFailed
} SendDataStatus;
// Send data subtask states
typedef enum WaterLevelMonitorState_enum {
    wlms_initial,
    wlms_resuming,
    wlms_waitingForConnection,
    wlms_sendingData,
    wlms_waitingForHostData,
    wlms_waitingForCellularCommDisable,
    wlms_poweringDown,
    wlms_done
} WaterLevelMonitorState;

// state variables
static const char tokenDelimiters[] = " \n\r";
static WaterLevelMonitorState wlmState;
static SendDataStatus sendDataStatus;
static SystemTime_t powerdownDelay;
static bool gotDataFromHost;

static void dataSender (void)
{
    CharString_define(80, dataToSend);
    CommandProcessor_createStatusMessage(&dataToSend);
    CellularTCPIP_writeDataCS(&dataToSend);
}

static void TCPIPSendCompletionCallaback (
    const bool success)
{
    sendDataStatus = success
        ? sds_completedSuccessfully
        : sds_completedFailed;   
}

void IPDataCallback (
    const CharString_t *ipData)
{
    gotDataFromHost = true;
    CommandProcessor_processCommand(CharString_cstr(ipData), "", "");
}

void WaterLevelMonitor_Initialize (void)
{
    wlmState = wlms_initial;
    // wlmState = wlms_done;
}

void WaterLevelMonitor_task (void)
{
    switch (wlmState) {
        case wlms_initial :
            wlmState = wlms_resuming;
            break;
        case wlms_resuming :
            // turn on peripheral power
            DDRC |= (1 << PC5);
            PORTC |= (1 << PC5);

            gotDataFromHost = false;
            CellularComm_Enable();
            TCPIPConsole_setDataReceiver(IPDataCallback);
            TCPIPConsole_enable(false);

            wlmState = wlms_waitingForConnection;
            break;
        case wlms_waitingForConnection :
            if (TCPIPConsole_readyToSend()) {
                sendDataStatus = sds_sending;
                TCPIPConsole_sendData(dataSender, TCPIPSendCompletionCallaback);
                wlmState = wlms_sendingData;
            }
            break;
        case wlms_sendingData :
            switch (sendDataStatus) {
                case sds_sending :
                    break;
                case sds_completedSuccessfully :
                    wlmState = wlms_waitingForHostData;
                    break;
                case sds_completedFailed :
                    wlmState = wlms_waitingForHostData;
                    break;
            }
            break;
        case wlms_waitingForHostData :
            if (gotDataFromHost) {
                TCPIPConsole_disable(false);
                CellularComm_Disable();
                wlmState = wlms_waitingForCellularCommDisable;
            }
            break;
        case wlms_waitingForCellularCommDisable :
            if (!CellularComm_isEnabled()) {
                // give it another two seconds of power to properly close the connection
                SystemTime_futureTime(200, &powerdownDelay);
                wlmState = wlms_poweringDown;
            }
            break;
        case wlms_poweringDown :
            if (SystemTime_timeHasArrived(&powerdownDelay)) {
                // power down peripherals
                DDRC |= (1 << PC5);
                PORTC &= ~(1 << PC5);

                wlmState = wlms_done;
            }
            break;
        case wlms_done :
            break;
    }
}

bool WaterLevelMonitor_taskIsDone (void)
{
    return wlmState == wlms_done;
}

void WaterLevelMonitor_resume (void)
{
    wlmState = wlms_resuming;
}