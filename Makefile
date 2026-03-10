CC ?= cc
EXEEXT :=
ifeq ($(OS),Windows_NT)
EXEEXT := .exe
endif
TARGET = cnqsosynth$(EXEEXT)
CFLAGS = $(shell pkg-config --cflags sdl3) -O2 -Wall -Wno-unused-function
LDFLAGS = $(shell pkg-config --libs sdl3) -lm
SRC = main.c draw.c audio.c midi.c ui.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f cnqsosynth cnqsosynth.exe

.PHONY: all clean
