//
// Command processor
//
// Interprets and executes commands from SMS or the console
//

#include "CommandProcessor.h"

#include <stdlib.h>
#include <avr/pgmspace.h>

#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#include "CellularComm_SIM800.h"
#include "CellularTCPIP_SIM800.h"
#include "TCPIPConsole.h"
#include "BatteryMonitor.h"
#include "InternalTemperatureMonitor.h"
#include "SystemTime.h"
#include "SIM800.h"
#include "Console.h"
#include "EEPROM_Util.h"
#include "EEPROMStorage.h"
#include "StringUtils.h"

#define CMD_TOKEN_BUFFER_LEN 80

static const char tokenDelimiters[] = " \n\r";

// appends n.nnV to outgoing message text
static void appendVoltageToString (
    const int16_t voltage,
    CharString_t *str)
{
    StringUtils_appendDecimal(voltage, 1, 2, str);
    CharString_appendP(PSTR("V"), str);
}

// appends n.nC to outgoing message text
static void appendTemperatureToString (
    const int16_t temp,
    CharString_t *str)
{
    StringUtils_appendDecimal(temp, 1, 1, str);
    CharString_appendP(PSTR("C"), str);
}

static void postReply (
    const char* phoneNumber)
{
    if (phoneNumber[0] != 0) {
        CellularComm_postOutgoingMessage(phoneNumber);
    } else {
        Console_printCS(&CellularComm_outgoingSMSMessageText);
    }
}

static void dataSender (void)
{
    CellularTCPIP_writeDataP(PSTR("data sent to host"));
}

static void sendCompletionCallaback (
    const bool success)
{
    if (success) {
        Console_printP(PSTR("-----> send successful"));
    } else {
        Console_printP(PSTR("-----> SEND FAILED"));
    }
}

void CommandProcessor_createStatusMessage (
    CharString_t *msg)
{
    CharString_clear(msg);
    StringUtils_appendDecimal(CellularComm_currentTime()->hour, 2, 0, msg);
    CharString_appendP(PSTR(":"), msg);
    StringUtils_appendDecimal(CellularComm_currentTime()->minutes, 2, 0, msg);
    CharString_appendP(PSTR(", up:"), msg);
    SystemTime_appendCurrentToString(msg);
    CharString_appendP(PSTR(", st:"), msg);
    StringUtils_appendDecimal(CellularComm_state(), 2, 0, msg);
    if (CellularComm_stateIsTCPIPSubtask(CellularComm_state())) {
        CharString_appendC('.', msg);
        StringUtils_appendDecimal(CellularTCPIP_state(), 2, 0, msg);
    }
    CharString_appendP(PSTR(", csq:"), msg);
    StringUtils_appendDecimal(CellularComm_SignalQuality(), 2, 0, msg);
    CharString_appendP(PSTR(", Vb:"), msg);
    StringUtils_appendDecimal(BatteryMonitor_currentVoltage(), 1, 2, msg);
    CharString_appendP(PSTR("V, Vc:"), msg);
    StringUtils_appendDecimal(CellularComm_batteryMillivolts(), 1, 3, msg);
    CharString_appendP(PSTR("V, T:"), msg);
    StringUtils_appendDecimal(InternalTemperatureMonitor_currentTemperature(), 1, 0, msg);
    CharString_appendP(PSTR(", reg:"), msg);
    StringUtils_appendDecimal((int)CellularComm_registrationStatus(), 1, 0, msg);
    CharString_appendP(PSTR("  "), msg);
}

