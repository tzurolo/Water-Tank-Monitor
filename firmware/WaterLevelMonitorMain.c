#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <math.h>
#include <util/delay.h>
#include <avr/power.h>

#include "intlimit.h"
#include "SystemTime.h"
#include "EEPROMStorage.h"
#include "ADCManager.h"
#include "BatteryMonitor.h"
#include "Console.h"
#include "CommandProcessor.h"
#include "BatteryMonitor.h"
#include "InternalTemperatureMonitor.h"
#include "UltrasonicSensorMonitor.h"
#include "CellularComm_SIM800.h"
#include "CellularTCPIP_SIM800.h"
#include "TCPIPConsole.h"
#include "SoftwareSerialRx0.h"
#include "SoftwareSerialRx2.h"
#include "SoftwareSerialTx.h"
#include "WaterLevelMonitor.h"
#include "RAMSentinel.h"

/** Configures the board hardware and chip peripherals for the demo's functionality. */
static void Initialize (void)
{
    // enable watchdog timer
    wdt_enable(WDTO_500MS);

    SystemTime_Initialize();
    EEPROMStorage_Initialize();
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
    WaterLevelMonitor_Initialize();
    RAMSentinel_Initialize();
}
 
int main (void)
{
    Initialize();

    sei();

    for (;;) {
        if (!RAMSentinel_sentinelIntact()) {
            Console_printP(PSTR("stack collision!"));
            SystemTime_commenceShutdown();
        }

        // run all the tasks
        SystemTime_task();
        ADCManager_task();
        BatteryMonitor_task();
        InternalTemperatureMonitor_task();
        UltrasonicSensorMonitor_task();
        SIM800_task();
        Console_task();
        TCPIPConsole_task();
        CellularComm_task();
        WaterLevelMonitor_task();

        if (WaterLevelMonitor_taskIsDone()) {
            Console_printP(PSTR("done"));

            // apply calibration to WDT

            // apply adjustment from server time
            SystemTime_applyTimeAdjustment();

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

            // compute how long to sleep until the next sample
            const uint16_t sampleInterval = EEPROMStorage_sampleInterval();
            SystemTime_t curTime;
            SystemTime_getCurrentTime(&curTime);
            SystemTime_t nextSampleTime;
            nextSampleTime.seconds =
                (((curTime.seconds + (sampleInterval / 2)) / sampleInterval) + 1) * sampleInterval;
            nextSampleTime.hundredths = 0;
            int32_t sec32 = SystemTime_diffSec(&nextSampleTime, &curTime);
            uint16_t sleepTime = (sec32 > 65535)
                ? 65535
                : (sec32 < 0)
                    ? 0
                    : ((uint16_t)sec32);

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

            WaterLevelMonitor_resume();
        }

#if COUNT_MAJOR_CYCLES
        ++majorCycleCounter;
#endif
    }

    return (0);
}
