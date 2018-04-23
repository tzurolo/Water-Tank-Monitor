//
//  Character String Range
//

#include "CharStringSpan.h"

void CharStringSpan_init (
    const CharString_t *str,
    CharStringSpan_t *span)
{
    span->m_begin = CharString_begin(str);
    span->m_end   = CharString_end(str);
}

void CharStringSpan_initRight (
    const CharString_t *str,
    const int offset,
    CharStringSpan_t *span)
{
    span->m_begin = CharString_begin(str) + offset;
    span->m_end   = CharString_end(str);
}

char CharStringSpan_front(
    const CharStringSpan_t *span)
{
    return 
        (span->m_begin < span->m_end)
        ? *span->m_begin
        : 0;
}

void CharStringSpan_incrBegin (
    CharStringSpan_t *span)
{
    if (span->m_begin < span->m_end) {
        ++span->m_begin;
    }
}

bool CharStringSpan_equalsP (
    const CharStringSpan_t* leftSpan,
    PGM_P rightStr)
{
    const int leftLen = CharStringSpan_length(leftSpan);
    return
        (leftLen == strlen_P(rightStr)) &&
        (strncmp_P(leftSpan->m_begin, rightStr, leftLen) == 0);
}

bool CharStringSpan_equalsNocaseP (
    const CharStringSpan_t* leftSpan,
    PGM_P rightStr)
{
    const int leftLen = CharStringSpan_length(leftSpan);
    return
        (leftLen == strlen_P(rightStr)) &&
        (strncasecmp_P(leftSpan->m_begin, rightStr, leftLen) == 0);
}

int CharStringSpan_compareP (
    const CharStringSpan_t* leftSpan,
    PGM_P rightStr)
{
    const int leftLen = CharStringSpan_length(leftSpan);
    const int rightLen = strlen_P(rightStr);
    if (leftLen == rightLen) {
        return strncmp_P(leftSpan->m_begin, rightStr, leftLen);
    } else {
        const int minLen =
            (leftLen < rightLen)
            ? leftLen
            : rightLen;
        int comp = strncmp_P(leftSpan->m_begin, rightStr, minLen);
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


