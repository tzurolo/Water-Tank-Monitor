#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <math.h>
#include <util/delay.h>

#include "intlimit.h"
#include "SystemTime.h"
#include "EEPROMStorage.h"
#include "ADCManager.h"
#include "BatteryMonitor.h"
#include "Console.h"
#include "CommandProcessor.h"
#include "BatteryMonitor.h"
#include "InternalTemperatureMonitor.h"
#include "CellularComm_SIM800.h"
#include "CellularTCPIP_SIM800.h"
#include "TCPIPConsole.h"
#include "SoftwareSerialRx.h"
#include "SoftwareSerialTx.h"
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
    SoftwareSerialTx_Initialize();
    SoftwareSerialRx_Initialize();
    Console_Initialize();
    CellularComm_Initialize();
    CellularTCPIP_Initialize();
    TCPIPConsole_Initialize();
    RAMSentinel_Initialize();
}
 
int main (void)
{
    Initialize();

    sei();

    for (;;) {
        // run all the tasks
        SystemTime_task();
        ADCManager_task();
        BatteryMonitor_task();
        InternalTemperatureMonitor_task();
        SIM800_task();
        Console_task();
        TCPIPConsole_task();
        CellularComm_task();
        if (!RAMSentinel_sentinelIntact()) {
            Console_printP(PSTR("stack collision!"));
            SystemTime_commenceShutdown();
        }

#if COUNT_MAJOR_CYCLES
        ++majorCycleCounter;
#endif
    }

    return (0);
}
