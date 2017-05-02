//
//  System Time
//
//  Uses AtMega328P 16 bit timer/counter 1
//
#include "SystemTime.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include "CellularComm_SIM800.h"
#include "StringUtils.h"
#include "Console.h"

// when to reboot daily
#define REBOOT_HOUR 12
#define REBOOT_MINUTES 23

#define REBOOT_FULL_DAY 8640000L

static volatile uint16_t tickCounter = 0;
static volatile SystemTime_t hundredthsSinceReset = 0;
static bool shuttingDown = false;
static SystemTime_TickNotification notificationFunction;

void SystemTime_Initialize (void)
{
    tickCounter = 0;
    hundredthsSinceReset = 0;
    notificationFunction = 0;

    // set up timer3 to fire interrupt once per second
    TCCR1B = (TCCR1B & 0xF8) | 2; // prescale by 8
    TCCR1B = (TCCR1B & 0xE7) | (1 << 3); // set CTC mode
    OCR1A = (F_CPU / 8) / SYSTEMTIME_TICKS_PER_SECOND;
    TCNT1 = 0;  // start the time counter at 0
    TIFR1 |= (1 << OCF1A);  // "clear" the timer compare flag
    TIMSK1 |= (1 << OCIE1A);// enable timer compare match interrupt
}

void SystemTime_registerForTickNotification (
    SystemTime_TickNotification notificationFcn)
{
    notificationFunction = notificationFcn;
}

uint16_t currentTick (void)
{
    uint16_t tick;

    char SREGSave = SREG;
    cli();
    tick = tickCounter;
    SREG = SREGSave;

    return tick;
}

void SystemTime_getCurrentTime (
    SystemTime_t *curTime)
{
    // we disable interrupts during read of secondsSinceReset because
    // it is updated in an interrupt handler
    char SREGSave;
    SREGSave = SREG;
    cli();
    *curTime = hundredthsSinceReset;
    SREG = SREGSave;
}

void SystemTime_futureTime (
    const int hundredthsFromNow,
    SystemTime_t* futureTime)
{
    SystemTime_t currentTime;
    SystemTime_getCurrentTime(&currentTime);
    *futureTime = currentTime + (SystemTime_t)hundredthsFromNow;
}

bool SystemTime_timeHasArrived (
    const SystemTime_t* time)
{
    bool timeHasArrived = false;

    SystemTime_t currentTime;
    SystemTime_getCurrentTime(&currentTime);
    if (currentTime >= (*time)) {
        timeHasArrived = true;
    }
    
    return timeHasArrived;
}

void SystemTime_sleepFor (
    const uint16_t seconds)
{
    uint16_t secondsRemaining = seconds;
    while (secondsRemaining > 0) {
        uint8_t wdtTimeout;
        uint8_t secondsThisLoop;
        if (secondsRemaining >= 8) {
            wdtTimeout = WDTO_8S;
            secondsThisLoop = 8;
        } else if (secondsRemaining >= 4) {
            wdtTimeout = WDTO_4S;
            secondsThisLoop = 4;
        } else if (secondsRemaining >= 2) {
            wdtTimeout = WDTO_2S;
            secondsThisLoop = 2;
        } else {
            wdtTimeout = WDTO_1S;
            secondsThisLoop = 1;
        }
            secondsRemaining -= secondsThisLoop;
            ADCSRA &= ~(1 << ADEN);
            wdt_enable(wdtTimeout);
            WDTCSR |= (1 << WDIE);
             set_sleep_mode(SLEEP_MODE_PWR_DOWN);
              cli();
              hundredthsSinceReset += secondsThisLoop * 100;
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

    }
    power_all_enable();
}

void SystemTime_commenceShutdown (void)
{
    shuttingDown = true;
    Console_printP(PSTR("shutting down..."));
    wdt_enable(WDTO_8S);
}

bool SystemTime_shuttingDown (void)
{
    return shuttingDown;
}

void SystemTime_task (void)
{
    // reset the watchdog timer
    if (shuttingDown) {
//        LED_OUTPORT |= (1 << LED_PIN);
    } else {
        wdt_reset();
#if 0
        // reboot daily at a predetermined time
        const CellularComm_NetworkTime* currentTime =
	        CellularComm_currentTime();
        SystemTime_t currentTimeHundredths;
        SystemTime_getCurrentTime(&currentTimeHundredths);
        if (((currentTime->hour == REBOOT_HOUR) &&
            (currentTime->minutes == REBOOT_MINUTES) &&
            (currentTimeHundredths > 720000L) &&    // running at least two hours
            CellularComm_isIdle()) ||
            (currentTimeHundredths > REBOOT_FULL_DAY)) {
            SystemTime_commenceShutdown();
        }
#endif
    }
}

void SystemTime_appendCurrentToString (
    CharString_t* timeString)
{

    // get current time
    SystemTime_t curTimeHundredths;
    SystemTime_getCurrentTime(&curTimeHundredths);
    uint32_t curTimeSeconds = curTimeHundredths / 100;

    // append hours
    const uint8_t hours = curTimeSeconds / 3600;
    StringUtils_appendDecimal(hours, 2, 0, timeString);
    CharString_appendP(PSTR(":"), timeString);

    // append minutes
    curTimeSeconds = (curTimeSeconds % 3600);
    const uint8_t minutes =  curTimeSeconds / 60;
    StringUtils_appendDecimal(minutes, 2, 0, timeString);
    CharString_appendP(PSTR(":"), timeString);

    // append seconds
    const uint8_t seconds = (curTimeSeconds % 60);
    StringUtils_appendDecimal(seconds, 2, 0, timeString);
}

ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
    ++tickCounter;
    if (tickCounter > (SYSTEMTIME_TICKS_PER_SECOND / 100)) {
        tickCounter = 0;
        ++hundredthsSinceReset;
    }

    if (notificationFunction != NULL) {
        notificationFunction();
    }

}

ISR(WDT_vect, ISR_BLOCK)
{
}
