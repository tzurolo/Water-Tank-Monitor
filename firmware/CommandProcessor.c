//
// Command processor
//
// Interprets and executes commands from SMS or the console
//

#include "CommandProcessor.h"

#include <stdlib.h>
#include <avr/pgmspace.h>
#include <avr/power.h>

#include "ADCManager.h"
#include "SoftwareSerialRx0.h"
#include "SoftwareSerialRx2.h"
#include "SoftwareSerialTx.h"
#include "CellularComm_SIM800.h"
#include "CellularTCPIP_SIM800.h"
#include "TCPIPConsole.h"
#include "BatteryMonitor.h"
#include "InternalTemperatureMonitor.h"
#include "UltrasonicSensorMonitor.h"
#include "WaterLevelMonitor.h"
#include "SystemTime.h"
#include "SIM800.h"
#include "Console.h"
#include "EEPROM_Util.h"
#include "EEPROMStorage.h"
#include "StringUtils.h"
#include "UART_async.h"

typedef void (*StringProvider)(
    CharString_t *string);

CharString_define(60, CommandProcessor_incomingCommand)
CharString_define(100, CommandProcessor_commandReply)

void CommandProcessor_createStatusMessage (
    CharString_t *msg)
{
    CharString_clear(msg);
    SystemTime_appendCurrentToString(msg);
    CharString_appendP(PSTR(",st:"), msg);
    StringUtils_appendDecimal(CellularComm_state(), 2, 0, msg);
    if (CellularComm_stateIsTCPIPSubtask(CellularComm_state())) {
        CharString_appendC('.', msg);
        StringUtils_appendDecimal(CellularTCPIP_state(), 2, 0, msg);
    }
    CharString_appendP(PSTR(","), msg);
    StringUtils_appendDecimal(WaterLevelMonitor_state(), 1, 0, msg);
    CharString_appendP(PSTR(",U:"), msg);
    StringUtils_appendDecimal(UltrasonicSensorMonitor_currentDistance(), 3, 1, msg);
    CharString_appendP(PSTR(",Vb:"), msg);
    StringUtils_appendDecimal(BatteryMonitor_currentVoltage(), 1, 2, msg);
    CharString_appendP(PSTR(",Vc:"), msg);
    StringUtils_appendDecimal(CellularComm_batteryMillivolts(), 1, 3, msg);
    CharString_appendP(PSTR(",T:"), msg);
    if (InternalTemperatureMonitor_haveValidSample()) {
        StringUtils_appendDecimal(InternalTemperatureMonitor_currentTemperature(), 1, 0, msg);
    } else {
        CharString_appendP(PSTR("__"), msg);
    }
    CharString_appendP(PSTR(",r:"), msg);
    StringUtils_appendDecimal((int)CellularComm_registrationStatus(), 1, 0, msg);
    CharString_appendP(PSTR(",q:"), msg);
    StringUtils_appendDecimal(CellularComm_SignalQuality(), 2, 0, msg);
    CharString_appendP(PSTR("  "), msg);
}

static int16_t scanIntegerToken (
    CharStringSpan_t *str,
    bool *isValid)
{
    StringUtils_skipWhitespace(str);
    int16_t value = 0;
    StringUtils_scanInteger(str, isValid, &value, str);
    return value;
}

static uint32_t scanIntegerU32Token (
    CharStringSpan_t *str,
    bool *isValid)
{
    StringUtils_skipWhitespace(str);
    uint32_t value = 0;
    StringUtils_scanIntegerU32(str, isValid, &value, str);
    return value;
}

static void beginJSON (
    CharString_t *str)
{
    CharString_copyP(PSTR("{"), str);
}

static void continueJSON (
    CharString_t *str)
{
    CharString_appendC(',', str);
}

static void endJSON (
    CharString_t *str)
{
    CharString_appendC('}', str);
}

static void appendJSONStrValue (
    PGM_P name,
    StringProvider strProvider,
    CharString_t *str)
{
    CharString_appendC('\"', str);
    CharString_appendP(name, str);
    CharString_appendP(PSTR("\":\""), str);
    strProvider(str);
    CharString_appendC('\"', str);
}

static void appendJSONIntValue (
    PGM_P name,
    const int16_t value,
    CharString_t *str)
{
    CharString_appendC('\"', str);
    CharString_appendP(name, str);
    CharString_appendP(PSTR("\":"), str);
    StringUtils_appendDecimal(value, 1, 0, str);
}

