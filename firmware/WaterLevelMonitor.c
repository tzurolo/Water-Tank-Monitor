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

// water level has to change by this percentage or more
// to get back to inRange after going out of range
#define waterLevelDeadband 5

// water level state
typedef enum WaterLevel_enum {
    wl_inRange = 'I',
    wl_low = 'L',
    wl_high = 'H'
} WaterLevelState;

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
static SystemTime_t lastSampleTime;
static SampleHistory_define(30, sampleHistory);
static int16_t dataSenderSampleIndex;
static WaterLevelState currentWaterLevelState;
static uint8_t currentWaterLevelPercent;

static bool dataSender (void)
{
    bool sendComplete = false;

    CharString_define(25, dataToSend);
    if (dataSenderSampleIndex == -1) {
        // send per-post data
        //CommandProcessor_createStatusMessage(&dataToSend);
        CharString_copyP(PSTR("I"), &dataToSend);
        StringUtils_appendDecimal(EEPROMStorage_unitID(), 1, 0, &dataToSend);
        CharString_appendC('N', &dataToSend);
        SystemTime_t curTime;
        SystemTime_getCurrentTime(&curTime);
        StringUtils_appendDecimal(SystemTime_dayOfWeek(&curTime), 1, 0, &dataToSend);
        StringUtils_appendDecimal(SystemTime_hours(&curTime), 2, 0, &dataToSend);
        StringUtils_appendDecimal(SystemTime_minutes(&curTime), 2, 0, &dataToSend);
        CharString_appendC('L', &dataToSend);
        StringUtils_appendDecimal(currentWaterLevelPercent, 1, 0, &dataToSend);
        CharString_appendC('B', &dataToSend);
        StringUtils_appendDecimal(BatteryMonitor_currentVoltage(), 1, 0, &dataToSend);
        CharString_appendC('R', &dataToSend);
        StringUtils_appendDecimal((int)CellularComm_registrationStatus(), 1, 0, &dataToSend);
        CharString_appendC('Q', &dataToSend);
        StringUtils_appendDecimal(CellularComm_SignalQuality(), 1, 0, &dataToSend);
    } else {
        // send sample data
        const SampleHistory_Sample* sample =
            SampleHistory_getAt(dataSenderSampleIndex, &sampleHistory);
        CharString_copyP(PSTR(";"), &dataToSend);
        if (sample->relSampleTime != 0) {
            CharString_appendC('S', &dataToSend);
            StringUtils_appendDecimal(sample->relSampleTime, 1, 0, &dataToSend);
        }
        CharString_appendC('W', &dataToSend);
        StringUtils_appendDecimal(sample->waterDistance, 1, 0, &dataToSend);
        CharString_appendC('T', &dataToSend);
        StringUtils_appendDecimal(sample->temperature, 1, 0, &dataToSend);
    }

    ++dataSenderSampleIndex;
    if (dataSenderSampleIndex >= ((int16_t)SampleHistory_length(&sampleHistory))) {
        SystemTime_t curTime;
        SystemTime_getCurrentTime(&curTime);
        const int32_t secondsSinceLastSample =
            SystemTime_diffSec(&curTime, &lastSampleTime);
        if (secondsSinceLastSample != 0) {
            CharString_appendP(PSTR(";S"), &dataToSend);
            StringUtils_appendDecimal(secondsSinceLastSample, 1, 0, &dataToSend);
        }
        CharString_appendC('Z', &dataToSend);
        sendComplete = true;
    }
    CellularTCPIP_writeDataCS(&dataToSend);

    return sendComplete;
}

static void TCPIPSendCompletionCallaback (
    const bool success)
{
    sendDataStatus = success
        ? sds_completedSuccessfully
        : sds_completedFailed;   
}

static void IPDataCallback (
    const CharString_t *ipData)
{
    gotDataFromHost = true;
    CommandProcessor_processCommand(CharString_cstr(ipData), "", "");
}

