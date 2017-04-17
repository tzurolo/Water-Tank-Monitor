//
//  Console for USB host
//
//
#include "Console.h"

#include "UART_async.h"
#include "SystemTime.h"
#include "CommandProcessor.h"
#include "StringUtils.h"
#include <avr/io.h>
#include <avr/pgmspace.h>

#define SINGLE_SCREEN 0

#define ANSI_ESCAPE_SEQUENCE(EscapeSeq)  "\33[" EscapeSeq
#define ESC_CURSOR_POS(Line, Column)    ANSI_ESCAPE_SEQUENCE(#Line ";" #Column "H")
#define ESC_ERASE_LINE                  ANSI_ESCAPE_SEQUENCE("K")
#define ESC_CURSOR_POS_RESTORE          ANSI_ESCAPE_SEQUENCE("u")

const prog_char crlfP[] = {13,10,0};

// state variables
CharString_define(40, commandBuffer)
static SystemTime_t nextStatusPrintTime;
static uint8_t currentPrintLine = 5;

static bool consoleIsConnected ()
{
    return true;
}

void Console_Initialize (void)
{
    UART_init();
}

void Console_task (void)
{
    char cmdByte;
    if (UART_read_byte(&cmdByte)) {
        switch (cmdByte) {
            case '\r' : {
                // command complete. execute it
                CommandProcessor_processCommand(CharString_cstr(&commandBuffer), "", "");

#if SINGLE_SCREEN
                USBTerminal_sendCharsToHost(ESC_CURSOR_POS(2, 1));
#endif
                UART_write_stringCS(&commandBuffer);
                UART_write_string(ESC_ERASE_LINE);
                CharString_clear(&commandBuffer);
                }
                break;
            case 0x7f : {
                CharString_truncate(CharString_length(&commandBuffer) - 1,
                    &commandBuffer);
                }
                break;
            default : {
                // command not complete yet. append to command buffer
                CharString_appendC(cmdByte, &commandBuffer);
                }
                break;
        }
		// echo current command
#if SINGLE_SCREEN
        USBTerminal_sendCharsToHost(ESC_CURSOR_POS(1, 1));
#endif
        UART_write_stringCS(&commandBuffer);
#if SINGLE_SCREEN
        USBTerminal_sendCharsToHost(ESC_ERASE_LINE);
#endif
    }

    // display status
    if (consoleIsConnected() &&
        SystemTime_timeHasArrived(&nextStatusPrintTime)) {
#if SINGLE_SCREEN
        USBTerminal_sendCharsToHost(ESC_CURSOR_POS(3, 1));
#endif
        CharString_define(120, statusMsg)
        CommandProcessor_createStatusMessage(&statusMsg);
        UART_write_stringCS(&statusMsg);
        UART_write_stringP(crlfP);

	// schedule next display
	SystemTime_futureTime(100, &nextStatusPrintTime);
	}
}

static void sendCursorTo (
    const int line,
    const int column)
{
    CharString_define(16, cursorToBuf);
    CharString_copyP(PSTR("\33["), &cursorToBuf);
    StringUtils_appendDecimal(line, 1, 0, &cursorToBuf);
    CharString_appendC(';', &cursorToBuf);
    StringUtils_appendDecimal(column, 1, 0, &cursorToBuf);
    CharString_appendC('H', &cursorToBuf);
    UART_write_stringCS(&cursorToBuf);
}

void Console_print (
    const char* text)
{
    if (consoleIsConnected()) {
#if SINGLE_SCREEN
#if 0
        if (currentPrintLine > 22) {
            currentPrintLine = 5;
            UART_write_string(ESC_CURSOR_POS(5, 1));
            UART_write_string(ANSI_ESCAPE_SEQUENCE("J"));
        }
#endif
        // print text on the current line
	sendCursorTo(currentPrintLine, 1);
#endif
        UART_write_string(text);
        UART_write_stringP(crlfP);
#if SINGLE_SCREEN
        UART_write_string(ESC_CURSOR_POS_RESTORE);
        ++currentPrintLine;

        // restore cursor to command buffer end
        sendCursorTo(1, CharString_length(&commandBuffer)+1);
#endif
    }
}

void Console_printP (
    PGM_P text)
{
    if (consoleIsConnected()) {
#if SINGLE_SCREEN
#if 0
        if (currentPrintLine > 22) {
            currentPrintLine = 5;
            UART_write_string(ESC_CURSOR_POS(5, 1));
            UART_write_string(ANSI_ESCAPE_SEQUENCE("J"));
        }
#endif

	// print text on the current line
	sendCursorTo(currentPrintLine, 1);
#endif
        UART_write_stringP(text);
        UART_write_stringP(crlfP);
#if SINGLE_SCREEN
        UART_write_string(ESC_CURSOR_POS_RESTORE);
        ++currentPrintLine;

        // restore cursor to command buffer end
        sendCursorTo(1, CharString_length(&commandBuffer)+1);
#endif
    }
}

void Console_printCS (
    const CharString_t *text)
{
    if (consoleIsConnected()) {
#if SINGLE_SCREEN
#if 0
        if (currentPrintLine > 22) {
	        currentPrintLine = 5;
	        UART_write_string(ESC_CURSOR_POS(5, 1));
	        UART_write_string(ANSI_ESCAPE_SEQUENCE("J"));
        }
#endif
        // print text on the current line
        sendCursorTo(currentPrintLine, 1);
#endif
        UART_write_stringCS(text);
        UART_write_stringP(crlfP);
#if SINGLE_SCREEN
        UART_write_string(ESC_CURSOR_POS_RESTORE);
        ++currentPrintLine;

        // restore cursor to command buffer end
        sendCursorTo(1, CharString_length(&commandBuffer)+1);
#endif
    }
}
