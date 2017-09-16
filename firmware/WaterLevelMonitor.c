//
//  Water Level Monitor
//
//  Main logic of monitor
//      use the TCPIP Console to communicate with the remote host
//
//  Pin usage:
//      PC1 - power switch
//
#include "WaterLevelMonitor.h"

#include "SystemTime.h"
#include "EEPROMStorage.h"
//#include "Thingspeak.h"
#include "BatteryMonitor.h"
#include "InternalTemperatureMonitor.h"
#include "UltrasonicSensorMonitor.h"
#include "CommandProcessor.h"
#include "Console.h"
#include "StringUtils.h"
#include "CellularComm_SIM800.h"
#include "TCPIPConsole.h"
#include "CommandProcessor.h"
#include "SampleHistory.h"

// Send data subtask states
typedef enum SendDataStatus_enum {
    sds_sending,
    sds_completedSuccessfully,
    sds_completedFailed
} SendDataStatus;

// state variables
static const char tokenDelimiters[] = " \n\r";
static WaterLevelMonitorState wlmState;
static SendDataStatus sendDataStatus;
static SystemTime_t time;   // used to measure how long it took to get a connection,
                            // and for the powerdown delay future time
static bool gotDataFromHost;
static SampleHistory_define(30, sampleHistory);
static int16_t dataSenderSampleIndex;

static bool dataSender (void)
{
    bool sendComplete = false;

    if (dataSenderSampleIndex == -1) {
        // send per-post data
        CharString_define(20, dataToSend);
        //CommandProcessor_createStatusMessage(&dataToSend);
        SystemTime_t curTime;
        SystemTime_getCurrentTime(&curTime);
        const int16_t secondsToConnect = SystemTime_diffSec(&curTime, &time);
        CharString_copyP(PSTR("C"), &dataToSend);
        StringUtils_appendDecimal(secondsToConnect, 1, 0, &dataToSend);
        CharString_appendC('B', &dataToSend);
        StringUtils_appendDecimal(BatteryMonitor_currentVoltage(), 1, 0, &dataToSend);
        CharString_appendC('R', &dataToSend);
        StringUtils_appendDecimal((int)CellularComm_registrationStatus(), 1, 0, &dataToSend);
        CharString_appendC('Q', &dataToSend);
        StringUtils_appendDecimal(CellularComm_SignalQuality(), 1, 0, &dataToSend);
        CellularTCPIP_writeDataCS(&dataToSend);
        ++dataSenderSampleIndex;
    } else {
        // send sample data
        const SampleHistory_Sample* sample =
            SampleHistory_getAt(dataSenderSampleIndex, &sampleHistory);
        CharString_define(20, dataToSend);
        CharString_copyP(PSTR(";S"), &dataToSend);
        StringUtils_appendDecimal(sample->relSampleTime, 1, 0, &dataToSend);
        CharString_appendC('W', &dataToSend);
        StringUtils_appendDecimal(sample->waterDistance, 1, 0, &dataToSend);
        CharString_appendC('T', &dataToSend);
        StringUtils_appendDecimal(sample->temperature, 1, 0, &dataToSend);

        ++dataSenderSampleIndex;
        if (dataSenderSampleIndex >= ((int16_t)SampleHistory_length(&sampleHistory))) {
            CharString_appendC('Z', &dataToSend);
            sendComplete = true;
        }
        CellularTCPIP_writeDataCS(&dataToSend);
    }

    return sendComplete;
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
    SampleHistory_clear(&sampleHistory);
}

void WaterLevelMonitor_task (void)
{
    switch (wlmState) {
        case wlms_initial :
            wlmState = wlms_resuming;
            break;
        case wlms_resuming :
            // turn on peripheral power
            DDRC |= (1 << PC1);
            PORTC |= (1 << PC1);

            // determine if it's time to log to server
            const uint16_t sampleInterval = EEPROMStorage_sampleInterval();
            const uint16_t logInterval = EEPROMStorage_LoggingUpdateInterval();
            SystemTime_getCurrentTime(&time);
            if (((time.seconds + (sampleInterval / 2)) % logInterval) < sampleInterval) {
                // time to log to server
                gotDataFromHost = false;
                CellularComm_Enable();
                TCPIPConsole_setDataReceiver(IPDataCallback);
                TCPIPConsole_enable(false);
            }
            wlmState = wlms_waitingForSensorData;
            break;
        case wlms_waitingForSensorData :
            if (BatteryMonitor_haveValidSample() &&
                InternalTemperatureMonitor_haveValidSample() &&
                UltrasonicSensorMonitor_haveValidSample()) {
                CharString_define(10, usMsg);
                CharString_appendP(PSTR("U:"), &usMsg);
                StringUtils_appendDecimal(UltrasonicSensorMonitor_currentDistance(), 3, 1, &usMsg);
                Console_printCS(&usMsg);
                // log sensor data
                SampleHistory_Sample sample;
                sample.relSampleTime = 0;
                sample.temperature =
                    (uint8_t)InternalTemperatureMonitor_currentTemperature();
                sample.waterDistance = UltrasonicSensorMonitor_currentDistance();
                SampleHistory_insertSample(&sample, &sampleHistory);

                if (TCPIPConsole_isEnabled()) {
                    wlmState = wlms_waitingForConnection;
                } else {
                    // just a sample interval
                    SystemTime_futureTime(50, &time);
                    wlmState = wlms_poweringDown;
                }
            }
            break;
        case wlms_waitingForConnection :
            if (TCPIPConsole_readyToSend()) {
                sendDataStatus = sds_sending;
                dataSenderSampleIndex = -1; // start with per-post data
                TCPIPConsole_sendData(dataSender, TCPIPSendCompletionCallaback);
                wlmState = wlms_sendingData;
            }
            break;
        case wlms_sendingData :
            switch (sendDataStatus) {
                case sds_sending :
                    break;
                case sds_completedSuccessfully :
                    SampleHistory_clear(&sampleHistory);
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
                SystemTime_futureTime(200, &time);
                wlmState = wlms_poweringDown;
            }
            break;
        case wlms_poweringDown :
            if (SystemTime_timeHasArrived(&time)) {
                // power down peripherals
                DDRC |= (1 << PC1);
                PORTC &= ~(1 << PC1);

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

WaterLevelMonitorState WaterLevelMonitor_state (void)
{
    return wlmState;
}

