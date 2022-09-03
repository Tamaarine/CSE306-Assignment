CC = gcc
CFLAGS += -g -O2 -Werror -Wall
# INC := -I include
BIN := test_syscall # The executable to create in the end

# Retrieve all of the .c files
SRC_FILES = $(wildcard *.c)

# Turn all the .c files into .o files
OBJ_FILES = $(SRC_FILES:.c=.o)

# Default goal to execute
# which requires the binary to be made
all: $(BIN)

# The target is the binary test_syscall
# with the prerequisite of the necessary .o files
$(BIN):$(OBJ_FILES)
	$(CC) $^ -o $@
	
# Rules on how to convert .c into .o object files
%.o:%.c 
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -f $(OBJ_FILES) $(BIN)

.PHONY: all clean