void CommandProcessor_processCommand (
    const char* command,
    const char* phoneNumber,
    const char* timestamp)
{
#if 0
	char msgbuf[80];
    sprintf(msgbuf, "cmd: '%s'", command);
    Console_print(msgbuf);
    sprintf(msgbuf, "phone#: '%s'", phoneNumber);
    Console_print(msgbuf);
    sprintf(msgbuf, "time: '%s'", timestamp);
    Console_print(msgbuf);
#endif

    char cmdTokenBuf[CMD_TOKEN_BUFFER_LEN];
    strncpy(cmdTokenBuf, command, CMD_TOKEN_BUFFER_LEN-1);
    cmdTokenBuf[CMD_TOKEN_BUFFER_LEN-1] = 0;
    const char* cmdToken = strtok(cmdTokenBuf, tokenDelimiters);
    if (cmdToken != NULL) {
        if (strcasecmp_P(cmdToken, PSTR("status")) == 0) {
            const char* statusToNumber = phoneNumber;
            cmdToken = strtok(NULL, tokenDelimiters);
            if (cmdToken != NULL) {
                if (strcasecmp_P(cmdToken, PSTR("to")) == 0) {
                    cmdToken = strtok(NULL, tokenDelimiters);
                    if (cmdToken != NULL) {
                        statusToNumber = cmdToken;
                    }
                }
            }
	    // return status info
	    CommandProcessor_createStatusMessage(
                &CellularComm_outgoingSMSMessageText);
            postReply(statusToNumber);
        } else if (strcasecmp_P(cmdToken, PSTR("led")) == 0) {
            cmdToken = strtok(NULL, tokenDelimiters);
            if (cmdToken != NULL) {
                if (strcasecmp_P(cmdToken, PSTR("status")) == 0) {
//                    CharString_copyCS(LEDLightingUPSMonitor_UPSStatusStr(),
//                        &CellularComm_outgoingSMSMessageText);
//                    postReply(phoneNumber);
                } else if (strcasecmp_P(cmdToken, PSTR("settings")) == 0) {
//                    CharString_copyCS(LEDLightingUPSMonitor_UPSSettings(),
//                        &CellularComm_outgoingSMSMessageText);
//                    postReply(phoneNumber);
                } else {
//                    LEDLightingUPSMonitor_sendCommand(command + 4);
                }
            }
        } else if (strcasecmp_P(cmdToken, PSTR("set")) == 0) {
            cmdToken = strtok(NULL, tokenDelimiters);
            if (cmdToken != NULL) {
                if (strcasecmp_P(cmdToken, PSTR("apn")) == 0) {
                    cmdToken = strtok(NULL, tokenDelimiters);
                    if (cmdToken != NULL) {
                        EEPROMStorage_setAPN(cmdToken);
                    }
                } else if (strcasecmp_P(cmdToken, PSTR("loginterval")) == 0) {
                    cmdToken = strtok(NULL, tokenDelimiters);
                    if (cmdToken != NULL) {
                        const uint16_t loggingInterval = atoi(cmdToken);
                        EEPROMStorage_setLoggingUpdateInterval(loggingInterval);
                    }
                } else if (strcasecmp_P(cmdToken, PSTR("thingspeak")) == 0) {
                    cmdToken = strtok(NULL, tokenDelimiters);
                    if (cmdToken != NULL) {
                        if (strcasecmp_P(cmdToken, PSTR("on")) == 0) {
                            EEPROMStorage_setThingspeak(true);
                        } else if (strcasecmp_P(cmdToken, PSTR("off")) == 0) {
                            EEPROMStorage_setThingspeak(false);
                        } else if (strcasecmp_P(cmdToken, PSTR("address")) == 0) {
                            cmdToken = strtok(NULL, tokenDelimiters);
                            if (cmdToken != NULL) {
                                EEPROMStorage_setThingspeakHostAddress(cmdToken);
                            }
                        } else if (strcasecmp_P(cmdToken, PSTR("port")) == 0) {
                            cmdToken = strtok(NULL, tokenDelimiters);
                            if (cmdToken != NULL) {
	                        const uint16_t port = atoi(cmdToken);
                                EEPROMStorage_setThingspeakHostPort(port);
                            }
                        } else if (strcasecmp_P(cmdToken, PSTR("writekey")) == 0) {
                            cmdToken = strtok(NULL, tokenDelimiters);
                            if (cmdToken != NULL) {
                                EEPROMStorage_setThingspeakWriteKey(cmdToken);
                            }
                        }
                    }
                }
            }
        } else if (strcasecmp_P(cmdToken, PSTR("get")) == 0) {
            cmdToken = strtok(NULL, tokenDelimiters);
            if (cmdToken != NULL) {
                if (strcasecmp_P(cmdToken, PSTR("PIN")) == 0) {
                    char pinBuffer[16];
                    EEPROMStorage_getPIN(pinBuffer);
                    CharString_clear(&CellularComm_outgoingSMSMessageText);
                    CharString_appendP(PSTR("PIN: "),
                        &CellularComm_outgoingSMSMessageText);
                    CharString_append(pinBuffer,
                        &CellularComm_outgoingSMSMessageText);
                    postReply(phoneNumber);
                } else if (strcasecmp_P(cmdToken, PSTR("apn")) == 0) {
                    char apnBuffer[64];
                    EEPROMStorage_getAPN(apnBuffer);
                    CharString_clear(&CellularComm_outgoingSMSMessageText);
                    CharString_appendP(PSTR("APN:'"),
                        &CellularComm_outgoingSMSMessageText);
                    CharString_append(apnBuffer,
                        &CellularComm_outgoingSMSMessageText);
                    CharString_appendP(PSTR("'"),
                        &CellularComm_outgoingSMSMessageText);
                    postReply(phoneNumber);
                } else if (strcasecmp_P(cmdToken, PSTR("loginterval")) == 0) {
                    CharString_clear(&CellularComm_outgoingSMSMessageText);
                    CharString_appendP(PSTR("Logging Interval:"),
                        &CellularComm_outgoingSMSMessageText);
                    StringUtils_appendDecimal(
                        EEPROMStorage_LoggingUpdateInterval(), 1, 0,
                        &CellularComm_outgoingSMSMessageText);
                    postReply(phoneNumber);
                } else if (strcasecmp_P(cmdToken, PSTR("thingspeak")) == 0) {
                    char buffer[64];
                    CharString_clear(&CellularComm_outgoingSMSMessageText);
                    CharString_appendP(PSTR("ThingSpeak Enabled:"),
                        &CellularComm_outgoingSMSMessageText);
                    StringUtils_appendDecimal(
                        EEPROMStorage_thingspeakEnabled() ? 1 : 0, 1, 0,
                        &CellularComm_outgoingSMSMessageText);
                    CharString_appendP(PSTR(", addr:'"),
                        &CellularComm_outgoingSMSMessageText);
                    EEPROMStorage_getThingspeakHostAddress(buffer);
                    CharString_append(buffer,
                        &CellularComm_outgoingSMSMessageText);
                    CharString_appendP(PSTR("', port:"),
                        &CellularComm_outgoingSMSMessageText);
                    StringUtils_appendDecimal(
                        EEPROMStorage_thingspeakHostPort(), 1, 0,
                        &CellularComm_outgoingSMSMessageText);
                    CharString_appendP(PSTR(", writekey:'"),
                        &CellularComm_outgoingSMSMessageText);
                    EEPROMStorage_getThingspeakWriteKey(buffer);
                    CharString_append(buffer,
                        &CellularComm_outgoingSMSMessageText);
                    CharString_appendP(PSTR("'"),
                        &CellularComm_outgoingSMSMessageText);
                    postReply(phoneNumber);
                }
            }
	} else if (strcasecmp_P(cmdToken, PSTR("notify")) == 0) {
	    cmdToken = strtok(NULL, tokenDelimiters);
	    if (cmdToken != NULL) {
                if (strcasecmp_P(cmdToken, PSTR("on")) == 0) {
                    EEPROMStorage_setNotification(true);
                } else if (strcasecmp_P(cmdToken, PSTR("off")) == 0) {
                    EEPROMStorage_setNotification(false);
                } else if (strcasecmp_P(cmdToken, PSTR("low")) == 0) {
                    cmdToken = strtok(NULL, tokenDelimiters);
                    const uint8_t level = atoi(cmdToken);
                    EEPROMStorage_setWaterLowNotificationLevel(level);
                } else if (strcasecmp_P(cmdToken, PSTR("high")) == 0) {
                    cmdToken = strtok(NULL, tokenDelimiters);
                    if (cmdToken != NULL) {
                        const uint8_t level = atoi(cmdToken);
                        EEPROMStorage_setWaterHighNotificationLevel(level);
                    }
                }
            }
        } else if (strcasecmp_P(cmdToken, PSTR("sms")) == 0) {
            // get number to send to
            cmdToken = strtok(NULL, tokenDelimiters);
            if ((cmdToken != NULL) && (cmdToken[0] == '+') && (strlen(cmdToken) > 5)) {
                // got a phone number - copy it into a buffer
                char recipientNumber[20];
                strncpy(recipientNumber, cmdToken, 19);
                recipientNumber[19] = 0;    // safety null terminate
                // get the message to send. it should be a quoted string
                const char* msgStart = cmdToken + strlen(cmdToken);
                const ptrdiff_t msgOffset = msgStart - cmdTokenBuf;
                msgStart = command + msgOffset;
                CharString_define(50, message);
                StringUtils_scanQuotedString(msgStart, &message);
                if (CharString_length(&message) > 0) {
                    // got the message
                    CharString_clear(&CellularComm_outgoingSMSMessageText);
                    CharString_appendCS(&message, &CellularComm_outgoingSMSMessageText);
                    postReply(recipientNumber);
#if 0
            char printbuf[80];
            sprintf(printbuf, "phone#: '%s'", recipientNumber);
            Console_print(printbuf);
            sprintf(printbuf, "message: '%s'", message);
            Console_print(printbuf);
#endif
                }
            }
        } else if (strcasecmp_P(cmdToken, PSTR("cell")) == 0) {
            cmdToken = strtok(NULL, tokenDelimiters);
            if (cmdToken != NULL) {
                if (strcasecmp_P(cmdToken, PSTR("enable")) == 0) {
                    CellularComm_Enable();
                } else if (strcasecmp_P(cmdToken, PSTR("disable")) == 0) {
                    CellularComm_Disable();
                }
            }
        } else if (strcasecmp_P(cmdToken, PSTR("ipconsole")) == 0) {
            cmdToken = strtok(NULL, tokenDelimiters);
            if (cmdToken != NULL) {
                if (strcasecmp_P(cmdToken, PSTR("enable")) == 0) {
                    TCPIPConsole_enable(true);
                } else if (strcasecmp_P(cmdToken, PSTR("disable")) == 0) {
                    TCPIPConsole_disable(true);
                }
            }
        } else if (strcasecmp_P(cmdToken, PSTR("send")) == 0) {
            CellularTCPIP_sendData(dataSender, sendCompletionCallaback);
        } else if (strcasecmp_P(cmdToken, PSTR("pp")) == 0) {
            cmdToken = strtok(NULL, tokenDelimiters);
            if (cmdToken != NULL) {
                if (strcasecmp_P(cmdToken, PSTR("on")) == 0) {
                    DDRC |= (1 << PC5);
                    PORTC |= (1 << PC5);
                } else if (strcasecmp_P(cmdToken, PSTR("off")) == 0) {
                    DDRC |= (1 << PC5);
                    PORTC &= ~(1 << PC5);
                }
            }
        } else if (strcasecmp_P(cmdToken, PSTR("sleep")) == 0) {
            //clock_prescale_set(clock_div_256);
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
        } else if (strcasecmp_P(cmdToken, PSTR("reboot")) == 0) {
            // reboot
            SystemTime_commenceShutdown();
	} else if (strcasecmp_P(cmdToken, PSTR("eeread")) == 0) {
            cmdToken = strtok(NULL, tokenDelimiters);
            if (cmdToken != NULL) {
                const unsigned int uiAddress = atoi(cmdToken);
                const uint8_t eeromData = EEPROM_read((uint8_t*)uiAddress);
                CharString_clear(&CellularComm_outgoingSMSMessageText);
                StringUtils_appendDecimal(uiAddress, 1, 0,
                    &CellularComm_outgoingSMSMessageText);
                CharString_appendP(PSTR(":"),
                    &CellularComm_outgoingSMSMessageText);
                StringUtils_appendDecimal(eeromData, 1, 0,
                    &CellularComm_outgoingSMSMessageText);
                postReply(phoneNumber);
            }
        } else if (strcasecmp_P(cmdToken, PSTR("eewrite")) == 0) {
            cmdToken = strtok(NULL, tokenDelimiters);
            if (cmdToken != NULL) {
                const unsigned int uiAddress = atoi(cmdToken);
                cmdToken = strtok(NULL, tokenDelimiters);
                if (cmdToken != NULL) {
                    const uint8_t value = atoi(cmdToken);
                    EEPROM_write((uint8_t*)uiAddress, value);
                }
            }
        } else {
            Console_printP(PSTR("unrecognized command"));
        }
    }
}
//                uint32_t i1 = 30463UL;
//                uint32_t i2 = 30582UL;
