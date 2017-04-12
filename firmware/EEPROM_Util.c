//
// EEPROM access
//

#include "EEPROM_Util.h"

#include <avr/io.h>

void EEPROM_write (
    uint8_t* uiAddress,
    const uint8_t ucData)
{
    /* Wait for completion of previous write */
    while(EECR & (1<<EEPE))
        ;
    /* Set up address and Data Registers */
    EEAR = (uint16_t)uiAddress;
    EEDR = ucData;
    /* Write logical one to EEMPE */
    EECR |= (1<<EEMPE);
    /* Start eeprom write by setting EEPE */
    EECR |= (1<<EEPE);
}

uint8_t EEPROM_read (
    const uint8_t* uiAddress)
{
    /* Wait for completion of previous write */
    while (EECR & (1<<EEPE))
        ;
    /* Set up address register */
    EEAR = (uint16_t)uiAddress;
    /* Start eeprom read by writing EERE */
    EECR |= (1<<EERE);
    /* Return data from Data Register */
    return EEDR;
}

void EEPROM_writeString (
    char* uiAddress,
    const int maxLength,
    const char* string)
{
    int lengthWritten = 0;
    char strChar;
    uint8_t* charAddr = (uint8_t*)uiAddress;
    const char* cp = string;
    do {
        strChar = *cp++;
        EEPROM_write(charAddr++, strChar);
        ++lengthWritten;
    } while ((lengthWritten < maxLength) && (strChar != 0));
}

void EEPROM_readString (
    const char* uiAddress,
    const int maxLength,
    char* string)
{
    int lengthRead = 0;
    char strChar;
    uint8_t* charAddr = (uint8_t*)uiAddress;
    char* cp = string;
    do {
        strChar = EEPROM_read(charAddr++);
        *cp++ = strChar;
        ++lengthRead;
    } while ((lengthRead < maxLength) && (strChar != 0));
    *cp = 0;
}

void EEPROM_writeWord (
    uint16_t* uiAddress,
    const uint16_t word)
{
    uint8_t* byteAddr = (uint8_t*)uiAddress;
    EEPROM_write(byteAddr, word & 0xFF);
    EEPROM_write(byteAddr + 1, (word >> 8) & 0xFF);
}

uint16_t EEPROM_readWord (
    const uint16_t* uiAddress)
{
    uint8_t* byteAddr = (uint8_t*)uiAddress;
    uint16_t word = 0;
    word = EEPROM_read(byteAddr);
    word |= (EEPROM_read(byteAddr + 1) << 8);

    return word;
}

