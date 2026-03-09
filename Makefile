# Makefile for CVX shell

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lm -s

SRC = src/main.c src/config.c src/commands.c src/prompt.c src/exec.c src/signals.c src/linenoise.c src/parser.c src/ast.c src/lexer.c src/utils.c src/jobs.c src/functions.c
OBJ_DIR = obj
OBJ = $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRC))
OUT = cvx

.PHONY: all clean install uninstall

all: $(OUT)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $(OUT) $(LDFLAGS)
	rm -rf $(OBJ_DIR)

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -rf $(OBJ_DIR) $(OUT)

install: all
	sudo cp $(OUT) /usr/local/bin/$(OUT)
	sudo rm -rf obj
	sudo chmod +x /usr/local/bin/$(OUT)
	sudo sh -c 'if [ ! -f /etc/cvx.conf ]; then \
		echo "PROMPT=default" > /etc/cvx.conf; \
		echo "HISTORY=true" >> /etc/cvx.conf; \
	fi'


uninstall:
	sudo rm -f /usr/local/bin/$(OUT) /etc/cvx.conf
