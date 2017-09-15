//
//  Sample History
//

#include "SampleHistory.h"


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
