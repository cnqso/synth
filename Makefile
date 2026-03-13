APP_NAME ?= cnqsosynth
VERSION ?= dev
PKG_CONFIG ?= pkg-config
CC ?= cc
EXEEXT :=
ifeq ($(OS),Windows_NT)
EXEEXT := .exe
endif

TARGET = $(APP_NAME)$(EXEEXT)
SRC = main.c draw.c audio.c midi.c ui.c
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl3)
SDL_LIBS := $(shell $(PKG_CONFIG) --libs sdl3)
CPPFLAGS += $(SDL_CFLAGS) -DAPP_NAME=\"$(APP_NAME)\" -DAPP_VERSION=\"$(VERSION)\"
CFLAGS += -O2 -Wall -Wno-unused-function
LDLIBS += $(SDL_LIBS) -lm

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(APP_NAME) $(APP_NAME).exe

.PHONY: all clean
