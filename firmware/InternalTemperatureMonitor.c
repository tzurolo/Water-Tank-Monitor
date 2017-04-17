//
//  Battery Monitor
//
//  Reads the AtMega328P's internal temperature sensor
//

#include "InternalTemperatureMonitor.h"

#include "ADCManager.h"
#include "SystemTime.h"
#include "DataHistory.h"

#define SENSOR_ADC_CHANNEL ADC_SINGLE_ENDED_INPUT_TEMP

#define REFERENCE_VOLTAGE 1.1

#define RESOLUTION 1000
#define SCALE 100
#define NUMERATOR (((BATTERY_DIVIDER_R1 + BATTERY_DIVIDER_R2) / BATTERY_DIVIDER_R1) * (1.1 / 1024.0) * RESOLUTION * SCALE)

#define SENSOR_SAMPLES 10
// sample 10 times per second (10 1/100ths)
#define SENSOR_SAMPLE_TIME 10

typedef enum {
    tms_idle,
    tms_waitingForADCStart,
    tms_waitingForADCCompletion
} TemperatureMonitor_state;

static TemperatureMonitor_state tmState = tms_idle;
static SystemTime_t sampleTimer;
DataHistory_define(SENSOR_SAMPLES, temperatureHistory);

void InternalTemperatureMonitor_Initialize (void)
{
    tmState = tms_idle;
    // start sampling in 1/50 second (20ms, to let power stabilize)
    SystemTime_futureTime(2, &sampleTimer);
    DataHistory_clear(&temperatureHistory);

    // set up the ADC channel for measuring battery voltage
    ADCManager_setupChannel(SENSOR_ADC_CHANNEL, ADC_VOLTAGE_REF_INTERNAL_1V1, false);
}

int16_t InternalTemperatureMonitor_currentTemperature (void)
{
    uint16_t avgTemperature = 0;
    if (DataHistory_length(&temperatureHistory) >= SENSOR_SAMPLES) {
        uint16_t minTemmp;
        uint16_t maxTemp;
        DataHistory_getStatistics(
            &temperatureHistory, SENSOR_SAMPLES,
            &minTemmp, &maxTemp, &avgTemperature);
    }

    int32_t temp = avgTemperature;
    // counts to degrees C
    return ((int16_t)temp);
}

void InternalTemperatureMonitor_task (void)
{
    switch (tmState) {
        case tms_idle :
            if (SystemTime_timeHasArrived(&sampleTimer)) {
                SystemTime_futureTime(SENSOR_SAMPLE_TIME, &sampleTimer);
                tmState = tms_waitingForADCStart;
            }
            break;
        case tms_waitingForADCStart :
            if (ADCManager_StartConversion(SENSOR_ADC_CHANNEL)) {
                // successfully started conversion
                tmState = tms_waitingForADCCompletion;
            }
            break;    
        case tms_waitingForADCCompletion : {
            uint16_t batteryVoltage;
            if (ADCManager_ConversionIsComplete(&batteryVoltage)) {
                DataHistory_insertValue(batteryVoltage, &temperatureHistory);

                tmState = tms_idle;
            }
            }
            break;
    }
}

