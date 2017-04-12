//
// EEPROM Storage
//

#include "EEPROMStorage.h"

#include "EEPROM_Util.h"
#include "Console.h"

// This prevents the MSVC editor from tripping over EEMEM in definitions
#ifndef EEMEM
#define EEMEM
#endif

// storage address map
uint8_t EEMEM initFlag = 1; // initialization flag. Unprogrammed EE comes up as all one's

char EEMEM cellPIN[8] = "7353";
uint8_t EEMEM notification = 0;
uint16_t EEMEM loggingUpdateInterval = 600;
uint8_t EEMEM timeoutState = 0;

// filter parameters
uint8_t EEMEM filterSampleTime = 1;     // once per second
uint8_t EEMEM numFilterSamples = 180;   // 3 minutes of samples
uint16_t EEMEM filterVariance = 16383;  // effectively unlimited variance

// tank parameters
uint16_t EEMEM emptyDist = 290;
uint16_t EEMEM fullDist = 30;
uint8_t EEMEM lowNotification = 50;
uint8_t EEMEM highNotification = 90;

// internet
char EEMEM apn[32] = "mobiledata";

// thingspeak
uint8_t EEMEM thingspeakEnabled = 0;
char EEMEM thingspeakHostAddress[32] = "api.thingspeak.com";
uint16_t EEMEM thingspeakHostPort = 80;
char EEMEM thingspeakWriteKey[20] = "";

// TCPIP Console
uint8_t EEMEM ipConsoleEnabled = 1;
char EEMEM ipConsoleServerAddress[32] = "tonyz.dyndns.org";
uint16_t EEMEM ipConsoleServerPort = 3000;

static const char* tzPin = "7353"; // Tony's telestial Sim card PIN

void EEPROMStorage_Initialize (void)
{
    // check if EE has been initialized
    const uint8_t initFlag = EEPROM_read(&initFlag);
    const uint8_t initLevel = (initFlag == 0xFF) ? 0 : initFlag;

    if (initLevel < 1) {
        Console_printP(PSTR("re-initializing EEPROM"));
        // EE has not been initialized. Initialize now.
        EEPROMStorage_setPIN(tzPin);

	EEPROMStorage_setWaterTankEmptyDistance(290);
	EEPROMStorage_setWaterTankFullDistance(30);

	EEPROMStorage_setWaterHighNotificationLevel(90);
	EEPROMStorage_setWaterLowNotificationLevel(33);

	EEPROMStorage_setNotification(false);

        EEPROMStorage_setAPN("mobiledata");

        EEPROMStorage_setLoggingUpdateInterval(600);
        EEPROMStorage_setThingspeak(false);
        EEPROMStorage_setThingspeakHostAddress("api.thingspeak.com");
        EEPROMStorage_setThingspeakHostPort(80);
        EEPROMStorage_setThingspeakWriteKey("");

        EEPROMStorage_setFilterSampleTime(1);   // once per second
        EEPROMStorage_setFilterSamples(180);    // 3 minutes of data at once per second
        EEPROMStorage_setFilterVariance(16383); // effectively unlimited variance

        EEPROMStorage_setIPConsoleEnabled(false);
        EEPROMStorage_setIPConsoleServerAddress("tonyz.dyndns.org");
        EEPROMStorage_setIPConsoleServerPort(3000);

        // indicate EE has been initialized
        EEPROM_write(0, 1);
    }
}

void EEPROMStorage_setPIN (
	const char* PIN)
{
    EEPROM_writeString(cellPIN, sizeof(cellPIN), PIN);
}

void EEPROMStorage_getPIN (
    char* PIN)
{
    EEPROM_readString(cellPIN, sizeof(cellPIN), PIN);
}

void EEPROMStorage_setWaterTankEmptyDistance (
    const uint16_t value)
{
    EEPROM_writeWord(&emptyDist, value);
}

uint16_t EEPROMStorage_waterTankEmptyDistance (void)
{
    return EEPROM_readWord(&emptyDist);
}

void EEPROMStorage_setWaterTankFullDistance (
    const uint16_t value)
{
    EEPROM_writeWord(&fullDist, value);
}

uint16_t EEPROMStorage_waterTankFullDistance (void)
{
    return EEPROM_readWord(&fullDist);
}

void EEPROMStorage_setWaterLowNotificationLevel (
	const uint8_t level)
{
    EEPROM_write(&lowNotification, level);
}

uint8_t EEPROMStorage_waterLowNotificationLevel (void)
{
    return EEPROM_read(&lowNotification);
}

