//
//  Ultrasonic Sensor Monitor
//
//  Uses the AtMega328P's UART to read the serial data stream from the sensor
//
//  Pin usage:
//      PD0 Rx
//

#include "UltrasonicSensorMonitor.h"

#include "UART_async.h"
#include "SystemTime.h"
#include "CharString.h"
#include "StringUtils.h"
#include "DataHistory.h"

#define SENSOR_SAMPLES 5
#define SENSOR_INVALID_RANGE 5000

CharString_define(16, sensorDataStr);
DataHistory_define(SENSOR_SAMPLES, distanceHistory);
static uint8_t numReadingsToIgnore;

void UltrasonicSensorMonitor_Initialize (void)
{
    UART_init(true);
    UART_set_baud_rate(9600);
    DataHistory_clear(&distanceHistory);

    // ignore the first readings - they are not filtered
    numReadingsToIgnore = 5;
}

bool UltrasonicSensorMonitor_haveValidSample (void)
{
    return DataHistory_length(&distanceHistory) >= SENSOR_SAMPLES;
}

int16_t UltrasonicSensorMonitor_currentDistance (void)
{
    uint16_t avgDistance = 0;
    if (DataHistory_length(&distanceHistory) >= SENSOR_SAMPLES) {
        uint16_t minTemmp;
        uint16_t maxTemp;
        DataHistory_getStatistics(
            &distanceHistory, SENSOR_SAMPLES,
            &minTemmp, &maxTemp, &avgDistance);
    }

    return avgDistance;
}

void UltrasonicSensorMonitor_task (void)
{
    char byte;
    while (UART_read_byte(&byte)) {
        if (byte == 13) {
            // end of string. Interpret
            if ((CharString_length(&sensorDataStr) == 5) &&
                (CharString_at(&sensorDataStr, 0) == 'R')) {
                CharStringSpan_t decimalStr;
                CharStringSpan_initRight(&sensorDataStr, 1, &decimalStr);
                bool isValid = false;
                int16_t dist = 0;
                uint8_t numFracDigits = 0;
                StringUtils_scanDecimal(&decimalStr, &isValid, &dist, &numFracDigits, NULL);
                if (isValid &&
					(dist > 0) &&
					(dist < SENSOR_INVALID_RANGE)) {
                    if (numReadingsToIgnore > 0) {
                        // ignoring first readings, which are not filtered
                        --numReadingsToIgnore;
                    } else {
                        DataHistory_insertValue(dist, &distanceHistory);
                    }
                }
            }
            CharString_clear(&sensorDataStr);
        } else {
            CharString_appendC(byte, &sensorDataStr);
        }
    }
}

