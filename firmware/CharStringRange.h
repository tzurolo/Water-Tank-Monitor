//
//  Character String Range
//
//  What it does:
//    Provides a range (i.e. substring) of characters within a CharString
//
//  How to use it:
//    Define a Range from a CharString like this:
//       CharStringRange_init(str);
//
#ifndef CHARSTRINGRANGE_H
#define CHARSTRINGRANGE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <avr/pgmspace.h>
#include "CharString.h"

typedef const char* CharStringRange_Iter;

typedef struct CharStringRange_struct {
    CharStringRange_Iter m_begin;
    CharStringRange_Iter m_end; // non-inclusive
} CharStringRange_t;

inline void CharStringRange_init (
    const CharString_t *str,
    CharStringRange_t *range)
{
    range->m_begin = &str->body[0];
    range->m_end   = &str->body[str->length];
}

inline CharStringRange_Iter CharStringRange_begin (
    CharStringRange_t *range)
{
    return range->m_begin;
}

inline CharStringRange_Iter CharStringRange_end (
    CharStringRange_t *range)
{
    return range->m_end;
}

inline bool CharStringRange_isEmpty (
    const CharStringRange_t* range)
{
    return (range->m_begin == range->m_end);
}

inline uint8_t CharStringRange_length (
    const CharStringRange_t* range)
{
    return (uint8_t)(range->m_end - range->m_begin);
}

inline bool CharStringRange_equalsP (
    const CharStringRange_t* leftRange,
    PGM_P rightStr)
{
    const int leftLen = CharStringRange_length(leftRange);
    return
        (leftLen == strlen_P(rightStr)) &&
        (strncmp_P(leftRange->m_begin, rightStr, leftLen) == 0);
}

// returns > 0 for greater
//         = 0 for equal
//         < 0 for less
inline int CharStringRange_compareP (
    const CharStringRange_t* leftRange,
    PGM_P rightStr)
{
    const int leftLen = CharStringRange_length(leftRange);
    const int rightLen = strlen_P(rightStr);
    if (leftLen == rightLen) {
        return strncmp_P(leftRange->m_begin, rightStr, leftLen);
    } else {
        return 0;   // TODO
    }
}

inline bool CharStringRange_startsWithP (
    const CharStringRange_t* range,
    PGM_P prefix)
{
    const int prefixLen = strlen_P(prefix);
    return
        (CharStringRange_length(range) >= prefixLen) &&
        (strncmp_P(range->m_begin, prefix, prefixLen) == 0);
}

#endif  // CHARSTRINGRANGE_H