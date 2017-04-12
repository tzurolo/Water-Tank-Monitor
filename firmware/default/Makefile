###############################################################################
# Makefile for the project WaterLevelSensor
###############################################################################

## General Flags
PROJECT = WaterLevelSensor
MCU = atmega328p
F_CPU = 8000000
TARGET = WaterLevelSensor.elf
CC = avr-gcc.exe

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -DF_CPU=$(F_CPU)UL
CFLAGS += -Wall -gstabs  -O3 -fsigned-char -fshort-enums -std=gnu99
CFLAGS += -Wa,-adhlns=$(<:.c=.lst)
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS += -Wl,-Map,WaterLevelSensor.map


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Include Directories
INCLUDES = -I"C:\WinAVR-20100110\avr\include" -I"C:\WinAVR-20100110\avr\bin" -I".." 

## Objects that must be built in order to link
OBJECTS = WaterLevelSensorMain.o SystemTime.o ADCManager.o DataHistory.o \
        CommandProcessor.o EEPROMStorage.o Console.o BatteryMonitor.o \
        CellularComm_SIM800.o CellularTCPIP_SIM800.o TCPIPConsole.o SIM800.o \
        ByteQueue.o SoftwareSerialTx.o SoftwareSerialRx.o CharString.o StringUtils.o \
        UART_async.o EEPROM_Util.o \
        RamSentinel.o

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) WaterLevelSensor.hex WaterLevelSensor.eep size

## Compile
WaterLevelSensorMain.o: ../WaterLevelSensorMain.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

SystemTime.o: ../SystemTime.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

ADCManager.o: ../ADCManager.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

DataHistory.o: ../DataHistory.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

ByteQueue.o: ../ByteQueue.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

SoftwareSerialTx.o: ../SoftwareSerialTx.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

SoftwareSerialRx.o: ../SoftwareSerialRx.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

EEPROM_Util.o: ../EEPROM_Util.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

CharString.o: ../CharString.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

StringUtils.o: ../StringUtils.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

BatteryMonitor.o: ../BatteryMonitor.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

CellularComm_SIM800.o: ../CellularComm_SIM800.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

CellularTCPIP_SIM800.o: ../CellularTCPIP_SIM800.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

TCPIPConsole.o: ../TCPIPConsole.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

SIM800.o: ../SIM800.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

CommandProcessor.o: ../CommandProcessor.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

UART_async.o: ../UART_async.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

EEPROMStorage.o: ../EEPROMStorage.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

Console.o: ../Console.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

RamSentinel.o: ../RamSentinel.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

size: ${TARGET}
	@echo
	@avr-size -C --mcu=${MCU} ${TARGET}

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) WaterLevelSensor.elf dep/* WaterLevelSensor.hex WaterLevelSensor.eep

## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

