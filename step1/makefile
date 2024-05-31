CC = gcc
CFLAGS = -Wall -Wextra -g -I$(INC_DIR)
DEBUGFLAGS = -D _DEBUG
DEBUG_FILE = .debug

TARGETS = BDS BDC_random BDC_command

SRC_DIR = src
INC_DIR = include

.PHONY: all debug clean

all: CFLAGS += -O3
all: check_mode $(TARGETS)

debug: CFLAGS += $(DEBUGFLAGS)
debug: check_mode $(TARGETS)

BDS: BDS.c $(SRC_DIR)/*.c
	$(CC) $(CFLAGS) -o $@ $^

BDC_random: BDC_random.c $(SRC_DIR)/ClientCore.c
	$(CC) $(CFLAGS) -o $@ $^

BDC_command: BDC_command.c $(SRC_DIR)/ClientCore.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS) $(DEBUG_FILE)

check_mode:
	@if [ -f $(DEBUG_FILE) ] && [ "$(MAKECMDGOALS)" != "debug" ]; then \
		echo "Switching to release mode."; \
		rm -f $(TARGETS) $(DEBUG_FILE); \
	fi
	@if [ ! -f $(DEBUG_FILE) ] && [ "$(MAKECMDGOALS)" = "debug" ]; then \
		echo "Switching to debug mode."; \
		rm -f $(TARGETS); \
		touch $(DEBUG_FILE); \
	fi