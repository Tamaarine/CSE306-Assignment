## [M1]
#  Explaint the following here
#  MAKEFILE_LIST is a special variable that contain the name of each
#  Makefile that is parsed by make, in the order it is parsed. If you read
#  the last word of the MAKEFILE_LIST variable it will tell you the current
#  Makefile that it is processing.
#
#  The function lastword is used return the last value in a series of names
#  separated by white spaces. So $(lastword foo bar hello) -> will return hello
#
#  The function realpath just returns the absolute path of the given file.
#  It will not contain any . or .. or //
#
#  Finally dirname is the command that's invoked with the shell function
#  the shell function will evaluate the command and return you the output.
#  dirname takes in a file path, and remove the trailing forward slash.
#  if the file is in current directory it just returns '.'.
#
#  So basically this line of code is using the current Makefile named "Makefile"
#  finding the absolute path of it using realpath, then find out the current directory
#  by doing dirname <my_cwd>/Makefile -> will return <my_cwd>.
#  We can't just do `dirname Makefile` because it will return '.' not the path of CWD
#  ...
CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

## [M2]
#  Explaint the following here
#  This is setting up the compiler. Variable CC stores the
#  compiler that we will be using to compile our c file, which will be gnu c compiler
#  CFLAGS tells us the flags that are passed to the compiler, '-g -O2 -Werror -Wall'
#  -g for compiling the file with debugging symbols. Will allow us to use gdb for debugging
#  -O2 for level 2 of code optimization
#  -Werror will make all warnings into errors. Unused variable warning? Error! Not compiled
#  -Wall will warn you about lots of warnings, unused variable? Warning!
#  -Werror and -Wall goes well with each other because one provides the warning
#  the other will make the warning become error, make you a better programmer!
#  Lastly LDFLAGS is suppose to be flags provided for linkers, but is empty for now
#  ...
CC = gcc
CFLAGS += -g -O2 -Werror -Wall
LDFLAGS +=



## [M3] -- REMOVED
#  ...

## [M5]
#  Explaint the following here
#  The function wildcard is doing wildcard expansion, basically
#  given the pattern which is *.c it will find all the files in the current
#  directory (where Makefile is presented) that matches the pattern and then
#  store it as a whitespace separated String into the variable SRC_FILES.
#  In our case it will give us 'client.c server.c'
#  ...
SRC_FILES = $(wildcard *.c)

## [M6]
#  Explaint the following here
#  This is doing substitution reference.
#  It just takes in the white space separated .c source files
#  then replace the .c with an empty space, which creates our executable
#  file names that will be used for the prerequisites of the default goal.
#  Ex: 'client.c server.c' -> 'client server', then storing it into the variable
#  EXE_FILES
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
#  even if the file 'all' and 'clean' exists. So it will always
#  execute the target.
#  ...
.PHONY: all clean
