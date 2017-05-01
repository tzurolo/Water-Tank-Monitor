//
//  LED Lighting UPS monitor
//
//      uses the hardware UART to communicate with LED lighting UPS
//      use the TCPIP Console to communicate with the remote host
//
//
#include "WaterLevelMonitor.h"

#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

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
    wlms_sleeping
} WaterLevelMonitorState;

// state variables
static const char tokenDelimiters[] = " \n\r";
static WaterLevelMonitorState wlmState;
static SendDataStatus sendDataStatus;
static SystemTime_t powerdownDelay;
static bool gotDataFromHost;
static int16_t latestBatteryVoltage;
static int16_t latestTemperature;

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

    latestBatteryVoltage = 0;
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
                // give it another second of power to properly close the connection
                SystemTime_futureTime(100, &powerdownDelay);
                wlmState = wlms_poweringDown;
            }
            break;
        case wlms_poweringDown :
            if (SystemTime_timeHasArrived(&powerdownDelay)) {
                wlmState = wlms_sleeping;
            }
            break;
        case wlms_sleeping :
            // power down peripherals
            DDRC |= (1 << PC5);
            PORTC &= ~(1 << PC5);

            ADCSRA &= ~(1 << ADEN);
            wdt_enable(WDTO_8S);
            // WDTCSR |= (1 << WDIE);
            do {
              set_sleep_mode(SLEEP_MODE_PWR_DOWN);
              cli();
                sleep_enable();
                sleep_bod_disable();
                power_all_disable();
                // disable all digital inputs
                DIDR0 = 0x3F;
                DIDR1 = 3;
                // turn off all pullups
                PORTD = 0;

              sei();
                sleep_cpu();
                sleep_disable();
              sei();
            }  while (0);
            break;
    }
}
