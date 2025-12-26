# Hospital Resource Scheduler - Makefile
# Build for Linux (Ubuntu). Requires gcc, pthreads, librt.

CC := gcc
CFLAGS := -Wall -Wextra -O2 -std=c11
LDFLAGS := -lpthread -lrt
UI_LDFLAGS := $(LDFLAGS) -lncurses

INCLUDE_DIR := include
SRC_DIR := src
BIN_DIR := bin
LOG_DIR := logs
DATA_DIR := data

SRCS := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/patient.c \
	$(SRC_DIR)/scheduler.c \
	$(SRC_DIR)/resources.c \
	$(SRC_DIR)/thread_worker.c \
	$(SRC_DIR)/ipc.c \
	$(SRC_DIR)/logger.c

UI_SRCS := \
	$(SRC_DIR)/ui.c \
	$(SRC_DIR)/patient.c \
	$(SRC_DIR)/scheduler.c \
	$(SRC_DIR)/resources.c \
	$(SRC_DIR)/thread_worker.c \
	$(SRC_DIR)/ipc.c

OBJS := $(SRCS:.c=.o)

APP := $(BIN_DIR)/hospital_scheduler
LOGGER := $(BIN_DIR)/logger
UI_APP := $(BIN_DIR)/hospital_ui

.PHONY: all clean run run-ui

all: $(BIN_DIR) $(LOG_DIR) $(DATA_DIR) $(APP) $(LOGGER) $(UI_APP)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(LOG_DIR):
	mkdir -p $(LOG_DIR)

$(DATA_DIR):
	mkdir -p $(DATA_DIR)

$(APP): $(SRC_DIR)/main.o $(SRC_DIR)/patient.o $(SRC_DIR)/scheduler.o $(SRC_DIR)/resources.o $(SRC_DIR)/thread_worker.o $(SRC_DIR)/ipc.o
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $^ -o $@ $(LDFLAGS)

$(LOGGER):
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $(SRC_DIR)/logger.c $(SRC_DIR)/ipc.c -o $@ $(LDFLAGS)


$(UI_APP): $(SRC_DIR)/ui.o $(SRC_DIR)/patient.o $(SRC_DIR)/scheduler.o $(SRC_DIR)/resources.o $(SRC_DIR)/thread_worker.o $(SRC_DIR)/ipc.o $(SRC_DIR)/storage.o
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $^ -o $@ $(UI_LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o
	rm -f $(APP) $(LOGGER)
	rm -f $(UI_APP)

run: all
	$(APP) --alg fcfs --patients 10 --doctors 3 --machines 2 --rooms 4 --quantum 3

run-ui: $(BIN_DIR) $(LOG_DIR) $(LOGGER) $(UI_APP)
	$(UI_APP) || true
