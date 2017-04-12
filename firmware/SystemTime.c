//
//  System Time
//
//  Uses AtMega328P 16 bit timer/counter 1
//
#include "SystemTime.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include "CellularComm_SIM800.h"
#include "StringUtils.h"

// when to reboot daily
#define REBOOT_HOUR 12
#define REBOOT_MINUTES 23

#define REBOOT_FULL_DAY 86400L

static volatile uint16_t tickCounter = 0;
static volatile SystemTime_t secondsSinceReset = 0;
static bool shuttingDown = false;
static SystemTime_TickNotification notificationFunction;

void SystemTime_Initialize (void)
{
    tickCounter = 0;
    secondsSinceReset = 0;
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
    *curTime = secondsSinceReset;
    SREG = SREGSave;
}

void SystemTime_futureTime (
    const int secondsFromNow,
    SystemTime_t* futureTime)
{
    SystemTime_t currentTime;
    SystemTime_getCurrentTime(&currentTime);
    *futureTime = currentTime + (SystemTime_t)secondsFromNow;
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

void SystemTime_commenceShutdown (void)
{
    shuttingDown = true;
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
        // blink the LED
        const uint16_t blinkTime =
            CellularComm_isEnabled()
            ? (CellularComm_isRegistered()
                ? (SYSTEMTIME_TICKS_PER_SECOND / 20)
                : (SYSTEMTIME_TICKS_PER_SECOND / 2))
            : (SYSTEMTIME_TICKS_PER_SECOND - (SYSTEMTIME_TICKS_PER_SECOND / 10)); // about 0.9 second
        if (currentTick() < blinkTime) {
            LED_OUTPORT |= (1 << LED_PIN);
        } else {
            LED_OUTPORT &= ~(1 << LED_PIN);
        }
#endif
#if 0
        // reboot daily at a predetermined time
        const CellularComm_NetworkTime* currentTime =
	        CellularComm_currentTime();
        SystemTime_t currentTimeSeconds;
        SystemTime_getCurrentTime(&currentTimeSeconds);
        if (((currentTime->hour == REBOOT_HOUR) &&
            (currentTime->minutes == REBOOT_MINUTES) &&
            (currentTimeSeconds > 7200) &&	// running at least two hours
            CellularComm_isIdle()) ||
            (currentTimeSeconds > REBOOT_FULL_DAY)) {
            SystemTime_commenceShutdown();
        }
#endif
    }
}

void SystemTime_appendCurrentToString (
    CharString_t* timeString)
{

    // get current time
    SystemTime_t curTime;
    SystemTime_getCurrentTime(&curTime);

    // append hours
    const uint8_t hours = curTime / 3600;
    StringUtils_appendDecimal(hours, 2, 0, timeString);
    CharString_appendP(PSTR(":"), timeString);

    // append minutes
    curTime = (curTime % 3600);
    const uint8_t minutes =  curTime / 60;
    StringUtils_appendDecimal(minutes, 2, 0, timeString);
    CharString_appendP(PSTR(":"), timeString);

    // append seconds
    const uint8_t seconds = (curTime % 60);
    StringUtils_appendDecimal(seconds, 2, 0, timeString);
}

ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
    ++tickCounter;
    if (tickCounter >= SYSTEMTIME_TICKS_PER_SECOND) {
        tickCounter = 0;
        ++secondsSinceReset;
    }

    if (notificationFunction != NULL) {
        notificationFunction();
    }

}
