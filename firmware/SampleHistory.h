//
//  Sample History
//
//  The object defined here is used to retain the latest
//  n sample readings, and provide a min, max, and
//  average value over the readings
//
#ifndef SAMPLEHISTORY_H
#define SAMPLEHISTORY_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t relSampleTime; // relative to sampling start time
    uint16_t waterDistance;
    uint8_t temperature;
} SampleHistory_Sample;

typedef struct {
    uint8_t tail;
    uint8_t length;
    uint8_t capacity;
    SampleHistory_Sample* sampleBuffer;
} SampleHistory_t;

#define SampleHistory_define(capacity, name) \
    SampleHistory_Sample name##_buf[capacity] = {{0, 0, 0}}; \
    SampleHistory_t name = {0, 0, capacity, name##_buf};

inline void SampleHistory_clear (
    SampleHistory_t* sampleHistory)
{
    sampleHistory->tail = 0;
    sampleHistory->length = 0;
}

extern void SampleHistory_insertSample (
    const SampleHistory_Sample* sample,
    SampleHistory_t* sampleHistory);

inline uint8_t SampleHistory_length (
    SampleHistory_t* sampleHistory)
{
    return sampleHistory->length;
}

#endif		// SAMPLEHISTORY_H