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

// buffer that clients can use to accumulate command characters
extern CharString_t CommandProcessor_incomingCommand;

// output from commands is put into this string
extern CharString_t CommandProcessor_commandReply;

// creates the status message in CommandProcessor_commandReply
extern void CommandProcessor_createStatusMessage (
    CharString_t *msg);

extern void CommandProcessor_executeCommand (
    const char* command,
    const char* phoneNumber,
    const char* timestamp);


#endif  // COMMANDPROCESSOR_H