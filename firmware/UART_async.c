//
// UART interface
//
//  Platform:
//     AtMega328P
//
//  Pin usage:
//      PD0 - Rx
//      PD1 - Tx
//
//  How it works:
//     Two static queues are used, one for transmit and one for receive.
//


#include "UART_async.h"

#include "ByteQueue.h"
#include <avr/io.h>
#include <string.h>

ByteQueue_define(8, tx_queue, static);
ByteQueue_define(16, rx_queue, static);

// called to start or continue transmitting
static void transmit_next_byte (void)
{
    if (!ByteQueue_is_empty(&tx_queue)) {
        const char tx_byte = ByteQueue_pop(&tx_queue);
        UDR0 = tx_byte;
    } else {
        // no more data - turn off interrupt
        UCSR0B &= ~(1<<UDRIE0);
    }
}

// transmits the first byte if the transmitter is idle
static void start_transmitting (void)
{
    // start interrupts
    UCSR0B |= (1<<UDRIE0);
}

void UART_init (
    const bool pullupRx)
{
    // initialize transmit and receive queues
    ByteQueue_clear(&tx_queue);
    ByteQueue_clear(&rx_queue);

    if (pullupRx) {
        PORTD |= (1 << PD0);
    }

    // set default baud rate
    UART_set_baud_rate(9600);

    // enable receiver and transmitter, and interrupts
    UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1<<RXCIE0);
    // set frame format to 8-N-1
    UCSR0C = (3<<UCSZ00);
}

void UART_set_baud_rate (
   const uint16_t new_baud_rate)
   {
   uint16_t ubrr0 = ((F_CPU >> 4) / new_baud_rate) - 1;
   UBRR0L = (uint8_t)ubrr0;
   UBRR0H = (uint8_t)((ubrr0 >> 8) & 0x0F);
   }

bool UART_read_byte (
   char *byte)
   {
       bool gotByte = false;

       if (!ByteQueue_is_empty(&rx_queue)) {
            *byte = ByteQueue_pop(&rx_queue);
            gotByte = true;
       }

       return gotByte;
   }

bool UART_write_byte (
   const char byte)
{
    bool successful = false;

    if (ByteQueue_push(byte, &tx_queue)) {
        successful = true;
        start_transmitting();
    }

    return successful;
}

bool UART_tx_queue_is_empty ()
   {
   return ByteQueue_is_empty(&tx_queue);
   }

bool UART_write_stringCS (
    const CharString_t *string)
{
    bool successful = false;

    const uint8_t stringLength = CharString_length(string);
    // check if there is enough space left in the tx queue
    if (stringLength <= ByteQueue_spaceRemaining(&tx_queue)) {
        // there is enough space in the queue
        // push all bytes onto the queue
        for (int offset = 0; offset < stringLength; ++offset) {
            ByteQueue_push(CharString_at(string, offset), &tx_queue);
        }

        start_transmitting();
        successful = true;
    }

    return successful;
}

bool UART_write_string (
   const char *string)
{
    bool successful = false;

    // check if there is enough space left in the tx queue
    if (strlen(string) <= ByteQueue_spaceRemaining(&tx_queue))
    {  // there is enough space in the queue
        // push all bytes onto the queue
        const char *byte_ptr = string;
        while ((*byte_ptr) != 0) {
            ByteQueue_push(*byte_ptr++, &tx_queue);
        }

        start_transmitting();
        successful = true;
    }

    return successful;
}

bool UART_write_stringP (
   PGM_P string)
{
    bool successful = false;

    // check if there is enough space left in the tx queue
    if (strlen_P(string) <= ByteQueue_spaceRemaining(&tx_queue))
        {  // there is enough space in the queue
        // push all bytes onto the queue
        PGM_P cp = string;
        char ch = 0;
        do {
            ch = pgm_read_byte(cp);
            ++cp;
            if (ch != 0) {
                ByteQueue_push(ch, &tx_queue);
            }
        } while (ch != 0);

        start_transmitting();
        successful = true;
        }

   return successful;
}

bool UART_write_bytes (
   const char *bytes,
   const uint16_t numBytes)
{
    bool successful = false;

    // check if there is enough space left in the tx queue
    if (numBytes <= ByteQueue_spaceRemaining(&tx_queue)) {
        // there is enough space in the queue
        if (numBytes > 0) {
            // push all bytes onto the queue
            const char *byte_ptr = bytes;
            uint16_t numBytesRemaining = numBytes;
            while (numBytesRemaining-- > 0) {
                ByteQueue_push(*byte_ptr++, &tx_queue);
            }
            start_transmitting();
        }
        successful = true;
    }

    return successful;
}

bool UART_read_string (
   CharString_t *str_buf)
   {
   bool string_is_complete = false;

   char str_byte;
   if (UART_read_byte(&str_byte))
      {
      if (str_byte == 0x0A)
         string_is_complete = true;
      else
          CharString_appendC(str_byte, str_buf);
      }

   return string_is_complete;
   }

#if BYTEQUEUE_HIGHWATERMARK_ENABLED
void UART_reportHighwater (void)
{
    ByteQueue_reportHighwater(PSTR("URX"), &rx_queue);
    ByteQueue_reportHighwater(PSTR("UTX"), &tx_queue);
}
#endif

ISR(USART_RX_vect, ISR_BLOCK)
{
    ByteQueue_push(UDR0, &rx_queue);
}

ISR(USART_UDRE_vect, ISR_BLOCK)
{
    transmit_next_byte();
}

ISR(USART_TX_vect, ISR_BLOCK)
{
}