static void enableTCPIP (void)
{
    gotDataFromHost = false;
    CellularComm_Enable();
    TCPIPConsole_setDataReceiver(IPDataCallback);
    TCPIPConsole_enable(false);
}

// returns true if the water level just went out of the normal range
static bool updateWaterLevelState (
    const uint16_t waterDistance) // in CM
{
    bool wentOutOfRange = false;

    // compute percentage full
    const uint16_t emptyDistance = EEPROMStorage_waterTankEmptyDistance();
    const uint16_t fullDistance = EEPROMStorage_waterTankFullDistance();
    if (waterDistance >= emptyDistance) {
        currentWaterLevelPercent = 0;
    } else if (waterDistance <= fullDistance) {
        currentWaterLevelPercent = 100;
    } else {
        // pressure is between empty and full
        const uint16_t distanceRange = emptyDistance - fullDistance;
        uint32_t relativeDistance = emptyDistance - waterDistance;
        currentWaterLevelPercent = (relativeDistance * 100) / distanceRange;
    }

    switch (currentWaterLevelState) {
	case wl_inRange :
	    if (currentWaterLevelPercent >= EEPROMStorage_waterHighNotificationLevel()) {
		Console_printP(PSTR(">>> high level notification <<<"));
		currentWaterLevelState = wl_high;
                wentOutOfRange = true;
	    } else if (currentWaterLevelPercent <= EEPROMStorage_waterLowNotificationLevel()) {
		Console_printP(PSTR(">>> low level notification <<<"));
		currentWaterLevelState = wl_low;
                wentOutOfRange = true;
	    }
	    break;
	case wl_low :
	    // apply hysteresis
	    if (currentWaterLevelPercent > (EEPROMStorage_waterLowNotificationLevel() + waterLevelDeadband)) {
		Console_printP(PSTR(">>> back in range from low <<<"));
		currentWaterLevelState = wl_inRange;
	    }
	    break;
	case wl_high :
	    // apply hysteresis
	    if (currentWaterLevelPercent < (EEPROMStorage_waterHighNotificationLevel() - waterLevelDeadband)) {
		Console_printP(PSTR(">>> back in range from high <<<"));
		currentWaterLevelState = wl_inRange;
	    }
	    break;
    }
    
    return wentOutOfRange;
}

void WaterLevelMonitor_Initialize (void)
{
    wlmState = wlms_initial;
    // wlmState = wlms_done;
    SampleHistory_clear(&sampleHistory);
    dataSenderSampleIndex = -1;
    currentWaterLevelState = wl_inRange;
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
                enableTCPIP();
            }
            wlmState = wlms_waitingForSensorData;
            break;
        case wlms_waitingForSensorData :
            if (BatteryMonitor_haveValidSample() &&
                InternalTemperatureMonitor_haveValidSample() &&
                UltrasonicSensorMonitor_haveValidSample()) {
                // log sensor data
                SampleHistory_Sample sample;
                SystemTime_t curTime;
                SystemTime_getCurrentTime(&curTime);
                if (SampleHistory_empty(&sampleHistory)) {
                    sample.relSampleTime = 0;
                } else {
                    const int32_t secondsSinceLastSample =
                        SystemTime_diffSec(&curTime, &lastSampleTime);
                    sample.relSampleTime = secondsSinceLastSample;
                }
                lastSampleTime = curTime;
                sample.temperature =
                    (uint8_t)InternalTemperatureMonitor_currentTemperature();
                sample.waterDistance = UltrasonicSensorMonitor_currentDistance();
                SampleHistory_insertSample(&sample, &sampleHistory);

                const bool wentOutOfRange = 
                    updateWaterLevelState(sample.waterDistance / 10);   // cvt mm to cm
    
                if (TCPIPConsole_isEnabled()) {
                    wlmState = wlms_waitingForConnection;
                } else {
                    if (wentOutOfRange) {
                        enableTCPIP();
                        wlmState = wlms_waitingForConnection;
                    } else {
                        // just a sample interval
                        SystemTime_futureTime(50, &time);
                        wlmState = wlms_poweringDown;
                    }
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

