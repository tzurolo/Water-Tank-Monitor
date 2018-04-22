//
//  Character String Span
//
//  What it does:
//    Provides a span (i.e. substring) of characters within a CharString
//
//  How to use it:
//    Define a Span from a CharString like this:
//       CharStringSpan_init(str);
//
#ifndef CHARSTRINGSPAN_H
#define CHARSTRINGSPAN_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <avr/pgmspace.h>
#include "CharString.h"

typedef const char* CharStringSpan_Iter;

typedef struct CharStringSpan_struct {
    CharStringSpan_Iter m_begin;
    CharStringSpan_Iter m_end; // non-inclusive
} CharStringSpan_t;

inline void CharStringSpan_init (
    const CharString_t *str,
    CharStringSpan_t *span)
{
    span->m_begin = &str->body[0];
    span->m_end   = &str->body[str->length];
}

inline void CharStringSpan_initRight (
    const CharString_t *str,
    const int offset,
    CharStringSpan_t *span)
{
    span->m_begin = &str->body[offset];
    span->m_end   = &str->body[str->length];
}

inline void CharStringSpan_clear (
    CharStringSpan_t *span)
{
    span->m_begin = 0;
    span->m_end = 0;
}

inline CharStringSpan_Iter CharStringSpan_begin (
    const CharStringSpan_t *span)
{
    return span->m_begin;
}

inline CharStringSpan_Iter CharStringSpan_end (
    const CharStringSpan_t *span)
{
    return span->m_end;
}

inline char CharStringSpan_front(
    const CharStringSpan_t *span)
{
    return *span->m_begin;
}

inline void CharStringSpan_setBegin (
    const CharStringSpan_Iter begin,
    CharStringSpan_t *span)
{
    span->m_begin = begin;
}

inline void CharStringSpan_setEnd (
    const CharStringSpan_Iter end,
    CharStringSpan_t *span)
{
    span->m_end = end;
}

inline void CharStringSpan_set (
    const CharStringSpan_Iter begin,
    const CharStringSpan_Iter end,
    CharStringSpan_t *span)
{
    span->m_begin = begin;
    span->m_end = end;
}

inline void CharStringSpan_incrBegin (
    CharStringSpan_t *span)
{
    ++span->m_begin;
}

inline bool CharStringSpan_isEmpty (
    const CharStringSpan_t* range)
{
    return (range->m_begin == range->m_end);
}

inline uint8_t CharStringSpan_length (
    const CharStringSpan_t* range)
{
    return (uint8_t)(range->m_end - range->m_begin);
}

inline bool CharStringSpan_equalsP (
    const CharStringSpan_t* leftRange,
    PGM_P rightStr)
{
    const int leftLen = CharStringSpan_length(leftRange);
    return
        (leftLen == strlen_P(rightStr)) &&
        (strncmp_P(leftRange->m_begin, rightStr, leftLen) == 0);
}

// returns > 0 for left greater than right
//         = 0 for equal
//         < 0 for less
inline int CharStringSpan_compareP (
    const CharStringSpan_t* leftRange,
    PGM_P rightStr)
{
    const int leftLen = CharStringSpan_length(leftRange);
    const int rightLen = strlen_P(rightStr);
    if (leftLen == rightLen) {
        return strncmp_P(leftRange->m_begin, rightStr, leftLen);
    } else {
        const int minLen =
            (leftLen < rightLen)
            ? leftLen
            : rightLen;
        int comp = strncmp_P(leftRange->m_begin, rightStr, minLen);
        if (comp == 0) {
            return
                (leftLen < rightLen)
                ? -1
                : 1;
        } else {
            return comp;
        }
    }
}

inline bool CharStringSpan_startsWithP (
    const CharStringSpan_t* range,
    PGM_P prefix)
{
    const int prefixLen = strlen_P(prefix);
    return
        (CharStringSpan_length(range) >= prefixLen) &&
        (strncmp_P(range->m_begin, prefix, prefixLen) == 0);
}

#endif  // CHARSTRINGSPAN_H