static void makeJSONStrValue (
    PGM_P name,
    StringProvider strProvider,
    CharString_t *str)
{
    beginJSON(str);
    appendJSONStrValue(name, strProvider, str);
    endJSON(str);
}

static void makeJSONIntValue (
    PGM_P name,
    const int16_t value,
    CharString_t *str)
{
    beginJSON(str);
    appendJSONIntValue(name, value, str);
    endJSON(str);
}

bool CommandProcessor_executeCommand (
    const CharStringSpan_t* command,
    CharString_t *reply)
{
    bool validCommand = true;

    CharStringSpan_t cmd = *command;
    CharStringSpan_t cmdToken;
    StringUtils_scanToken(&cmd, &cmdToken);
    if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("status"))) {
        CharStringSpan_t statusToNumber;
        CharStringSpan_clear(&statusToNumber);
        StringUtils_scanToken(&cmd, &cmdToken);
        if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("to"))) {
            StringUtils_scanToken(&cmd, &statusToNumber);
        }
	// return status info
	CommandProcessor_createStatusMessage(reply);
        if (!CharStringSpan_isEmpty(&statusToNumber)) {
            CellularComm_setOutgoingSMSMessageNumber(&statusToNumber);
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("tset"))) {
        const uint32_t serverTime = scanIntegerU32Token(&cmd, &validCommand);
        if (validCommand && (serverTime != 0)) {
            SystemTime_setTimeAdjustment(&serverTime);
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("set"))) {
        StringUtils_scanToken(&cmd, &cmdToken);
        if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("tCalOffset"))) {
            const int16_t tempCalOffset = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setTempCalOffset(tempCalOffset);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("apn"))) {
            StringUtils_scanToken(&cmd, &cmdToken);
            if (!CharStringSpan_isEmpty(&cmdToken)) {
                EEPROMStorage_setAPN(&cmdToken);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("ipserver"))) {
            StringUtils_scanToken(&cmd, &cmdToken);
            const uint16_t ipPort = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setIPConsoleServerAddress(&cmdToken);
                EEPROMStorage_setIPConsoleServerPort(ipPort);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("sampleinterval"))) {
            const uint16_t sampleInterval = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setSampleInterval(sampleInterval);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("loginterval"))) {
            const uint16_t loggingInterval = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setSampleInterval(loggingInterval);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("thingspeak"))) {
            StringUtils_scanToken(&cmd, &cmdToken);
            if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("on"))) {
                EEPROMStorage_setThingspeak(true);
            } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("off"))) {
                EEPROMStorage_setThingspeak(false);
            } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("address"))) {
                StringUtils_scanToken(&cmd, &cmdToken);
                if (!CharStringSpan_isEmpty(&cmdToken)) {
                    EEPROMStorage_setThingspeakHostAddress(&cmdToken);
                }
            } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("port"))) {
                const uint16_t port = scanIntegerToken(&cmd, &validCommand);
                if (validCommand) {
                    EEPROMStorage_setThingspeakHostPort(port);
                }
            } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("writekey"))) {
                StringUtils_scanToken(&cmd, &cmdToken);
                if (!CharStringSpan_isEmpty(&cmdToken)) {
                    EEPROMStorage_setThingspeakWriteKey(&cmdToken);
                }
            } else {
                validCommand = false;
            }
        } else {
            validCommand = false;
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("get"))) {
        StringUtils_scanToken(&cmd, &cmdToken);
        if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("PIN"))) {
            makeJSONStrValue(PSTR("PIN"), EEPROMStorage_getPIN, reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("tCalOffset"))) {
            makeJSONIntValue(PSTR("tcal"), EEPROMStorage_tempCalOffset(), reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("apn"))) {
            makeJSONStrValue(PSTR("APN'"), EEPROMStorage_getAPN, reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("ipserver"))) {
            beginJSON(reply);
            appendJSONStrValue(PSTR("IP_Addr"), EEPROMStorage_getIPConsoleServerAddress, reply);
            continueJSON(reply);
            appendJSONIntValue(PSTR("IP_Port"), EEPROMStorage_ipConsoleServerPort(), reply);
            endJSON(reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("sampleinterval"))) {
            makeJSONIntValue(PSTR("sampleInterval"), EEPROMStorage_sampleInterval(),
                reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("loginterval"))) {
            makeJSONIntValue(PSTR("logInterval"), EEPROMStorage_LoggingUpdateInterval(), reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("thingspeak"))) {
            beginJSON(reply);
            appendJSONIntValue(PSTR("TS_En"), EEPROMStorage_thingspeakEnabled() ? 1 : 0, reply);
            continueJSON(reply);
            appendJSONStrValue(PSTR("TS_Addr"), EEPROMStorage_getThingspeakHostAddress, reply);
            continueJSON(reply);
            appendJSONIntValue(PSTR("TS_Port"), EEPROMStorage_thingspeakHostPort(), reply);
            continueJSON(reply);
            appendJSONStrValue(PSTR("TS_WK"), EEPROMStorage_getThingspeakWriteKey, reply);
            endJSON(reply);
        } else {
            validCommand = false;
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("notify"))) {
        StringUtils_scanToken(&cmd, &cmdToken);
        if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("on"))) {
            EEPROMStorage_setNotification(true);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("off"))) {
            EEPROMStorage_setNotification(false);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("low"))) {
            const uint16_t level = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setWaterLowNotificationLevel(level);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("high"))) {
            const uint16_t level = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setWaterHighNotificationLevel(level);
            }
        } else {
            validCommand = false;
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("sms"))) {
        // get number to send to
        CharStringSpan_t recipientNumber;
        StringUtils_scanToken(&cmd, &recipientNumber);
        if ((CharStringSpan_length(&recipientNumber) > 5) &&
            (CharStringSpan_front(&recipientNumber) == '+')) {
            // got a phone number
            // get the message to send. it should be a quoted string
            CharStringSpan_t message;
            StringUtils_scanQuotedString(&cmd, &message, NULL);
            if (!CharStringSpan_isEmpty(&message)) {
                // got the message
                CharString_copyIters(
                    CharStringSpan_begin(&message),
                    CharStringSpan_end(&message),
                    reply);
                CellularComm_setOutgoingSMSMessageNumber(&recipientNumber);
            }
        } else {
            validCommand = false;
        }
#if BYTEQUEUE_HIGHWATERMARK_ENABLED
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("bqhw"))) {
        // byte queue report highwater
        SoftwareSerialRx0_reportHighwater();
        SoftwareSerialRx2_reportHighwater();
        SoftwareSerialTx_reportHighwater();
        UART_reportHighwater();
#endif
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("reboot"))) {
        // reboot
        SystemTime_commenceShutdown();
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("eeread"))) {
        const uint16_t eeAddr = scanIntegerToken(&cmd, &validCommand);
        if (validCommand) {
            beginJSON(reply);
            appendJSONIntValue(PSTR("EEAddr"), eeAddr, reply);
            continueJSON(reply);
            appendJSONIntValue(PSTR("EEVal"), EEPROM_read((uint8_t*)eeAddr), reply);
            endJSON(reply);
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("eewrite"))) {
        const uint16_t eeAddr = scanIntegerToken(&cmd, &validCommand);
        if (validCommand) {
            const uint16_t eeValue = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROM_write((uint8_t*)eeAddr, eeValue);
            }
        }
    } else {
        validCommand = false;
        Console_printP(PSTR("unrecognized command"));
    }

    return validCommand;
}
//                uint32_t i1 = 30463UL;
//                uint32_t i2 = 30582UL;
#if 0
// sleep code
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("sleep"))) {
            //clock_prescale_set(clock_div_256);
            uint16_t sleepTime = 8;
            cmdToken = strtok(NULL, tokenDelimiters);
            if (cmdToken != NULL) {
                sleepTime = atoi(cmdToken);
            }
            // disable ADC (move to ADCManager)
            ADCSRA &= ~(1 << ADEN);
            power_all_disable();
            // disable all digital inputs
            DIDR0 = 0x3F;
            DIDR1 = 3;
            // turn off all pullups
            PORTB = 0;
            PORTC = 0;
            PORTD = 0;
            SystemTime_sleepFor(sleepTime);
            power_all_enable();
            ADCManager_Initialize();
            BatteryMonitor_Initialize();
            InternalTemperatureMonitor_Initialize();
            UltrasonicSensorMonitor_Initialize();
            SoftwareSerialRx0_Initialize();
            SoftwareSerialRx2_Initialize();
            Console_Initialize();
            CellularComm_Initialize();
            CellularTCPIP_Initialize();
            TCPIPConsole_Initialize();
#endif