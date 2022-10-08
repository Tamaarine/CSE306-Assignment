CC = gcc
CFLAGS += -g -O2 -Werror -Wall
# INC := -I include
BIN := s2dsm # The executable to create in the end

# Retrieve all of the .c files
SRC_FILES = $(wildcard *.c)

# Turn all the .c files into .o files
OBJ_FILES = $(SRC_FILES:.c=.o)

part1: s2dsm_P1b.o
	$(CC) s2dsm_P1b.o -o $(BIN)

part2: s2dsm_P1c.o
	$(CC) s2dsm_P1c.o -o $(BIN)

# Rules on how to convert .c into .o object files
%.o:%.c 
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -f $(OBJ_FILES) $(BIN)

.PHONY: all clean
