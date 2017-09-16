//
//  Sample History
//

#include "SampleHistory.h"

#include <stdlib.h>

void SampleHistory_insertSample (
	const SampleHistory_Sample* sample,
	SampleHistory_t* sampleHistory)
{
    // put sample in buffer
    sampleHistory->sampleBuffer[sampleHistory->tail] = *sample;
	
    // advance tail
    if (sampleHistory->tail >= (sampleHistory->capacity - 1)) {
        // wrap around
        sampleHistory->tail = 0;
    } else {
        ++sampleHistory->tail;
    }

    // increment length
    if (sampleHistory->length < sampleHistory->capacity) {
        ++sampleHistory->length;
    }
}

const SampleHistory_Sample* SampleHistory_getAt (
    const uint8_t at,
    SampleHistory_t* sampleHistory)
{
    if (at < sampleHistory->length) {
        int16_t index = ((int16_t)sampleHistory->tail) - (sampleHistory->length - at);
        if (index < 0) {
            index += sampleHistory->capacity;
        }
        return &sampleHistory->sampleBuffer[index];
    } else {
        return NULL;
    }
}
