## [M1]
#  Explaint the following here
#  ...
CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

## [M2]
#  Explaint the following here
#  ...
CC = gcc
CFLAGS += -g -O2 -Werror -Wall
LDFLAGS +=



## [M3] -- REMOVED
#  ...

## [M5]
#  Explaint the following here
#  ...
SRC_FILES = $(wildcard *.c)

## [M6]
#  Explaint the following here
#  This is doing substitution reference.
#  it just takes in the white space separated .c source files
#  then replace the .c with an empty space, which creates our executable
#  file names that will be used for the prerequisites of the default goal.
#  Ex: 'client.c server.c' -> 'client server'
#  ...
EXE_FILES = $(SRC_FILES:.c=)

## [M7]
#  Explaint the following here
#  This is the default goal of this makefile. The rule that gets executed
#  when you just run 'make' in the terminal. 
#  It is responsible for compiling this two program, the client and the server.
#  It has the prerequisites of the executable files, so in order to complete this goal
#  it needs the executable files to be made, which will be created by the implicit rule in [M8]
#  ...
all: $(EXE_FILES)

## [M8] -- UPDATED
#  Explaint the following here
#  An implicit rule in make.
#  It will convert all .c files into an executable with the gcc compiler,
#  with the "-g -O2 -Werror -Wall" flags for the compiler. 
#  No linker flags apparently. The executable name for each .c file will
#  just be its name without the extension signify by the -o and $@ automatic variable.
#  The input files for the compiler will just be the files with its .c extension
#  indicated by the $< automatic variable which is the first pre-requisites of the rule.
#  ...
%:%.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< 

## [M9]
#  Explaint the following here
#  This target will basically remove all of the executables 
#  in the current directory 
#  which is namely just $(EXE_FILES) or client and server
#  -f is used to don't complain even if the files don't exist
#  and don't ask for permission, just delete it!
#  ...
clean:
	rm -f $(EXE_FILES)

## [M10]
#  Explaint the following here
#  It will make the target all and clean always out of date
#  even if the file 'all' and 'clean' exists
#  ...
.PHONY: all clean
