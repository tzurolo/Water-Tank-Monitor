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
#include "StringUtils.h"
#include "Console.h"
#include "EEPROMStorage.h"

#define DEBUG_TRACE 1

// when to reboot daily
#define REBOOT_HOUR 12
#define REBOOT_MINUTES 23

#define REBOOT_FULL_DAY 8640000L

static volatile uint16_t tickCounter = 0;
static volatile SystemTime_t currentTime;
static int32_t timeAdjustment;
static bool shuttingDown = false;
static SystemTime_TickNotification notificationFunction;

void SystemTime_Initialize (void)
{
    tickCounter = 0;
    currentTime.seconds = EEPROMStorage_lastRebootTimeSec();
    currentTime.hundredths = 0;
    timeAdjustment = 0;
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

void SystemTime_getCurrentTime (
    SystemTime_t *curTime)
{
    // we disable interrupts during read of secondsSinceReset because
    // it is updated in an interrupt handler
    char SREGSave;
    SREGSave = SREG;
    cli();
    curTime->seconds = currentTime.seconds;
    curTime->hundredths = currentTime.hundredths;
    SREG = SREGSave;
}

void SystemTime_futureTime (
    const int hundredthsFromNow,
    SystemTime_t* futureTime)
{
    SystemTime_t curTime;
    SystemTime_getCurrentTime(&curTime);
    SystemTime_copy(&curTime, futureTime);
    futureTime->hundredths += hundredthsFromNow % 100;
    if (futureTime->hundredths >= 100) {
        // overflow - carry
        futureTime->hundredths -= 100;
        ++futureTime->seconds;
    }
    futureTime->seconds += hundredthsFromNow / 100;
}

bool SystemTime_timeHasArrived (
    const SystemTime_t* futureTime)
{
    SystemTime_t curTime;
    SystemTime_getCurrentTime(&curTime);
    return (curTime.seconds > futureTime->seconds)
        ? true
        : (curTime.seconds == futureTime->seconds)
            ? curTime.hundredths >= futureTime->hundredths
            : false;
}

void SystemTime_setTimeAdjustment (
    const uint32_t *newTime)
{
    char SREGSave;
    SREGSave = SREG;
    cli();
    timeAdjustment = *newTime - currentTime.seconds;
    SREG = SREGSave;
#if DEBUG_TRACE
    {
        CharString_define(20, msg);
        CharString_copyP(PSTR("Tadj: "), &msg);
        StringUtils_appendDecimal32(timeAdjustment, 1, 0, &msg);
        Console_printCS(&msg);
    }
#endif
}

void SystemTime_applyTimeAdjustment ()
{
    char SREGSave;
    SREGSave = SREG;
    cli();
    currentTime.seconds += timeAdjustment;
    timeAdjustment = 0;
    SREG = SREGSave;
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
        wdt_enable(wdtTimeout);
        WDTCSR |= (1 << WDIE);
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        cli();
        currentTime.seconds += secondsThisLoop;
        sleep_enable();
        sleep_bod_disable();

        sei();
        sleep_cpu();
        sleep_disable();
        sei();
    }
}

void SystemTime_commenceShutdown (void)
{
    shuttingDown = true;
    // store reboot time
    SystemTime_t curTime;
    SystemTime_getCurrentTime(&curTime);
    curTime.seconds += timeAdjustment;    // apply time adjustment
    curTime.seconds += 8;  // account for watchdog time
    EEPROMStorage_setLastRebootTimeSec(curTime.seconds);

    Console_printP(PSTR("shutting down..."));
    wdt_enable(WDTO_8S);
    wdt_reset();
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

uint8_t SystemTime_dayOfWeek (
    const SystemTime_t *time)
{
    return (time->seconds / 86400L) % 7;
}

uint8_t SystemTime_hours (
    const SystemTime_t *time)
{
    return (time->seconds / 3600) % 24;
}

uint8_t SystemTime_minutes (
    const SystemTime_t *time)
{
    return (time->seconds / 60) % 60;
}

uint8_t SystemTime_seconds (
    const SystemTime_t *time)
{
    return time->seconds % 60;
}

void SystemTime_appendCurrentToString (
    CharString_t* timeString)
{
    // get current time
    SystemTime_t curTime;
    SystemTime_getCurrentTime(&curTime);

    // append day of week
    StringUtils_appendDecimal(SystemTime_dayOfWeek(&curTime), 1, 0, timeString);
    CharString_appendP(PSTR(":"), timeString);

    // append hours
    StringUtils_appendDecimal(SystemTime_hours(&curTime), 2, 0, timeString);
    CharString_appendP(PSTR(":"), timeString);

    // append minutes
    StringUtils_appendDecimal(SystemTime_minutes(&curTime), 2, 0, timeString);
    CharString_appendP(PSTR(":"), timeString);

    // append seconds
    StringUtils_appendDecimal(SystemTime_seconds(&curTime), 2, 0, timeString);
}

ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
    ++tickCounter;
    if (tickCounter > (SYSTEMTIME_TICKS_PER_SECOND / 100)) {
        tickCounter = 0;
        ++currentTime.hundredths;
        if (currentTime.hundredths >= 100) {
            currentTime.hundredths = 0;
            ++currentTime.seconds;
        }
    }

    if (notificationFunction != NULL) {
        notificationFunction();
    }

}

ISR(WDT_vect, ISR_BLOCK)
{
}
