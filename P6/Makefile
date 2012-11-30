# Makefile for minithreads on x86_64 Linux

# You probably only need to modify the MAIN variable to build the desired file

CC = gcc

CFLAGS = -Wall -Wfatal-errors -mno-red-zone -fno-omit-frame-pointer -ggdb -O0 \
    -I. -Wdeclaration-after-statement
LFLAGS = -lrt -lpthread

# change MAIN to the name of the file you want to link with minithreads,
# dropping the ".c": so to use "sieve.c", change to "MAIN = sieve".
#
# Note that you must write your own test programs.
MAIN =  shell
OUTPUT_MAIN = minithread

# Extra ultility: mkfs
TARGET_MKFS = mkfs
OUTPUT_MKFS = mkfs

OBJ = \
    alarm.o \
    alarm_queue.o \
    disk.o \
    miniroute_cache.o \
    interrupts.o \
    machineprimitives_x86_64_asm.o \
    machineprimitives_x86_64.o \
    machineprimitives.o \
    minifile.o \
    minifile_cache.o \
    minifile_diskutil.o \
    miniheader.o \
    minimsg.o \
    minisocket.o \
    miniroute.o \
    minithread.o \
    multilevel_queue.o \
    network.o \
    queue.o \
    queue_locked.o \
    queue_wrap.o \
    random.o \
    read.o \
    synch.o \

.PHONY: all
.DEFAULT: all
all: minithread mkfs

machineprimitives_x86_64_asm.o: machineprimitives_x86_64_asm.S
	$(CC) -c machineprimitives_x86_64_asm.S -o machineprimitives_x86_64_asm.o

minithread: start.o end.o $(OBJ) $(SYSTEMOBJ) $(MAIN).o
	$(CC) $(LFLAGS) $(LIB) start.o $(OBJ) end.o $(MAIN).o $(LFLAGS) -o $(OUTPUT_MAIN)

mkfs: start.o end.o $(OBJ) $(SYSTEMOBJ) $(TARGET_MKFS).o
	$(CC) $(LFLAGS) $(LIB) start.o $(OBJ) end.o $(TARGET_MKFS).o $(LFLAGS) -o $(OUTPUT_MKFS)

clean:
	rm -f *.o
	rm -f $(OUTPUT_MAIN) $(OUTPUT_MKFS)

depend:
	gcc -MM *.c 2>/dev/null > Depend

include Depend
