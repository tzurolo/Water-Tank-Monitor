//
//  Battery Monitor
//
//  Monitors the state of the System battery. Measures battery voltage
//  downstream of the FET power switch
//
//  Pin usage:
//     ADC0 (PA0) - input from battery voltage divider
//

#include "BatteryMonitor.h"

#include "ADCManager.h"
#include "SystemTime.h"
#include "DataHistory.h"
#include "EEPROMStorage.h"

#define BATTERY_ADC_CHANNEL ADC_SINGLE_ENDED_INPUT_ADC0

#define BATTERY_DIVIDER_R1 9.71
#define BATTERY_DIVIDER_R2 21.69
#define REFERENCE_VOLTAGE 3.368

#define RESISTOR_DIVIDER_COUNTS(VIN, REF, R1, R2) ((uint16_t)((((R1/(R1+R2))*VIN)/REF)*1024+0.5))
#define BATTERY_VOLTAGE_COUNTS(VIN) RESISTOR_DIVIDER_COUNTS(VIN, REFERENCE_VOLTAGE, BATTERY_DIVIDER_R1, BATTERY_DIVIDER_R2)

#define BATTERY_3V5  BATTERY_VOLTAGE_COUNTS(3.5)
#define BATTERY_4V5  BATTERY_VOLTAGE_COUNTS(4.5)

#define RESOLUTION 1000
#define SCALE 100
#define NUMERATOR ((int32_t)(((BATTERY_DIVIDER_R1 + BATTERY_DIVIDER_R2) / BATTERY_DIVIDER_R1) * (REFERENCE_VOLTAGE / 1024.0) * RESOLUTION * SCALE))

#define BATTERY_VOLTAGE_SAMPLES 10
// sample 10 times per second (10 1/100ths)
#define BATTERY_VOLTAGE_SAMPLE_TIME 10

typedef enum {
    bms_idle,
    bms_waitingForADCStart,
    bms_waitingForADCCompletion
} BatteryMonitor_state;

static BatteryMonitor_batteryStatus battStatus = bs_unknown;
static BatteryMonitor_state bmState = bms_idle;
static SystemTime_t sampleTimer;
DataHistory_define(BATTERY_VOLTAGE_SAMPLES, batteryVoltageHistory);

void BatteryMonitor_Initialize (void)
{
    bmState = bms_idle;
    battStatus = bs_unknown;
    // start sampling in 1/50 second (20ms, to let power stabilize)
    SystemTime_futureTime(2, &sampleTimer);
    DataHistory_clear(&batteryVoltageHistory);

    // set up the ADC channel for measuring battery voltage
    ADCManager_setupChannel(BATTERY_ADC_CHANNEL, ADC_VOLTAGE_REF_AVCC, false);
}

bool BatteryMonitor_haveValidSample (void)
{
    return DataHistory_length(&batteryVoltageHistory) >= BATTERY_VOLTAGE_SAMPLES;
}

BatteryMonitor_batteryStatus BatteryMonitor_currentStatus (void)
{
    return battStatus;
}

int16_t BatteryMonitor_currentVoltage (void)
{
    uint16_t batteryVoltage = 0;
    if (DataHistory_length(&batteryVoltageHistory) >= BATTERY_VOLTAGE_SAMPLES) {
        uint16_t minVoltage;
        uint16_t maxVoltage;
        DataHistory_getStatistics(
            &batteryVoltageHistory, BATTERY_VOLTAGE_SAMPLES,
            &minVoltage, &maxVoltage, &batteryVoltage);
    }

    int32_t vBatt = batteryVoltage;
    // counts to 1/100s volt
    return ((((int32_t)vBatt) * NUMERATOR) / RESOLUTION);
}

void BatteryMonitor_task (void)
{
    switch (bmState) {
        case bms_idle :
            if (SystemTime_timeHasArrived(&sampleTimer)) {
                SystemTime_futureTime(BATTERY_VOLTAGE_SAMPLE_TIME, &sampleTimer);
                bmState = bms_waitingForADCStart;
            }
            break;
        case bms_waitingForADCStart :
            if (ADCManager_StartConversion(BATTERY_ADC_CHANNEL)) {
                // successfully started conversion
                bmState = bms_waitingForADCCompletion;
            }
            break;    
        case bms_waitingForADCCompletion : {
            uint16_t batteryVoltage;
            if (ADCManager_ConversionIsComplete(&batteryVoltage)) {
                // apply calibration
                const uint16_t batteryVoltageCalibrated =
                    (((uint32_t)batteryVoltage) * EEPROMStorage_batteryVoltageCal()) / 100;

                DataHistory_insertValue(batteryVoltageCalibrated, &batteryVoltageHistory);

                if (DataHistory_length(&batteryVoltageHistory) >=
                        BATTERY_VOLTAGE_SAMPLES) {
                    uint16_t minVoltage;
                    uint16_t maxVoltage;
                    uint16_t avgVoltage;
                    DataHistory_getStatistics(
                        &batteryVoltageHistory, BATTERY_VOLTAGE_SAMPLES,
                        &minVoltage, &maxVoltage, &avgVoltage);
                    // determine status based on voltage reading
                    if (maxVoltage < BATTERY_3V5) {
                        battStatus = bs_lowVoltage;
                    } else if (maxVoltage < BATTERY_4V5) {
                        battStatus = bs_goodVoltage;
                    } else {
                        battStatus = bs_fullVoltage;
                    }
                }
                bmState = bms_idle;
            }
            }
            break;
    }
}

