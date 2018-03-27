//
// Command processor
//
// Interprets and executes commands from SMS or the console
//

#ifndef COMMANDPROCESSOR_H
#define COMMANDPROCESSOR_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "CharStringRange.h"

extern void CommandProcessor_processCommand (
    const char* command,
    const char* phoneNumber,
    const char* timestamp);

// creates the status message in CellularComm's outgoing
// SMS message text buffer
extern void CommandProcessor_createStatusMessage (
    CharString_t *msg);

#endif  // COMMANDPROCESSOR_H