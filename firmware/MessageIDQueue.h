//
//  Queue (uin16_t)
//

#ifndef MessageIDQueue_H
#define MessageIDQueue_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

#define MessageIDQueue_Capacity 8

typedef uint16_t MessageIDQueue_ValueType;

typedef struct {
    uint8_t head;
    uint8_t tail;
    uint8_t length;
    MessageIDQueue_ValueType buffer[MessageIDQueue_Capacity];
} MessageIDQueue_type;

static inline void MessageIDQueue_init (
    MessageIDQueue_type* queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->length = 0;
}

static inline bool MessageIDQueue_isEmpty (
    MessageIDQueue_type* queue)
{
    return (queue->length == 0);
}

static inline bool MessageIDQueue_isFull (
    MessageIDQueue_type* queue)
{
    return (queue->length == MessageIDQueue_Capacity);
}

static inline void MessageIDQueue_insert (
    const MessageIDQueue_ValueType value,
    MessageIDQueue_type* queue)
{
    queue->buffer[queue->tail++] = value;
    ++queue->length;
    if (queue->tail == MessageIDQueue_Capacity) {
        // wrap around
        queue->tail = 0;
    }
}

static inline MessageIDQueue_ValueType MessageIDQueue_remove (
    MessageIDQueue_type* queue)
{
    const MessageIDQueue_ValueType value =
        queue->buffer[queue->head++];
    --queue->length;
    if (queue->head == MessageIDQueue_Capacity) {
        // wrap around
        queue->head = 0;
    }

    return value;
}

#endif  // MessageIDQueue_H
