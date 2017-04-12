//
//  Analog to Digital Converter Manager
//
//  Used to reserve the ADC and do conversions
//
//  Platform: AtMega328P
//
#ifndef ADCMANAGER_H
#define ADCMANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>

// ADC voltage reference selection (use one only)
#define ADC_VOLTAGE_REF_AREF            0
#define ADC_VOLTAGE_REF_AVCC            1
#define ADC_VOLTAGE_REF_INTERNAL_1V1    3

// ADC single-ended input channels (use one only)
#define ADC_SINGLE_ENDED_INPUT_ADC0 0   // PC0
#define ADC_SINGLE_ENDED_INPUT_ADC1 1   // PC1
#define ADC_SINGLE_ENDED_INPUT_ADC2 2   // PC2
#define ADC_SINGLE_ENDED_INPUT_ADC3 3   // PC3
#define ADC_SINGLE_ENDED_INPUT_ADC4 4   // PC4
#define ADC_SINGLE_ENDED_INPUT_ADC5 5   // PC5
#define ADC_SINGLE_ENDED_INPUT_TEMP 8   // Temperature sensor
#define ADC_SINGLE_ENDED_INPUT_1V1  14  // I Ref
#define ADC_SINGLE_ENDED_INPUT_0V   15  // AGND

// called once at power-up
extern void ADCManager_Initialize (void);

// call once at the beginning of program after power-up for
// each channel you intend to use
extern void ADCManager_setupChannel (
    const uint8_t channel,  // one of ADC_SINGLE_ENDED_INPUT_xxx
    const uint8_t adcRef,   // one of ADC_VOLTAGE_REF_xxx
    const bool leftAdjustResult);

// schedules and reads the requested channel
// called in each iteration of the mainloop
extern void ADCManager_task (void);

// returns true and starts a conversion if the ADC is
// available (not in use by another caller).
// channel must be one of ADC_CHANNELn as defined in
// LUFA/Drivers/Peripheral/ADC.h
extern bool ADCManager_StartConversion (
    const uint8_t channelIndex);

// returns true when the conversion is complete and
// returns the analog value. only passes the value
// to the caller the first time it returns true
extern bool ADCManager_ConversionIsComplete (
    uint16_t* analogValue);

#endif  // ADCMANAGER_H
