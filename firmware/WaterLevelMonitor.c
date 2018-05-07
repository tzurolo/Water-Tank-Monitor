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
#include "RAMSentinel.h"

#define SW_VERSION 10

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

typedef enum CommandProcessingMode_enum {
    cpm_singleCommand,
    cpm_commandBlock
} CommandProcessingMode;


// state variables
static const char tokenDelimiters[] = " \n\r";
static WaterLevelMonitorState wlmState;
static CommandProcessingMode commandMode = cpm_singleCommand;
static SendDataStatus sendDataStatus;
static SystemTime_t time;   // used to measure how long it took to get a connection,
                            // and for the powerdown delay future time
static bool gotCommandFromHost;
static SystemTime_t lastSampleTime;
static SampleHistory_define(30, sampleHistory);
static int16_t dataSenderSampleIndex;
static CharStringSpan_t remainingReplyDataToSend;
static WaterLevelState currentWaterLevelState;
static uint8_t currentWaterLevelPercent;

#define DATA_SENDER_BUFFER_LEN 30

static bool sampleDataSender (void)
{
    bool sendComplete = false;

    // check to see if there is enough room in the output queue for our data. If
    // not, we check again next time we're called.
    if (CellularTCPIP_availableSpaceForWriteData() >= DATA_SENDER_BUFFER_LEN) {
        // there is room in the output queue for our data
        CharString_define(DATA_SENDER_BUFFER_LEN, dataToSend);
        // RAMSentinel_printStackPtr();
        if (dataSenderSampleIndex == -1) {
            // send per-post data
            CharString_copyP(PSTR("I"), &dataToSend);
            StringUtils_appendDecimal(EEPROMStorage_unitID(), 1, 0, &dataToSend);
            CharString_appendC('V', &dataToSend);
            StringUtils_appendDecimal(SW_VERSION, 1, 0, &dataToSend);
            CharString_appendC('B', &dataToSend);
            StringUtils_appendDecimal(BatteryMonitor_currentVoltage(), 1, 0, &dataToSend);
            CharString_appendC('R', &dataToSend);
            StringUtils_appendDecimal((int)CellularComm_registrationStatus(), 1, 0, &dataToSend);
            CharString_appendC('Q', &dataToSend);
            StringUtils_appendDecimal(CellularComm_SignalQuality(), 1, 0, &dataToSend);
            SystemTime_t curTime;
            SystemTime_getCurrentTime(&curTime);
            const int32_t secondsSinceLastSample = SystemTime_diffSec(&curTime, &lastSampleTime);
            CharString_appendP(PSTR("C"), &dataToSend);
            StringUtils_appendDecimal(secondsSinceLastSample, 1, 0, &dataToSend);
            CharString_appendC(';', &dataToSend);
        } else {
            // send next sample
            const SampleHistory_Sample* sample =
                SampleHistory_getAt(dataSenderSampleIndex, &sampleHistory);
            if (sample->relSampleTime != 0) {
                CharString_appendC('D', &dataToSend);
                StringUtils_appendDecimal(sample->relSampleTime, 1, 0, &dataToSend);
            }
            CharString_appendC('W', &dataToSend);
            StringUtils_appendDecimal(sample->waterDistance, 1, 0, &dataToSend);
            CharString_appendC('T', &dataToSend);
            StringUtils_appendDecimal(sample->temperature, 1, 0, &dataToSend);
            CharString_appendC(';', &dataToSend);
        }

        ++dataSenderSampleIndex;
        // if this is the last sample append the delta time between the last sample and
        // now, and append the terminator (Z)
        if (dataSenderSampleIndex >= ((int16_t)SampleHistory_length(&sampleHistory))) {
            CharString_appendP(PSTR("Z\n"), &dataToSend);
            sendComplete = true;
        }
        CellularTCPIP_writeDataCS(&dataToSend);
    }

    return sendComplete;
}

static bool replyDataSender (void)
{
    // RAMSentinel_printStackPtr();

    // send as much as there is enough room in the output queue for a chunk of our data.
    const uint16_t availableSpace = CellularTCPIP_availableSpaceForWriteData();
    const uint8_t spaceInSpan =
        (availableSpace > 255)
        ? 255
        : ((uint8_t)availableSpace);
    CharStringSpan_t chunk;
    CharStringSpan_extractLeft(spaceInSpan, &remainingReplyDataToSend, &chunk);
    CellularTCPIP_writeDataCSS(&chunk);

    return CharStringSpan_isEmpty(&remainingReplyDataToSend);
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
    // RAMSentinel_printStackPtr();
    for (int i = 0; i < CharString_length(ipData); ++i) {
        const char c = CharString_at(ipData, i);
        if ((c == '\r') || (c == '\n')) {
            // got command terminator
            if (!CharString_isEmpty(&CommandProcessor_incomingCommand)) {
                gotCommandFromHost = true;
                if (CharString_length(&CommandProcessor_incomingCommand) == 1) {
                    // check for mode command characters
                    switch (CharString_at(&CommandProcessor_incomingCommand, 0)) {
                        case '[' :
                            commandMode = cpm_commandBlock;
                            CharString_clear(&CommandProcessor_incomingCommand);
                            break;
                        case ']' :
                            commandMode = cpm_singleCommand;
                            CharString_clear(&CommandProcessor_incomingCommand);
                            break;
                        default:
                            break;
                    }
                }
            }
        } else {
            gotCommandFromHost = false;
            CharString_appendC(c, &CommandProcessor_incomingCommand);
        }
    }
}

