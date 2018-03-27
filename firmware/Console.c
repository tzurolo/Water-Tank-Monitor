//
//  System Console for debug and local control
//
//  Pin usage:
//      PC2 - serial input
//      PC3 - serial output
//
#include "Console.h"

#include "SoftwareSerialRx0.h"
#include "SoftwareSerialTx.h"
#include "SystemTime.h"
#include "CommandProcessor.h"
#include "StringUtils.h"
#include <avr/io.h>
#include <avr/pgmspace.h>

#define SINGLE_SCREEN 0

#define TX_CHAN_INDEX 1

#define ANSI_ESCAPE_SEQUENCE(EscapeSeq)  "\33[" EscapeSeq
#define ESC_CURSOR_POS(Line, Column)    ANSI_ESCAPE_SEQUENCE(#Line ";" #Column "H")
#define ESC_ERASE_LINE                  ANSI_ESCAPE_SEQUENCE("K")
#define ESC_CURSOR_POS_RESTORE          ANSI_ESCAPE_SEQUENCE("u")

const prog_char crP[] = {13,0};
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
    SoftwareSerialTx_Initialize(TX_CHAN_INDEX, ps_b, 3);
    SoftwareSerialTx_enable(TX_CHAN_INDEX);
    SoftwareSerialTx_sendP(TX_CHAN_INDEX, crlfP);

    SystemTime_futureTime(0, &nextStatusPrintTime); // start right away
}

void Console_task (void)
{
    ByteQueue_t* rxQueue = SoftwareSerial_rx0Queue();
    if (!ByteQueue_is_empty(rxQueue)) {
        char cmdByte = ByteQueue_pop(rxQueue);
        switch (cmdByte) {
            case '\r' : {
                // command complete. execute it
                SoftwareSerialTx_sendP(TX_CHAN_INDEX, crlfP);
                CommandProcessor_processCommand(CharString_cstr(&commandBuffer), "", "");
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
        SoftwareSerialTx_sendP(TX_CHAN_INDEX, crP);
        SoftwareSerialTx_sendCS(TX_CHAN_INDEX, &commandBuffer);
        SoftwareSerialTx_send(TX_CHAN_INDEX, ESC_ERASE_LINE);
    }

    // display status
    if (consoleIsConnected() &&
        SystemTime_timeHasArrived(&nextStatusPrintTime)) {
        CharString_define(120, statusMsg)
        CommandProcessor_createStatusMessage(&statusMsg);
        SoftwareSerialTx_sendP(TX_CHAN_INDEX, crP);
        SoftwareSerialTx_sendCS(TX_CHAN_INDEX, &statusMsg);
        SoftwareSerialTx_sendP(TX_CHAN_INDEX, crlfP);

        if (!CharString_isEmpty(&commandBuffer)) {
            // echo current command
            SoftwareSerialTx_sendCS(TX_CHAN_INDEX, &commandBuffer);
            SoftwareSerialTx_send(TX_CHAN_INDEX, ESC_ERASE_LINE);
        }

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
    SoftwareSerialTx_sendCS(TX_CHAN_INDEX, &cursorToBuf);
}

void Console_print (
    const char* text)
{
    if (consoleIsConnected()) {
#if SINGLE_SCREEN
#if 0
        if (currentPrintLine > 22) {
            currentPrintLine = 5;
            SoftwareSerialTx_send(TX_CHAN_INDEX, ESC_CURSOR_POS(5, 1));
            SoftwareSerialTx_send(TX_CHAN_INDEX, ANSI_ESCAPE_SEQUENCE("J"));
        }
#endif
        // print text on the current line
	sendCursorTo(currentPrintLine, 1);
#endif
        SoftwareSerialTx_send(TX_CHAN_INDEX, text);
        SoftwareSerialTx_sendP(TX_CHAN_INDEX, crlfP);
#if SINGLE_SCREEN
        SoftwareSerialTx_send(TX_CHAN_INDEX, ESC_CURSOR_POS_RESTORE);
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
            SoftwareSerialTx_send(TX_CHAN_INDEX, ESC_CURSOR_POS(5, 1));
            SoftwareSerialTx_send(TX_CHAN_INDEX, ANSI_ESCAPE_SEQUENCE("J"));
        }
#endif

	// print text on the current line
	sendCursorTo(currentPrintLine, 1);
#endif
        SoftwareSerialTx_sendP(TX_CHAN_INDEX, text);
        SoftwareSerialTx_sendP(TX_CHAN_INDEX, crlfP);
#if SINGLE_SCREEN
        SoftwareSerialTx_send(TX_CHAN_INDEX, ESC_CURSOR_POS_RESTORE);
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
	        SoftwareSerialTx_send(TX_CHAN_INDEX, ESC_CURSOR_POS(5, 1));
	        SoftwareSerialTx_send(TX_CHAN_INDEX, ANSI_ESCAPE_SEQUENCE("J"));
        }
#endif
        // print text on the current line
        sendCursorTo(currentPrintLine, 1);
#endif
        SoftwareSerialTx_sendCS(TX_CHAN_INDEX, text);
        SoftwareSerialTx_sendP(TX_CHAN_INDEX, crlfP);
#if SINGLE_SCREEN
        SoftwareSerialTx_send(TX_CHAN_INDEX, ESC_CURSOR_POS_RESTORE);
        ++currentPrintLine;

        // restore cursor to command buffer end
        sendCursorTo(1, CharString_length(&commandBuffer)+1);
#endif
    }
}
