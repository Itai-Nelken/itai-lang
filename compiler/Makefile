CC := gcc
CFLAGS = -Wall -Wextra -Wpedantic -Iinclude -g
BIN ?= ilc # [I]tai [L]ang [C]ompiler

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: clean all

