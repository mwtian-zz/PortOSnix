# Makefile for minithreads on x86_64 Linux

# You probably only need to modify the MAIN variable to build the desired file

CC = gcc

CFLAGS = -mno-red-zone -fno-omit-frame-pointer -ggdb -O0 -I. -Wdeclaration-after-statement
LFLAGS = -lrt -lpthread

OUTPUT = minithread

# change this to the name of the file you want to link with minithreads,
# dropping the ".c": so to use "sieve.c", change to "MAIN = sieve".
#
# Note that you must write your own test programs.

MAIN = network3_mod

OBJ = \
    alarm.o \
    alarm_queue.o \
    interrupts.o \
    machineprimitives_x86_64_asm.o \
    machineprimitives_x86_64.o \
    machineprimitives.o \
    $(MAIN).o \
    miniheader.o \
    minimsg.o \
    minithread.o \
    multilevel_queue.o \
    network.o \
    queue.o \
    random.o \
    synch.o \

all: minithread

.c.obj:
	$(CC) $(CFLAGS) $<

machineprimitives_x86_64_asm.o: machineprimitives_x86_64_asm.S
	$(CC) -c machineprimitives_x86_64_asm.S -o machineprimitives_x86_64_asm.o

minithread: start.o end.o $(OBJ) $(SYSTEMOBJ)
	$(CC) $(LFLAGS) $(LIB) start.o $(OBJ) end.o $(LFLAGS) -o $(OUTPUT)

clean:
	rm -f *.o
	rm -f $(OUTPUT)

depend:
	gcc -MM *.c 2>/dev/null > Depend

include Depend
