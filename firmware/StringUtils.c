//
//  String
//

#include "StringUtils.h"

#include <stdio.h>
#include <stdlib.h>

void StringUtils_scanDelimitedString (
    const char startDelimiter,
    const char endDelimiter,
    CharStringSpan_t* source,
    CharStringSpan_t* delimitedSpan)
{
    CharStringSpan_clear(delimitedSpan);
    CharStringSpan_Iter iter = CharStringSpan_begin(source);
    CharStringSpan_Iter end = CharStringSpan_end(source);

    // advance to the next startDelimiter
    while ((iter != end) && (*iter != startDelimiter)) {
        ++iter;
    }
    if (*iter == startDelimiter) {
        // step over start delimiter
        ++iter;
        CharStringSpan_Iter delimitedStartIter = iter;
        // read characters between delimiters
        while ((iter != end) && (*iter != endDelimiter)) {
            ++iter;
        }

        if (*iter == endDelimiter) {
            // we have a valid delimited string
            CharStringSpan_set(delimitedStartIter, iter, delimitedSpan);
            // step over closing delimiter
            ++iter;
            CharStringSpan_setBegin(iter, source);
        }
    }
}

void StringUtils_scanQuotedString (
    CharStringSpan_t* source,
    CharStringSpan_t* quotedSpan)
{
    StringUtils_scanDelimitedString('"', '"', source, quotedSpan);
}

void StringUtils_skipWhitespace (
    CharStringSpan_t* source)
{
    CharStringSpan_Iter iter = CharStringSpan_begin(source);
    CharStringSpan_Iter end = CharStringSpan_end(source);
    char ch;
    while ((iter != end) && (ch = *iter) && ((ch == ' ') || (ch == '\t') || (ch == '\n'))) {
        ++iter;
    }
    CharStringSpan_setBegin(iter, source);
}

void StringUtils_scanInteger (
    CharStringSpan_t* source,
    bool *isValid,
    int16_t *value)
{
    *isValid = false;

    int16_t workingValue = 0;
    CharStringSpan_Iter iter = CharStringSpan_begin(source);
    CharStringSpan_Iter end = CharStringSpan_end(source);

    char ch;
    bool gotDigit = false;
    while ((iter != end) && (ch = *iter) && (ch >= '0') && (ch <= '9')) {
        workingValue = (workingValue * 10) + (ch - '0');
        ++iter;
        gotDigit = true;
    }

    if (gotDigit) {
        *isValid = true;
        *value = workingValue;
    }
    CharStringSpan_setBegin(iter, source);
}

void StringUtils_scanIntegerU32 (
    CharStringSpan_t* source,
    bool *isValid,
    uint32_t *value)
{
    *isValid = false;

    uint32_t workingValue = 0;
    CharStringSpan_Iter iter = CharStringSpan_begin(source);
    CharStringSpan_Iter end = CharStringSpan_end(source);

    char ch;
    bool gotDigit = false;
    while ((iter != end) && (ch = *iter) && (ch >= '0') && (ch <= '9')) {
        workingValue = (workingValue * 10) + (ch - '0');
        ++iter;
        gotDigit = true;
    }

    if (gotDigit) {
        *isValid = true;
        *value = workingValue;
    }
    CharStringSpan_setBegin(iter, source);
}

void StringUtils_scanDecimal (
    CharStringSpan_t* source,
    bool *isValid,
    int16_t *value,
    uint8_t *numFractionalDigits)
{
    *isValid = true;

    int16_t workingValue = 0;
    uint8_t fractionalDigits = 0;
    bool gotDecimalPoint = false;
    CharStringSpan_Iter iter = CharStringSpan_begin(source);
    CharStringSpan_Iter end = CharStringSpan_end(source);

    // read minus sign
    bool isNegative = false;
    if (*iter == '-') {
        isNegative = true;
        ++iter;
    }

    char ch;
    while ((iter != end) && (ch = *iter++) && *isValid) {
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
    const CharStringSpan_t *str,
    PGM_P table[],
    const int tableSize)
{
    int first = 0;
    int last = tableSize - 1;
    int middle;

    while (first <= last) {
        middle = (first + last) / 2;
        PGM_P tableEntry = (PGM_P)pgm_read_word(&(table[middle]));
        const int comparison = CharStringSpan_compareP(str, tableEntry);
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