void EEPROMStorage_setWaterHighNotificationLevel (
    const uint8_t level)
{
    EEPROM_write(&highNotification, level);
}

uint8_t EEPROMStorage_waterHighNotificationLevel (void)
{
    return EEPROM_read(&highNotification);
}

void EEPROMStorage_setNotification (
    const bool onOff)
{
    EEPROM_write(&notification, onOff ? 1 : 0);
}

bool EEPROMStorage_notificationEnabled (void)
{
    return (EEPROM_read(&notification) == 1);
}

void EEPROMStorage_setTimeoutState (
    const uint8_t state)
{
    EEPROM_write(&timeoutState, state);
}

uint8_t EEPROMStorage_timeoutState (void)
{
    return EEPROM_read(&timeoutState);
}

void EEPROMStorage_setAPN (
    const char* APN)
{
    EEPROM_writeString(apn, sizeof(apn), APN);
}

void EEPROMStorage_getAPN (
    char* APN)
{
    EEPROM_readString(apn, sizeof(apn), APN);
}

void EEPROMStorage_setLoggingUpdateInterval (
    const uint16_t updateInterval)
{
    EEPROM_writeWord(&loggingUpdateInterval, updateInterval);
}

uint16_t EEPROMStorage_LoggingUpdateInterval (void)
{
    return EEPROM_readWord(&loggingUpdateInterval);
}

void EEPROMStorage_setThingspeak (
    const bool enabled)
{
    EEPROM_write(&thingspeakEnabled, enabled ? 1 : 0);
}

bool EEPROMStorage_thingspeakEnabled (void)
{
    return (EEPROM_read(&thingspeakEnabled) == 1);
}

void EEPROMStorage_setThingspeakHostAddress (
    const char* address)
{
    EEPROM_writeString(thingspeakHostAddress, sizeof(thingspeakHostAddress),
        address);
}

void EEPROMStorage_getThingspeakHostAddress (
    char* address)
{
    EEPROM_readString(thingspeakHostAddress, sizeof(thingspeakHostAddress),
        address);
}

void EEPROMStorage_setThingspeakHostPort (
    const uint16_t port)
{
    EEPROM_writeWord(&thingspeakHostPort, port);
}

uint16_t EEPROMStorage_thingspeakHostPort (void)
{
    return EEPROM_readWord(&thingspeakHostPort);
}

void EEPROMStorage_setThingspeakWriteKey (
    const char* writekey)
{
    EEPROM_writeString(thingspeakWriteKey, sizeof(thingspeakWriteKey),
        writekey);
}

void EEPROMStorage_getThingspeakWriteKey (
    char* writekey)
{
    EEPROM_readString(thingspeakWriteKey, sizeof(thingspeakWriteKey),
        writekey);
}

void EEPROMStorage_setFilterSampleTime (
    const uint8_t sampleTime)
{
    EEPROM_write(&filterSampleTime, sampleTime);
}

uint8_t EEPROMStorage_filterSampleTime (void)
{
    return EEPROM_read(&filterSampleTime);
}

void EEPROMStorage_setFilterSamples (
    const uint8_t samples)
{
    EEPROM_write(&numFilterSamples, samples);
}

uint8_t EEPROMStorage_filterSamples (void)
{
    return EEPROM_read(&numFilterSamples);
}

void EEPROMStorage_setFilterVariance (
    const uint16_t variance)
{
    EEPROM_writeWord(&filterVariance, variance);
}

uint16_t EEPROMStorage_filterVariance (void)
{
    return EEPROM_readWord(&filterVariance);
}

void EEPROMStorage_setIPConsoleEnabled (
    const bool enabled)
{
    EEPROM_write(&ipConsoleEnabled, enabled ? 1 : 0);
}

bool EEPROMStorage_ipConsoleEnabled (void)
{
    return EEPROM_read(&ipConsoleEnabled);
}

void EEPROMStorage_setIPConsoleServerAddress (
    const char* server)
{
    EEPROM_writeString(ipConsoleServerAddress, sizeof(ipConsoleServerAddress), server);
}

void EEPROMStorage_getIPConsoleServerAddress (
    char* server)
{
    EEPROM_readString(ipConsoleServerAddress, sizeof(ipConsoleServerAddress), server);
}

void EEPROMStorage_setIPConsoleServerPort (
    const uint16_t port)
{
    EEPROM_writeWord(&ipConsoleServerPort, port);
}

uint16_t EEPROMStorage_ipConsoleServerPort (void)
{
    return EEPROM_readWord(&ipConsoleServerPort);
}
