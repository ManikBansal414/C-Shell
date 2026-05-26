CC=gcc
CFLAGS=-std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm


SRC_DIR=src
INC_DIR=include
OBJS=$(SRC_DIR)/main.o $(SRC_DIR)/prompt.o $(SRC_DIR)/parser.o $(SRC_DIR)/hop.o $(SRC_DIR)/reveal.o $(SRC_DIR)/log.o \
	$(SRC_DIR)/command_execution.o $(SRC_DIR)/io_redirection.o $(SRC_DIR)/pipeline.o $(SRC_DIR)/seq_exec.o \
	$(SRC_DIR)/background_job.o $(SRC_DIR)/activities.o $(SRC_DIR)/ping.o

.PHONY: all clean


all: shell.out

shell.out: $(OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $^

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/%.h
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o shell.out
