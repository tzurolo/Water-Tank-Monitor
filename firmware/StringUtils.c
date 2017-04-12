//
//  String
//

#include "StringUtils.h"

#include <stdio.h>
#include <stdlib.h>

extern const char* StringUtils_scanDelimitedString (
    const char startDelimiter,
    const char endDelimiter,
    const char* sourcePtr,
    CharString_t* delimitedString)
{
    CharString_clear(delimitedString);
    bool isValid = false;
    const char* startingSourcePtr = sourcePtr;

    // advance to the next startDelimiter
    while ((*sourcePtr != 0) && (*sourcePtr != startDelimiter)) {
        ++sourcePtr;
    }
    if (*sourcePtr == startDelimiter) {
        // step over start delimiter
        ++sourcePtr;
        // read characters between delimiters
        while ((*sourcePtr != 0) && (*sourcePtr != endDelimiter)) {
            CharString_appendC(*sourcePtr, delimitedString);
            ++sourcePtr;
        }

        if (*sourcePtr == endDelimiter) {
            // step over closing delimiter
            ++sourcePtr;
            isValid = true;
        }
    }

    if (!isValid) {
        CharString_clear(delimitedString);
        sourcePtr = startingSourcePtr;
    }

    return sourcePtr;
}

const char* StringUtils_scanQuotedString (
    const char* sourcePtr,
    CharString_t* quotedString)
{
    return StringUtils_scanDelimitedString('"', '"', sourcePtr, quotedString);
}

const char* StringUtils_skipWhitespace (
    const char* sourcePtr)
{
    char ch;
    while ((ch = *sourcePtr) && ((ch == ' ') || (ch == '\t') || (ch == '\n'))) {
        ++sourcePtr;
    }

    return sourcePtr;
}

const char* StringUtils_scanInteger (
    const char* str,
    bool *isValid,
    int16_t *value)
{
    *isValid = false;

    int16_t workingValue = 0;
    const char* cp = str;

    char ch;
    bool gotDigit = false;
    while ((ch = *cp) && (ch >= '0') && (ch <= '9')) {
        workingValue = (workingValue * 10) + (ch - '0');
        ++cp;
        gotDigit = true;
    }

    if (gotDigit) {
        *isValid = true;
        *value = workingValue;
    }

    return cp;
}

const char* StringUtils_scanIntegerU32 (
    const char* str,
    bool *isValid,
    uint32_t *value)
{
    *isValid = false;

    uint32_t workingValue = 0;
    const char* cp = str;

    char ch;
    bool gotDigit = false;
    while ((ch = *cp) && (ch >= '0') && (ch <= '9')) {
        workingValue = (workingValue * 10) + (ch - '0');
        ++cp;
        gotDigit = true;
    }

    if (gotDigit) {
        *isValid = true;
        *value = workingValue;
    }

    return cp;
}

void StringUtils_scanDecimal (
    const char* str,
    bool *isValid,
    int16_t *value,
    uint8_t *numFractionalDigits)
{
    *isValid = true;

    int16_t workingValue = 0;
    uint8_t fractionalDigits = 0;
    bool gotDecimalPoint = false;
    const char* cp = str;

    // read minus sign
    bool isNegative = false;
    if (*cp == '-') {
        isNegative = true;
        ++cp;
    }

    char ch;
    while ((ch = *cp++) && *isValid) {
        if ((ch >= '0') && (ch <= '9')) {
            workingValue = (workingValue * 10) + (ch - '0');
            if (gotDecimalPoint) {
                ++fractionalDigits;
            }           
        } else if (ch == '.') {
            if (gotDecimalPoint) {
                *isValid = false;
            } else {
                gotDecimalPoint = true;
            }
        } else {
            *isValid = false;
        }
    }

    if (*isValid) {
        *value = (isNegative) ? -workingValue : workingValue;
        *numFractionalDigits = fractionalDigits;
    }
}

void StringUtils_appendDecimal (
    const int16_t value,
    const uint8_t minIntegerDigits,
    const uint8_t numFractionalDigits,
    CharString_t* destStr)
{
    char strBuffer[16];
    char* cp = &strBuffer[15];
    *cp-- = 0;  // null terminate

    uint16_t workingValue = (value < 0) ? -value : value;

    // working backwards, start with fractional digits
    if (numFractionalDigits > 0) {
        for (int f = 0; f < numFractionalDigits; ++f) {
            *cp-- = (workingValue % 10) + '0';
            workingValue /= 10;
        }
        *cp-- = '.';
    }

    // continue with integer digits
    for (int i = 0; (i < minIntegerDigits) || (workingValue != 0); ++i) {
        *cp-- = (workingValue % 10) + '0';
        workingValue /= 10;
    }

    // insert sign for negative value
    if (value < 0) {
        *cp-- = '-';
    }
    CharString_append(cp+1, destStr);
}

void StringUtils_appendDecimal32 (
    const int32_t value,
    const uint8_t minIntegerDigits,
    const uint8_t numFractionalDigits,
    CharString_t* destStr)
{
    char strBuffer[16];
    char* cp = &strBuffer[15];
    *cp-- = 0;  // null terminate

    uint32_t workingValue = (value < 0) ? -value : value;

    // working backwards, start with fractional digits
    if (numFractionalDigits > 0) {
        for (int f = 0; f < numFractionalDigits; ++f) {
            *cp-- = (workingValue % 10) + '0';
            workingValue /= 10;
        }
        *cp-- = '.';
    }

    // continue with integer digits
    for (int i = 0; (i < minIntegerDigits) || (workingValue != 0); ++i) {
        *cp-- = (workingValue % 10) + '0';
        workingValue /= 10;
    }

    // insert sign for negative value
    if (value < 0) {
        *cp-- = '-';
    }
    CharString_append(cp+1, destStr);
}

int StringUtils_lookupString (
    const CharString_t *str,
    PGM_P table[],
    const int tableSize)
{
    int first = 0;
    int last = tableSize - 1;
    int middle;

    while (first <= last) {
        middle = (first + last) / 2;
        PGM_P tableEntry = (PGM_P)pgm_read_word(&(table[middle]));
        const int comparison = CharString_compareP(str, tableEntry);
        if (comparison == 0) {
            // found at index middle;
            break;
        } else if (comparison > 0) {
            first = middle + 1;    
        } else {
            last = middle - 1;
        }
    }
    if (first > last) {
        // Not found!
        middle = tableSize;
    }

    return middle;
}