static void enableTCPIP (void)
{
    CellularComm_Enable();
    gotCommandFromHost = false;
    CharString_clear(&CommandProcessor_incomingCommand);
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

void initiatePowerdown (void)
{
    TCPIPConsole_disable(false);
    CellularComm_Disable();

    // give it a little while to disable cellular comm
    SystemTime_futureTime(300, &time);

    wlmState = wlms_waitingForCellularCommDisable;
}

void transitionPerCommandMode(void)
{
    if (commandMode == cpm_commandBlock) {
        // more commands coming. wait for next command
        wlmState = wlms_waitingForHostCommand;
    } else {
        // no more commands coming. initiate powerdown sequence
        initiatePowerdown();
    }
}

void WaterLevelMonitor_Initialize (void)
{
    wlmState = wlms_initial;
    commandMode = cpm_singleCommand;
    SampleHistory_clear(&sampleHistory);
    dataSenderSampleIndex = -1;
    currentWaterLevelState = wl_inRange;
    gotCommandFromHost = false;
}

void WaterLevelMonitor_task (void)
{
    if ((wlmState > wlms_resuming) &&
        (wlmState < wlms_waitingForCellularCommDisable) &&
        SystemTime_timeHasArrived(&time)) {
        // task has exceeded timeout
        Console_printP(PSTR("WLM timeout"));
        initiatePowerdown();
    }

    switch (wlmState) {
        case wlms_initial :
            wlmState = (SystemTime_LastReboot() == lrb_software)
                ? wlms_done         // starting up after software reboot
                : wlms_resuming;    // hardware reboot
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

            // set up overal task timeout
            SystemTime_futureTime(EEPROMStorage_monitorTaskTimeout() * 100, &time);

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
                TCPIPConsole_sendData(sampleDataSender, TCPIPSendCompletionCallaback);
                wlmState = wlms_sendingSampleData;
            }
            break;
        case wlms_sendingSampleData :
            switch (sendDataStatus) {
                case sds_sending :
                    break;
                case sds_completedSuccessfully :
                    SampleHistory_clear(&sampleHistory);
                    commandMode = cpm_singleCommand;
                    wlmState = wlms_waitingForHostCommand;
                    break;
                case sds_completedFailed :
                    commandMode = cpm_singleCommand;
                    wlmState = wlms_waitingForHostCommand;
                    break;
            }
            break;
        case wlms_waitingForHostCommand:
            if (gotCommandFromHost) {
                gotCommandFromHost = false;
                CharString_clear(&CommandProcessor_commandReply);
                if (!CharString_isEmpty(&CommandProcessor_incomingCommand)) {
                    CharStringSpan_t cmd;
                    CharStringSpan_init(&CommandProcessor_incomingCommand, &cmd);
                    const bool successful =
                        CommandProcessor_executeCommand(&cmd, &CommandProcessor_commandReply);
                    CharString_clear(&CommandProcessor_incomingCommand);
                    if (CharString_isEmpty(&CommandProcessor_commandReply)) {
                        if (commandMode == cpm_commandBlock) {
                            // this will prompt the host for the next command
                            CharString_copyP(
                                successful
                                ? PSTR("OK\n")
                                : PSTR("ERROR\n"),
                                &CommandProcessor_commandReply);
                        }
                    } else {
                        CharString_appendC('\n', &CommandProcessor_commandReply);
                    }
                }
                if (SystemTime_shuttingDown()) {
                    initiatePowerdown();
                } else if (CharString_isEmpty(&CommandProcessor_commandReply)) {
                    transitionPerCommandMode();
                } else {
                    // prepare to send reply
                    wlmState = wlms_waitingForReadyToSendReply;
                }
            }
            break;
        case wlms_waitingForReadyToSendReply :
            if (TCPIPConsole_readyToSend()) {
                sendDataStatus = sds_sending;
                CharStringSpan_init(&CommandProcessor_commandReply, &remainingReplyDataToSend);
                TCPIPConsole_sendData(replyDataSender, TCPIPSendCompletionCallaback);
                wlmState = wlms_sendingReplyData;
            }
            break;
        case wlms_sendingReplyData :
            switch (sendDataStatus) {
                case sds_sending :
                    break;
                case sds_completedSuccessfully :
                case sds_completedFailed :
                    transitionPerCommandMode();
                    break;
            }
            break;
        case wlms_waitingForCellularCommDisable :
            if ((!CellularComm_isEnabled()) || SystemTime_timeHasArrived(&time)) {
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

bool WaterLevelMonitor_hasSampleData (void)
{
    return !SampleHistory_empty(&sampleHistory);
}

void WaterLevelMonitor_resume (void)
{
    wlmState = wlms_resuming;
}

WaterLevelMonitorState WaterLevelMonitor_state (void)
{
    return wlmState;
}

