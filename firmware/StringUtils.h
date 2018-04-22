//
//  String Utilities
//
//  Utility functions for operating on strings, with overrun protection
//
#ifndef StringUtils_H
#define StringUtils_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <avr/pgmspace.h>
#include "CharStringSpan.h"

// scans for startDelimiter and then puts everything up to endDelimiter in
// delimitedString. returns the updated source ptr. returns empty string
// in delimitedString if delimiters not found. Updates source to beyond
// the end of the delimited string
extern void StringUtils_scanDelimitedString (
    const char startDelimiter,
    const char endDelimiter,
    CharStringSpan_t* source,
    CharStringSpan_t* delimitedSpan);

// scans for " and then puts everything up to the next " in
// quotedString. returns the updated source ptr
extern void StringUtils_scanQuotedString (
    CharStringSpan_t* source,
    CharStringSpan_t* quotedSpan);

extern void StringUtils_skipWhitespace (
    CharStringSpan_t* source);

// scans for digits, if any, and returns the integer value in 'value'.
// returns the updated source ptr
extern void StringUtils_scanInteger (
    CharStringSpan_t* source,
    bool *isValid,
    int16_t *value);
extern void StringUtils_scanIntegerU32 (
    CharStringSpan_t* source,
    bool *isValid,
    uint32_t *value);

// value returned is in units of 1/10^numFractionalDigits
extern void StringUtils_scanDecimal (
    CharStringSpan_t* source,
    bool *isValid,
    int16_t *value,
    uint8_t *numFractionalDigits);

// appends the decimal string for the given value to destStr
extern void StringUtils_appendDecimal (
    const int16_t value,
    const uint8_t minIntegerDigits,
    const uint8_t numFractionalDigits,
    CharString_t* destStr);
extern void StringUtils_appendDecimal32 (
    const int32_t value,
    const uint8_t minIntegerDigits,
    const uint8_t numFractionalDigits,
    CharString_t* destStr);

// returns index of match (0..tableSize-1), or tableSize if not found
extern int StringUtils_lookupString (
    const CharStringSpan_t *str,
    PGM_P table[],
    const int tableSize);

#endif  // StringUtils_H