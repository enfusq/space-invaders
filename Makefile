# Toolchain
# (MCU flags appended)
CC=avr-gcc -mmcu=atmega328p
OBJCOPY=avr-objcopy

# Flags
CFLAGS=-g -Os -DF_CPU=$(FREQ)UL -Wall

# Configuration
# Clock speed
FREQ=8000000
# Serial port (used for programming)
PORT?=/dev/ttyUSB0

# Files that make up the program
OBJECTS=space_invaders.o
# Program name
PROGRAM=space_invaders


# Build rule
$(PROGRAM).elf: $(OBJECTS)
	$(CC) -o $(PROGRAM).elf $^

# Compile rules
# C
%.o: %.c Makefile
	$(CC) $(CFLAGS) -c -o $@ $<


# Flashing rules
# 1. Generate .hex
%.hex: %.elf
	$(OBJCOPY) -R .eeprom -R .fuse -R .lock -R .signature -O ihex $< $@

# 2. Actual flashing
flash: $(PROGRAM).hex
	avrdude -p m328p -P $(PORT) -c arduino -b 57600 -U flash:w:$<
