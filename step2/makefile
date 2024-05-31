CC = gcc
CFLAGS = -g -I$(INC_DIR)
DEBUGFLAGS = -D _DEBUG
DEBUG_FILE = .debug

TARGETS = BDS FS FC

SRC_DIR = src
INC_DIR = include

.PHONY: all debug clean

all: CFLAGS += -O3 -Wall -Wextra
all: check_mode $(TARGETS)

debug: CFLAGS += $(DEBUGFLAGS)
debug: check_mode $(TARGETS)

BDS: BDS.c $(SRC_DIR)/BasicDisk.c $(SRC_DIR)/ServerCore.c
	$(CC) $(CFLAGS) -o $@ $^

FS: FS.c $(SRC_DIR)/*.c
	$(CC) $(CFLAGS) -o $@ $^

FC: FC.c $(SRC_DIR)/*.c
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